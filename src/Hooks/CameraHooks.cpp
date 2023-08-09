#include "Main.hpp"
#include "Config.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"
#include "CustomTypes/CameraRig.hpp"
#include "JNIUtils.hpp"

#include "hollywood/shared/Hollywood.hpp"

#include "UnityEngine/Camera.hpp"

using namespace GlobalNamespace;

Hollywood::AudioCapture* audioCapture = nullptr;
UnityEngine::Camera* customCamera = nullptr;

ReplayHelpers::CameraRig* cameraRig = nullptr;
UnityEngine::Camera* mainCamera = nullptr;
int oldCullingMask = 0;
int uiCullingMask = 1 << 5;

static bool wasMoving = false;

static const auto TmpVidPath = "/sdcard/replay-tmp.mp4";
static const auto TmpAudPath = "/sdcard/replay-tmp.wav";
static const auto TmpOutPath = "/sdcard/replay-tmp-mux.mp4";

static std::string fileName = "";

#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/TransformExtensions.hpp"

// set camera positions
void Camera_PlayerTransformsUpdate_Pre(PlayerTransforms* self) {
    if(!Manager::replaying)
        return;
    if(wasMoving && Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson) {
        auto parent = self->originParentTransform ? self->originParentTransform : cameraRig->get_transform()->get_parent();
        // always update rotation but only update position when releasing
        if(!Manager::Camera::moving)
            getConfig().ThirdPerPos.SetValue(parent->InverseTransformPoint(self->headTransform->get_position()));
        auto rot = TransformExtensions::InverseTransformRotation(parent, self->headTransform->get_rotation()).get_eulerAngles();
        getConfig().ThirdPerRot.SetValue(rot);
    }
    wasMoving = Manager::Camera::moving;
    if(!Manager::paused && Manager::Camera::GetMode() != (int) CameraMode::Headset) {
        // head tranform IS the camera
        Vector3 targetPos;
        Quaternion targetRot;
        if(Manager::GetCurrentInfo().positionsAreLocal || Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson) {
            auto parent = self->originParentTransform ? self->originParentTransform : cameraRig->get_transform()->get_parent();
            auto rot = parent->get_rotation();
            targetPos = Sombrero::QuaternionMultiply(rot, Manager::Camera::GetHeadPosition()) + parent->get_position();
            targetRot = Sombrero::QuaternionMultiply(rot, Manager::Camera::GetHeadRotation());
        } else {
            targetPos = Manager::Camera::GetHeadPosition();
            targetRot = Manager::Camera::GetHeadRotation();
        }
        if(cameraRig)
            cameraRig->SetPositionAndRotation(targetPos, targetRot);
        if(customCamera)
            customCamera->get_transform()->SetPositionAndRotation(targetPos, targetRot);
    }
}

void Camera_Pause() {
    if(Manager::replaying && Manager::Camera::rendering && mainCamera && oldCullingMask != 0)
        mainCamera->set_cullingMask(oldCullingMask);
}

void Camera_Unpause() {
    if(Manager::replaying && Manager::Camera::rendering && mainCamera && oldCullingMask != 0)
        mainCamera->set_cullingMask(uiCullingMask);
}

#include "UnityEngine/Matrix4x4.hpp"

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

#include "GlobalNamespace/MainCameraCullingMask.hpp"
#include "GlobalNamespace/MainCamera.hpp"
#include "GlobalNamespace/MainEffectController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"

void SetupRecording() {
    LOG_INFO("Setting up recording");
    fileName = SanitizedPath(GetMapString(Manager::beatmap));
    LOG_INFO("Using filename: {}", fileName);

    LOG_INFO("Muting audio");
    JNIUtils::SetMute(true);

    if(!Manager::Camera::GetAudioMode()) {
        LOG_INFO("Beginning video capture");
        customCamera = UnityEngine::Object::Instantiate(mainCamera);
        customCamera->set_enabled(true);

        while(customCamera->get_transform()->get_childCount() > 0)
            UnityEngine::Object::DestroyImmediate(customCamera->get_transform()->GetChild(0)->get_gameObject());
        if(auto comp = customCamera->GetComponent<MainCameraCullingMask*>())
            UnityEngine::Object::DestroyImmediate(comp);
        if(auto comp = customCamera->GetComponent<MainCamera*>())
            UnityEngine::Object::DestroyImmediate(comp);

        customCamera->set_clearFlags(mainCamera->get_clearFlags());
        customCamera->set_nearClipPlane(mainCamera->get_nearClipPlane());
        customCamera->set_farClipPlane(mainCamera->get_farClipPlane());
        customCamera->set_backgroundColor(mainCamera->get_backgroundColor());
        customCamera->set_hideFlags(mainCamera->get_hideFlags());
        customCamera->set_depthTextureMode(mainCamera->get_depthTextureMode());
        customCamera->set_cullingMask(mainCamera->get_cullingMask());
        // Makes the camera render before the main
        customCamera->set_depth(mainCamera->get_depth() - 1);

        int width = getConfig().OverrideWidth.GetValue();
        int height = getConfig().OverrideHeight.GetValue();

        if(width <= 0)
            width = resolutions[getConfig().Resolution.GetValue()].first;
        if(height <= 0)
            height = resolutions[getConfig().Resolution.GetValue()].second;

        Hollywood::CameraRecordingSettings settings{
            .width = width,
            .height = height,
            .fps = getConfig().FPS.GetValue(),
            .bitrate = getConfig().Bitrate.GetValue(),
            .fov = getConfig().FOV.GetValue(),
            .filePath = TmpVidPath,
        };
        Hollywood::SetCameraCapture(customCamera, settings)->Init(settings);

        UnityEngine::Time::set_captureDeltaTime(1.0f / settings.fps);

        if(!getConfig().SFX.GetValue()) {
            LOG_INFO("Beginning audio capture");
            // might not be available from manager objects yet, and I don't mind a bit of lag here lol
            auto songSource = UnityEngine::Resources::FindObjectsOfTypeAll<AudioTimeSyncController*>().First();
            audioCapture = Hollywood::SetAudioCapture(songSource->audioSource);
            audioCapture->OpenFile(TmpAudPath);
        }
    } else {
        LOG_INFO("Beginning audio capture");
        auto audioListener = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::AudioListener*>().First([](auto x) {
            return x->get_gameObject()->get_activeInHierarchy();
        });
        audioCapture = Hollywood::SetAudioCapture(audioListener);
        audioCapture->OpenFile(TmpAudPath);
    }

    // prevent bloom from applying to the main camera
    if(auto comp = mainCamera->GetComponent<MainEffectController*>())
        UnityEngine::Object::DestroyImmediate(comp);
    // mask everything but ui
    oldCullingMask = mainCamera->get_cullingMask();
    mainCamera->set_cullingMask(uiCullingMask);
}

MAKE_HOOK_MATCH(AudioTimeSyncController_StartSong, &AudioTimeSyncController::StartSong, void, AudioTimeSyncController* self, float startTimeOffset) {

    AudioTimeSyncController_StartSong(self, startTimeOffset);

    if(Manager::replaying && Manager::Camera::rendering)
        SetupRecording();
}

#include "GlobalNamespace/CoreGameHUDController.hpp"

#include "questui/shared/BeatSaberUI.hpp"

// setup some general camera stuff
MAKE_HOOK_MATCH(CoreGameHUDController_Start, &CoreGameHUDController::Start, void, CoreGameHUDController* self) {

    CoreGameHUDController_Start(self);

    if(Manager::replaying) {
        auto levelData = (IPreviewBeatmapLevel*) Manager::beatmap->get_level();
        std::string songName = levelData->get_songName();
        std::string songAuthor = levelData->get_songAuthorName();
        std::string mapper = levelData->get_levelAuthorName();

        // TODO: maybe move elsewhere
        auto& player = Manager::GetCurrentInfo().playerName;
        if(player.has_value() && (!Manager::AreReplaysLocal() || !getConfig().HideText.GetValue())) {
            using namespace QuestUI;

            std::string text = fmt::format("<color=red>REPLAY</color>    {} - {}    Player: {}", mapper, songName, player.value());

            auto canvas = BeatSaberUI::CreateCanvas();
            canvas->get_transform()->set_position({0, getConfig().TextHeight.GetValue(), 7});

            auto textObj = BeatSaberUI::CreateText(canvas, text);
            textObj->set_fontSize(7);
            textObj->set_alignment(TMPro::TextAlignmentOptions::Center);
        }

        // set culling matrix for moved camera modes and for rendering
        static auto set_cullingMatrix = il2cpp_utils::resolve_icall<void, UnityEngine::Camera*, UnityEngine::Matrix4x4>
            ("UnityEngine.Camera::set_cullingMatrix_Injected");
        mainCamera = UnityEngine::Camera::get_main();

        set_cullingMatrix(mainCamera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
            MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * mainCamera->get_worldToCameraMatrix());

        cameraRig = ReplayHelpers::CameraRig::Create(mainCamera->get_transform());
    }
}

#include "UnityEngine/WaitForSeconds.hpp"
#include "custom-types/shared/coroutine.hpp"

custom_types::Helpers::Coroutine WaitThenMuxCoroutine() {
    bool vidExists = fileexists(TmpVidPath);
    bool audExists = fileexists(TmpAudPath);

    if(vidExists && audExists) {
        co_yield (System::Collections::IEnumerator*) UnityEngine::WaitForSeconds::New_ctor(2);

        std::string outputFile = string_format("%s/%s.mp4", RendersFolder, fileName.c_str());
        Hollywood::MuxFilesSync(TmpVidPath, TmpAudPath, TmpOutPath);
        if(fileexists(TmpOutPath))
            std::filesystem::rename(TmpOutPath, outputFile);
    } else
        LOG_ERROR("Video exists: {} Audio exists: {}", vidExists, audExists);

    if(vidExists)
        std::filesystem::remove(TmpVidPath);
    if(audExists)
        std::filesystem::remove(TmpAudPath);

    Manager::Camera::muxingFinished = true;

    co_return;
}

#include "GlobalNamespace/StandardLevelScenesTransitionSetupDataSO.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"

// undo rendering changes when exiting a level
MAKE_HOOK_MATCH(StandardLevelScenesTransitionSetupDataSO_Finish, &StandardLevelScenesTransitionSetupDataSO::Finish, void, StandardLevelScenesTransitionSetupDataSO* self, LevelCompletionResults* levelCompletionResults) {

    if(audioCapture)
        UnityEngine::Object::DestroyImmediate(audioCapture);
    audioCapture = nullptr;
    if(customCamera)
        UnityEngine::Object::DestroyImmediate(customCamera);
    customCamera = nullptr;
    if(cameraRig)
        UnityEngine::Object::DestroyImmediate(cameraRig);
    cameraRig = nullptr;
    if(mainCamera)
        mainCamera->set_enabled(true);
    oldCullingMask = 0;
    mainCamera = nullptr;

    UnityEngine::Time::set_captureDeltaTime(0);
    LOG_INFO("Unmuting audio");
    JNIUtils::SetMute(false);

    // mux audio and video when done with both
    if(Manager::Camera::rendering && (Manager::Camera::GetAudioMode() || !getConfig().SFX.GetValue()))
        SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(WaitThenMuxCoroutine()));

    return StandardLevelScenesTransitionSetupDataSO_Finish(self, levelCompletionResults);
}

// delay ending of renders (min check in shared hooks)
MAKE_HOOK_MATCH(AudioTimeSyncController_get_songEndTime, &AudioTimeSyncController::get_songEndTime, float, AudioTimeSyncController* self) {

    float ret = AudioTimeSyncController_get_songEndTime(self);

    if(Manager::replaying && Manager::Camera::rendering && !Manager::Camera::GetAudioMode())
        return ret + 1;
    return ret;
}

#include "GlobalNamespace/PauseController.hpp"

// prevent pauses during recording
MAKE_HOOK_MATCH(PauseController_get_canPause, &PauseController::get_canPause, bool, PauseController* self) {

    if(Manager::replaying && Manager::Camera::rendering)
        return getConfig().Pauses.GetValue();

    return PauseController_get_canPause(self);
}

#include "GlobalNamespace/MainSystemInit.hpp"

// get mirror and bloom presets
MAKE_HOOK_MATCH(MainSystemInit_Init, &MainSystemInit::Init, void, MainSystemInit* self) {

    MainSystemInit_Init(self);

    Manager::Camera::bloomPresets = self->mainEffectGraphicsSettingsPresets;
    LOG_INFO("Bloom presets: {}", self->mainEffectGraphicsSettingsPresets->presets.Length());
    Manager::Camera::bloomContainer = self->mainEffectContainer;
    Manager::Camera::mirrorPresets = self->mirrorRendererGraphicsSettingsPresets;
    LOG_INFO("Mirror presets: {}", self->mirrorRendererGraphicsSettingsPresets->presets.Length());
    Manager::Camera::mirrorRenderer = self->mirrorRenderer;
}

HOOK_FUNC(
    INSTALL_HOOK(logger, AudioTimeSyncController_StartSong);
    INSTALL_HOOK(logger, CoreGameHUDController_Start);
    INSTALL_HOOK(logger, StandardLevelScenesTransitionSetupDataSO_Finish);
    INSTALL_HOOK(logger, AudioTimeSyncController_get_songEndTime);
    INSTALL_HOOK(logger, PauseController_get_canPause);
    INSTALL_HOOK(logger, MainSystemInit_Init);
)
