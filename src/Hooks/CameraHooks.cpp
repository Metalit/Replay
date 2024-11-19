#include <GLES3/gl3.h>

#include "Config.hpp"
#include "CustomTypes/CameraRig.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/GraphicSettingsConditionalActivator.hpp"
#include "GlobalNamespace/HeadObstacleLowPassAudioEffect.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/MainCamera.hpp"
#include "GlobalNamespace/MainCameraCullingMask.hpp"
#include "GlobalNamespace/MainEffectController.hpp"
#include "GlobalNamespace/ObstacleMaterialSetter.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/ShockwaveEffect.hpp"
#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/TransformExtensions.hpp"
#include "GlobalNamespace/VRRenderingParamsSetup.hpp"
#include "Hooks.hpp"
#include "Main.hpp"
#include "Replay.hpp"
#include "ReplayManager.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/Matrix4x4.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "Utils.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "hollywood/shared/hollywood.hpp"

using namespace GlobalNamespace;

Hollywood::AudioCapture* audioCapture = nullptr;
UnityEngine::Camera* customCamera = nullptr;
std::fstream videoOutput;

ReplayHelpers::CameraRig* cameraRig = nullptr;
UnityEngine::Camera* mainCamera = nullptr;
int oldCullingMask = 0;
int uiCullingMask = 1 << 5;

static bool wasMoving = false;

static int reinitDistortions = -1;

static auto const TmpVidPath = "/sdcard/replay-tmp.h264";
static auto const TmpAudPath = "/sdcard/replay-tmp.wav";
static auto const TmpOutPath = "/sdcard/replay-tmp-mux.mp4";

static std::string fileName = "";

// set camera positions
MAKE_AUTO_HOOK_MATCH(PlayerTransforms_Update, &PlayerTransforms::Update, void, PlayerTransforms* self) {
    if (Manager::replaying) {
        if (wasMoving && Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson) {
            auto parent = self->_originParentTransform ? self->_originParentTransform : cameraRig->fakeHead->parent;
            // always update rotation but only update position when releasing
            if (!Manager::Camera::moving)
                getConfig().ThirdPerPos.SetValue(parent->InverseTransformPoint(cameraRig->transform->position));
            auto rot = TransformExtensions::InverseTransformRotation(parent, cameraRig->transform->rotation).eulerAngles;
            getConfig().ThirdPerRot.SetValue(rot);
        }
        wasMoving = Manager::Camera::moving;
        if (!Manager::paused && Manager::Camera::GetMode() != (int) CameraMode::Headset) {
            Vector3 targetPos;
            Quaternion targetRot;
            if (Manager::GetCurrentInfo().positionsAreLocal || Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson) {
                auto parent = self->_originParentTransform ? self->_originParentTransform : cameraRig->fakeHead->parent;
                auto rot = parent->rotation;
                targetPos = Sombrero::QuaternionMultiply(rot, Manager::Camera::GetHeadPosition()) + parent->position;
                targetRot = Sombrero::QuaternionMultiply(rot, Manager::Camera::GetHeadRotation());
            } else {
                targetPos = Manager::Camera::GetHeadPosition();
                targetRot = Manager::Camera::GetHeadRotation();
            }
            if (cameraRig)
                cameraRig->SetPositionAndRotation(targetPos, targetRot);
            if (customCamera)
                customCamera->transform->SetPositionAndRotation(targetPos, targetRot);
        }
    }
    PlayerTransforms_Update(self);
}

void Camera_Pause() {
    if (Manager::replaying && Manager::Camera::rendering && mainCamera && oldCullingMask != 0)
        mainCamera->cullingMask = oldCullingMask;
}

void Camera_Unpause() {
    if (Manager::replaying && Manager::Camera::rendering && mainCamera && oldCullingMask != 0)
        mainCamera->cullingMask = uiCullingMask;
}

constexpr UnityEngine::Matrix4x4 MatrixTranslate(UnityEngine::Vector3 const& vector) {
    UnityEngine::Matrix4x4 result;
    result.m00 = 1;
    result.m01 = 0;
    result.m02 = 0;
    result.m03 = vector.x;
    result.m10 = 0;
    result.m11 = 1;
    result.m12 = 0;
    result.m13 = vector.y;
    result.m20 = 0;
    result.m21 = 0;
    result.m22 = 1;
    result.m23 = vector.z;
    result.m30 = 0;
    result.m31 = 0;
    result.m32 = 0;
    result.m33 = 1;
    return result;
}

void SetupRecording() {
    LOG_INFO("Setting up recording");
    fileName = SanitizedPath(GetMapString(Manager::beatmap));
    LOG_INFO("Using filename: {}", fileName);

    LOG_INFO("Muting audio");
    Hollywood::SetMuteSpeakers(true);
    LOG_INFO("Keeping screen on");
    Hollywood::SetScreenOn(true);

    LOG_INFO("Beginning video capture");
    customCamera = UnityEngine::Object::Instantiate(mainCamera);
    customCamera->gameObject->active = false;
    customCamera->enabled = true;

    int kept = 0;
    while (customCamera->transform->childCount > kept) {
        auto child = customCamera->transform->GetChild(kept)->gameObject;
        if (child->name == "AudioListener")
            kept++;
        else
            UnityEngine::Object::DestroyImmediate(child);
    }
    if (auto comp = customCamera->GetComponent<MainCameraCullingMask*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent<MainCamera*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent<UnityEngine::SpatialTracking::TrackedPoseDriver*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent<ReplayHelpers::CameraRig*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent("CameraController"))
        UnityEngine::Object::DestroyImmediate(comp);

    customCamera->clearFlags = mainCamera->clearFlags;
    customCamera->nearClipPlane = mainCamera->nearClipPlane;
    customCamera->farClipPlane = mainCamera->farClipPlane;
    customCamera->backgroundColor = mainCamera->backgroundColor;
    customCamera->hideFlags = mainCamera->hideFlags;
    customCamera->depthTextureMode = mainCamera->depthTextureMode;
    customCamera->cullingMask = mainCamera->cullingMask;
    // Makes the camera render before the main
    customCamera->depth = mainCamera->depth - 1;

    customCamera->stereoTargetEye = UnityEngine::StereoTargetEyeMask::None;

    int width = getConfig().OverrideWidth.GetValue();
    int height = getConfig().OverrideHeight.GetValue();

    if (width <= 0)
        width = resolutions[getConfig().Resolution.GetValue()].first;
    if (height <= 0)
        height = resolutions[getConfig().Resolution.GetValue()].second;

    auto video = customCamera->gameObject->AddComponent<Hollywood::CameraCapture*>();
    videoOutput.open(TmpVidPath, std::ios::out | std::ios::binary);
    video->onOutputUnit = [](uint8_t* data, size_t len) {
        videoOutput.write((char*) data, len);
    };
    video->Init(width, height, getConfig().FPS.GetValue(), getConfig().Bitrate.GetValue() * 1000, getConfig().FOV.GetValue());

    UnityEngine::Time::set_captureDeltaTime(1 / (float) getConfig().FPS.GetValue());

    Hollywood::SetSyncTimes(true);

    LOG_INFO("Beginning audio capture");

    auto audioListener = customCamera->GetComponentInChildren<UnityEngine::AudioListener*>();
    audioCapture = audioListener->gameObject->AddComponent<Hollywood::AudioCapture*>();
    audioCapture->OpenFile(TmpAudPath);

    customCamera->gameObject->active = true;

    // prevent bloom from applying to the main camera
    if (auto comp = mainCamera->GetComponent<MainEffectController*>())
        UnityEngine::Object::DestroyImmediate(comp);
    mainCamera->GetComponentInChildren<UnityEngine::AudioListener*>()->enabled = false;

    // mask everything but ui
    oldCullingMask = mainCamera->cullingMask;
    mainCamera->cullingMask = uiCullingMask;

    reinitDistortions = 0;
}

// apply wall quality without modifying the normal preset
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetCoreMaterial,
    &ObstacleMaterialSetter::SetCoreMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer
) {
    if (Manager::replaying && Manager::Camera::rendering) {
        switch (getConfig().Walls.GetValue()) {
            case 1:
                renderer->sharedMaterial = self->_texturedCoreMaterial;
                break;
            case 2:
                renderer->sharedMaterial = self->_hwCoreMaterial;
                break;
            default:
                renderer->sharedMaterial = self->_lwCoreMaterial;
                break;
        }
    } else
        ObstacleMaterialSetter_SetCoreMaterial(self, renderer);
}
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetFakeGlowMaterial,
    &ObstacleMaterialSetter::SetFakeGlowMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer
) {
    if (Manager::replaying && Manager::Camera::rendering)
        renderer->sharedMaterial = getConfig().Walls.GetValue() == 0 ? self->_fakeGlowLWMaterial : self->_fakeGlowTexturedMaterial;
    else
        ObstacleMaterialSetter_SetFakeGlowMaterial(self, renderer);
}

// override shockwaves too
MAKE_AUTO_HOOK_MATCH(ShockwaveEffect_Start, &ShockwaveEffect::Start, void, ShockwaveEffect* self) {

    ShockwaveEffect_Start(self);

    if (Manager::replaying && Manager::Camera::rendering)
        self->_shockwavePS->main.maxParticles = getConfig().ShockwavesOn.GetValue() ? getConfig().Shockwaves.GetValue() : 0;
}
// I think graphics tweaks is what unnecessarily disables the component in addition to the game object
MAKE_AUTO_HOOK_MATCH(ShockwaveEffect_SpawnShockwave, &ShockwaveEffect::SpawnShockwave, void, ShockwaveEffect* self, UnityEngine::Vector3 pos) {

    if (getConfig().ShockwavesOn.GetValue() && getConfig().Shockwaves.GetValue() > 0) {
        self->gameObject->active = true;
        self->enabled = true;
    }
    ShockwaveEffect_SpawnShockwave(self, pos);
}

MAKE_AUTO_HOOK_MATCH(
    AudioTimeSyncController_StartSong, &AudioTimeSyncController::StartSong, void, AudioTimeSyncController* self, float startTimeOffset
) {
    AudioTimeSyncController_StartSong(self, startTimeOffset);

    if (Manager::replaying) {
        auto showDebris = mainCamera->GetComponent<MainCameraCullingMask*>()->_initData->showDebris;

        int mask = 2147483647;
        // Existing except FirstPerson (6)
        if (Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson)
            mask = 2147483647 & ~(1 << 6);
        // Existing except ThirdPerson (3)
        else
            mask = 2147483647 & ~(1 << 3);

        if (!showDebris)
            mask &= ~(1 << 9);

        mainCamera->cullingMask = mask;
    }

    if (Manager::replaying && Manager::Camera::rendering)
        SetupRecording();
}

// setup some general camera stuff
MAKE_AUTO_HOOK_MATCH(AudioTimeSyncController_Start, &AudioTimeSyncController::Start, void, AudioTimeSyncController* self) {

    AudioTimeSyncController_Start(self);

    if (Manager::replaying) {
        auto levelData = Manager::beatmap.level;
        std::string songName = levelData->songName;
        std::string songAuthor = levelData->songAuthorName;

        // TODO: maybe move elsewhere
        auto player = Manager::GetCurrentInfo().playerName;
        if (player.has_value() && (!Manager::GetCurrentInfo().playerOk || !getConfig().HideText.GetValue())) {
            std::string text = fmt::format("<color=red>REPLAY</color>    {} - {}    Player: {}", songName, songAuthor, player.value());

            auto canvas = BSML::Lite::CreateCanvas();
            canvas->transform->position = {0, getConfig().TextHeight.GetValue(), 7};

            BSML::Lite::CreateText(canvas, text, TMPro::FontStyles::Italic, 7, {}, {200, 10})->alignment = TMPro::TextAlignmentOptions::Center;
        }

        // set culling matrix for moved camera modes and for rendering
        mainCamera = UnityEngine::Camera::get_main();
        static auto set_cullingMatrix =
            il2cpp_utils::resolve_icall<void, UnityEngine::Camera*, UnityEngine::Matrix4x4>("UnityEngine.Camera::set_cullingMatrix_Injected");

        auto forwardMult = UnityEngine::Vector3::op_Multiply(UnityEngine::Vector3::get_forward(), -49999.5);
        auto mat = UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001, 99999);
        mat = UnityEngine::Matrix4x4::op_Multiply(mat, MatrixTranslate(forwardMult));
        mat = UnityEngine::Matrix4x4::op_Multiply(mat, mainCamera->worldToCameraMatrix);
        set_cullingMatrix(mainCamera, mat);

        cameraRig = ReplayHelpers::CameraRig::Create(mainCamera->transform);
    }
}

void FinishMux(bool quit) {
    if (getConfig().CleanFiles.GetValue()) {
        if (fileexists(TmpVidPath))
            std::filesystem::remove(TmpVidPath);
        if (fileexists(TmpAudPath))
            std::filesystem::remove(TmpAudPath);
    }

    LOG_INFO("Removing screen on");
    if (quit || getConfig().LevelsToSelect.GetValue().empty())
        Hollywood::SetScreenOn(false);

    Manager::Camera::muxingFinished = true;
}

void WaitThenMux(bool quit) {
    bool vidExists = fileexists(TmpVidPath);
    bool audExists = fileexists(TmpAudPath);

    if (vidExists && audExists) {
        BSML::MainThreadScheduler::ScheduleAfterTime(5, [quit]() {
            std::string outputFile = fmt::format("{}/{}.mp4", RendersFolder, fileName);
            int num = 1;
            while (fileexists(outputFile))
                outputFile = fmt::format("{}/{}_{}.mp4", RendersFolder, fileName, num++);

            Hollywood::MuxFilesSync(TmpVidPath, TmpAudPath, TmpOutPath, getConfig().FPS.GetValue());

            if (fileexists(TmpOutPath)) {
                // idk how but I got a "no such file or directory" error here once
                int tries = 2;
                while (tries-- > 0) {
                    try {
                        std::filesystem::rename(TmpOutPath, outputFile);
                        break;
                    } catch (std::filesystem::filesystem_error const& err) {
                        LOG_ERROR("filesystem error renaming file {} -> {}: {}", TmpOutPath, outputFile, err.what());
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                }
            }
            FinishMux(quit);
        });
    } else {
        LOG_ERROR("Video exists: {} Audio exists: {}", vidExists, audExists);
        FinishMux(quit);
    }
}

// undo rendering changes when exiting a level
MAKE_AUTO_HOOK_MATCH(
    StandardLevelScenesTransitionSetupDataSO_Finish,
    &StandardLevelScenesTransitionSetupDataSO::Finish,
    void,
    StandardLevelScenesTransitionSetupDataSO* self,
    LevelCompletionResults* levelCompletionResults
) {
    Hollywood::SetSyncTimes(false);

    if (audioCapture)
        UnityEngine::Object::DestroyImmediate(audioCapture);
    audioCapture = nullptr;
    if (customCamera)
        UnityEngine::Object::DestroyImmediate(customCamera->gameObject);
    customCamera = nullptr;
    if (cameraRig)
        UnityEngine::Object::DestroyImmediate(cameraRig);
    cameraRig = nullptr;
    oldCullingMask = 0;
    mainCamera = nullptr;

    UnityEngine::Time::set_captureDeltaTime(0);

    videoOutput.close();

    LOG_INFO("Unmuting audio");
    Hollywood::SetMuteSpeakers(false);

    if (Manager::Camera::rendering) {
        // mux audio and video when done with both
        WaitThenMux(levelCompletionResults->levelEndAction == LevelCompletionResults::LevelEndAction::Quit);
        // make distortions work for the headset camera again
        reinitDistortions = 3;
    }

    return StandardLevelScenesTransitionSetupDataSO_Finish(self, levelCompletionResults);
}

// delay ending of renders (_songTime clamp is removed in shared hooks)
MAKE_AUTO_HOOK_MATCH(AudioTimeSyncController_get_songEndTime, &AudioTimeSyncController::get_songEndTime, float, AudioTimeSyncController* self) {

    float ret = AudioTimeSyncController_get_songEndTime(self);

    if (Manager::replaying && Manager::Camera::rendering)
        return ret + 1;
    return ret;
}

// prevent pauses during recording
MAKE_AUTO_HOOK_MATCH(PauseController_get_canPause, &PauseController::get_canPause, bool, PauseController* self) {

    if (Manager::replaying && Manager::Camera::rendering)
        return getConfig().Pauses.GetValue();

    return PauseController_get_canPause(self);
}

// fix distortions in renders
MAKE_HOOK_NO_CATCH(initialize_blit_fbo, 0x0, void, void* drawQuad, int conversion, int passMode) {

    int& programId = *(int*) drawQuad;
    if (reinitDistortions != -1) {
        glDeleteProgram(programId);
        programId = 0;
        passMode = reinitDistortions;
    }
    reinitDistortions = -1;

    if (programId != 0)
        return;

    initialize_blit_fbo(drawQuad, conversion, passMode);
}

void InstallBlitHook() {
    logger.info("Installing blit init hook...");
    uintptr_t libunity = baseAddr("libunity.so");
    uintptr_t initialize_blit_fbo_addr = findPattern(
        libunity, "fc 6f ba a9 fa 67 01 a9 f8 5f 02 a9 f6 57 03 a9 f4 4f 04 a9 fd 7b 05 a9 ff 83 0b d1 58 d0 3b d5 08 17 40 f9 e8 6f", 0x2000000
    );
    logger.debug("Found blit init address: {}", initialize_blit_fbo_addr);
    INSTALL_HOOK_DIRECT(logger, initialize_blit_fbo, (void*) initialize_blit_fbo_addr);
    logger.info("Installed blit init hook!");
}
