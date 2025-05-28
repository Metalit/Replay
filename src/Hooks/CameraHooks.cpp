#include <GLES3/gl3.h>

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
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "config.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "hooks.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/strings.hpp"
#include "replay.hpp"
#include "utils.hpp"

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

ON_EVENT(MetaCore::Events::MapPaused) {
    if (Manager::replaying && Manager::Camera::rendering && mainCamera && oldCullingMask != 0)
        mainCamera->cullingMask = oldCullingMask;
}

ON_EVENT(MetaCore::Events::MapUnpaused) {
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

// apply wall quality without modifying the normal preset
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetCoreMaterial,
    &ObstacleMaterialSetter::SetCoreMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer,
    BeatSaber::Settings::QualitySettings::ObstacleQuality obstacleQuality,
    bool screenDisplacementEffects
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
        ObstacleMaterialSetter_SetCoreMaterial(self, renderer, obstacleQuality, screenDisplacementEffects);
}
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetFakeGlowMaterial,
    &ObstacleMaterialSetter::SetFakeGlowMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer,
    BeatSaber::Settings::QualitySettings::ObstacleQuality obstacleQuality
) {
    if (Manager::replaying && Manager::Camera::rendering)
        renderer->sharedMaterial = getConfig().Walls.GetValue() == 0 ? self->_fakeGlowLWMaterial : self->_fakeGlowTexturedMaterial;
    else
        ObstacleMaterialSetter_SetFakeGlowMaterial(self, renderer, obstacleQuality);
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

AUTO_HOOK_FUNCTION(blit_fbo) {
    LOG_INFO("Installing blit init hook...");
    uintptr_t libunity = baseAddr("libunity.so");
    uintptr_t initialize_blit_fbo_addr =
        findPattern(libunity, "fd 7b ba a9 fc 6f 01 a9 fa 67 02 a9 f8 5f 03 a9 f6 57 04 a9 f4 4f 05 a9 ff 83 0b d1 58 d0 3b d5 08", 0x2000000);
    LOG_DEBUG("Found blit init address: {}", initialize_blit_fbo_addr);
    INSTALL_HOOK_DIRECT(logger, initialize_blit_fbo, (void*) initialize_blit_fbo_addr);
    LOG_INFO("Installed blit init hook!");
}

void SetupText() {
    auto levelData = MetaCore::Songs::GetSelectedLevel();
    std::string songName = levelData->songName;
    std::string songAuthor = levelData->songAuthorName;

    auto player = Manager::GetCurrentInfo().playerName;
    if (player.has_value() && (!Manager::GetCurrentInfo().playerOk || !getConfig().HideText.GetValue())) {
        std::string text = fmt::format("<color=red>REPLAY</color>    {} - {}    Player: {}", songName, songAuthor, player.value());

        auto canvas = BSML::Lite::CreateCanvas();
        canvas->transform->position = {0, getConfig().TextHeight.GetValue(), 7};

        BSML::Lite::CreateText(canvas, text, TMPro::FontStyles::Italic, 7, {}, {200, 10})->alignment = TMPro::TextAlignmentOptions::Center;
    }
}

void SetupCamera() {
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

void SetupRecording() {
    LOG_INFO("Setting up recording");
    fileName = MetaCore::Strings::SanitizedPath(GetMapString());
    LOG_INFO("Using filename: {}", fileName);

    LOG_INFO("Keeping screen on");
    Hollywood::SetScreenOn(true);

    LOG_INFO("Beginning video capture");
    customCamera = UnityEngine::Object::Instantiate(mainCamera);
    customCamera->tag = "Untagged";
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
    video->Init(width, height, getConfig().FPS.GetValue(), getConfig().Bitrate.GetValue() * 1000, getConfig().FOV.GetValue(), false);

    UnityEngine::Time::set_captureDeltaTime(1 / (float) getConfig().FPS.GetValue());

    Hollywood::SetSyncTimes(true);

    LOG_INFO("Beginning audio capture");

    auto audioListener = customCamera->GetComponentInChildren<UnityEngine::AudioListener*>();
    audioCapture = audioListener->gameObject->AddComponent<Hollywood::AudioCapture*>();
    audioCapture->OpenFile(TmpAudPath);

    // make sure other audio listeners are disabled
    for (auto listener : UnityEngine::Object::FindObjectsOfType<UnityEngine::AudioListener*>())
        listener->enabled = listener == audioListener;

    customCamera->gameObject->active = true;

    // prevent bloom from applying to the main camera
    if (auto comp = mainCamera->GetComponent<MainEffectController*>())
        UnityEngine::Object::DestroyImmediate(comp);

    // mask everything but ui
    oldCullingMask = mainCamera->cullingMask;
    mainCamera->cullingMask = uiCullingMask;

    reinitDistortions = 0;
}

ON_EVENT(MetaCore::Events::MapStarted) {
    if (!Manager::replaying)
        return;

    SetupText();
    SetupCamera();

    if (Manager::Camera::rendering)
        SetupRecording();
}

void FinishMux() {
    if (getConfig().CleanFiles.GetValue()) {
        if (fileexists(TmpVidPath))
            std::filesystem::remove(TmpVidPath);
        if (fileexists(TmpAudPath))
            std::filesystem::remove(TmpAudPath);
    }

    LOG_INFO("Removing screen on");
    if (MetaCore::Internals::mapWasQuit || getConfig().LevelsToSelect.GetValue().empty())
        Hollywood::SetScreenOn(false);

    Manager::Camera::muxingFinished = true;
}

void WaitThenMux() {
    bool vidExists = fileexists(TmpVidPath);
    bool audExists = fileexists(TmpAudPath);

    if (vidExists && audExists) {
        BSML::MainThreadScheduler::ScheduleAfterTime(5, []() {
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
            FinishMux();
        });
    } else {
        LOG_ERROR("Video exists: {} Audio exists: {}", vidExists, audExists);
        FinishMux();
    }
}

void FinishRecording() {
    Hollywood::SetSyncTimes(false);

    if (audioCapture)
        UnityEngine::Object::DestroyImmediate(audioCapture);
    audioCapture = nullptr;
    if (customCamera)
        UnityEngine::Object::DestroyImmediate(customCamera->gameObject);
    customCamera = nullptr;
    if (cameraRig)
        UnityEngine::Object::DestroyImmediate(cameraRig);

    UnityEngine::Time::set_captureDeltaTime(0);

    videoOutput.close();

    if (Manager::Camera::rendering) {
        // mux audio and video when done with both
        WaitThenMux();
        // make distortions work for the headset camera again
        reinitDistortions = 3;
    }
}

ON_EVENT(MetaCore::Events::MapEnded) {
    if (!Manager::replaying)
        return;

    cameraRig = nullptr;
    mainCamera = nullptr;
    oldCullingMask = 0;

    if (Manager::Camera::rendering)
        FinishRecording();
}
