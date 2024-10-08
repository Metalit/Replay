#include "Config.hpp"
#include "CustomTypes/CameraRig.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/HeadObstacleLowPassAudioEffect.hpp"
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
#include "JNIUtils.hpp"
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
#include "hollywood/shared/Hollywood.hpp"

using namespace GlobalNamespace;

Hollywood::AudioCapture* audioCapture = nullptr;
UnityEngine::Camera* customCamera = nullptr;

UnityEngine::AudioSource* whitelistedAudio = nullptr;

ReplayHelpers::CameraRig* cameraRig = nullptr;
UnityEngine::Camera* mainCamera = nullptr;
int oldCullingMask = 0;
int uiCullingMask = 1 << 5;

static bool wasMoving = false;

static auto const TmpVidPath = "/sdcard/replay-tmp.mp4";
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

    auto env = JNIUtils::GetJNIEnv();
    LOG_INFO("Muting audio");
    JNIUtils::SetMute(true, env);
    LOG_INFO("Keeping screen on");
    JNIUtils::SetScreenOn(true, env);

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

    Hollywood::SetSyncTimes(true);

    Hollywood::CameraRecordingSettings settings{
        .width = width,
        .height = height,
        .fps = getConfig().FPS.GetValue(),
        .bitrate = getConfig().Bitrate.GetValue(),
        .fov = getConfig().FOV.GetValue(),
        .filePath = TmpVidPath,
    };
    Hollywood::SetCameraCapture(customCamera, settings)->Init();

    UnityEngine::Time::set_captureDeltaTime(1.0 / settings.fps);

    LOG_INFO("Beginning audio capture");

    auto audioListener = customCamera->GetComponentInChildren<UnityEngine::AudioListener*>();
    audioCapture = Hollywood::SetAudioCapture(audioListener);
    audioCapture->OpenFile(TmpAudPath);

    if (customCamera)
        customCamera->gameObject->active = true;

    // prevent bloom from applying to the main camera
    if (auto comp = mainCamera->GetComponent<MainEffectController*>())
        UnityEngine::Object::DestroyImmediate(comp);
    mainCamera->GetComponentInChildren<UnityEngine::AudioListener*>()->enabled = false;

    // mask everything but ui
    oldCullingMask = mainCamera->cullingMask;
    mainCamera->cullingMask = uiCullingMask;
}

// apply wall quality without modifying the normal preset
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetCoreMaterial,
    &ObstacleMaterialSetter::SetCoreMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer
) {
    if (!Manager::replaying || !Manager::Camera::rendering)
        ObstacleMaterialSetter_SetCoreMaterial(self, renderer);
    else
        renderer->sharedMaterial = getConfig().Walls.GetValue() ? self->_texturedCoreMaterial : self->_lwCoreMaterial;
}
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetFakeGlowMaterial,
    &ObstacleMaterialSetter::SetFakeGlowMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer
) {
    if (!Manager::replaying || !Manager::Camera::rendering)
        ObstacleMaterialSetter_SetFakeGlowMaterial(self, renderer);
    else
        renderer->sharedMaterial = getConfig().Walls.GetValue() ? self->_fakeGlowTexturedMaterial : self->_fakeGlowLWMaterial;
}

// make sure shockwaves don't spawn
MAKE_AUTO_HOOK_MATCH(ShockwaveEffect_SpawnShockwave, &ShockwaveEffect::SpawnShockwave, void, ShockwaveEffect* self, UnityEngine::Vector3 pos) {

    if (!Manager::replaying || !Manager::Camera::rendering)
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

    if (Manager::replaying && Manager::Camera::rendering)
        whitelistedAudio = self->_audioSource;

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

void FinishMux() {
    if (getConfig().CleanFiles.GetValue()) {
        if (fileexists(TmpVidPath))
            std::filesystem::remove(TmpVidPath);
        if (fileexists(TmpAudPath))
            std::filesystem::remove(TmpAudPath);
    }

    LOG_INFO("Removing screen on");
    JNIUtils::SetScreenOn(false);

    Manager::Camera::muxingFinished = true;
}

void WaitThenMux() {
    bool vidExists = fileexists(TmpVidPath);
    bool audExists = fileexists(TmpAudPath);

    if (vidExists && audExists) {
        BSML::MainThreadScheduler::ScheduleAfterTime(5, []() {
            std::string outputFile;
            int num = 0;
            do {
                if (num == 0)
                    outputFile = fmt::format("{}/{}.mp4", RendersFolder, fileName);
                else
                    outputFile = fmt::format("{}/{}_{}.mp4", RendersFolder, fileName, num);
                num++;
            } while (fileexists(outputFile));

            Hollywood::MuxFilesSync(TmpVidPath, TmpAudPath, TmpOutPath);
            if (fileexists(TmpOutPath))
                std::filesystem::rename(TmpOutPath, outputFile);
            FinishMux();
        });
    } else {
        LOG_ERROR("Video exists: {} Audio exists: {}", vidExists, audExists);
        FinishMux();
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
        UnityEngine::Object::DestroyImmediate(customCamera);
    customCamera = nullptr;
    if (cameraRig)
        UnityEngine::Object::DestroyImmediate(cameraRig);
    cameraRig = nullptr;
    oldCullingMask = 0;
    mainCamera = nullptr;
    whitelistedAudio = nullptr;

    UnityEngine::Time::set_captureDeltaTime(0);

    LOG_INFO("Unmuting audio");
    JNIUtils::SetMute(false);

    // mux audio and video when done with both
    if (Manager::Camera::rendering)
        WaitThenMux();

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
