#include "Main.hpp"
#include "Config.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"
#include "CustomTypes/CameraRig.hpp"

#include "hollywood/shared/Hollywood.hpp"

#include "GlobalNamespace/PlayerTransforms.hpp"

using namespace GlobalNamespace;

Hollywood::AudioCapture* audioCapture = nullptr;
UnityEngine::Camera* customCamera = nullptr;

ReplayHelpers::CameraRig* cameraRig = nullptr;
UnityEngine::Camera* mainCamera = nullptr;

MAKE_HOOK_MATCH(PlayerTransforms_Update_Camera, &PlayerTransforms::Update, void, PlayerTransforms* self) {

    if(Manager::replaying && !Manager::paused && Manager::Camera::GetMode() != (int) CameraMode::Headset) {
        // head tranform IS the camera, but we can set its position anyway for some reason
        Vector3 targetPos;
        Quaternion targetRot;
        if(Manager::GetCurrentInfo().positionsAreLocal) {
            auto parent = self->originParentTransform ? self->originParentTransform : self->headTransform->get_parent();
            auto rot = parent->get_rotation();
            targetPos = Sombrero::QuaternionMultiply(rot, Manager::Camera::GetHeadPosition()) + parent->get_position();
            targetRot = Sombrero::QuaternionMultiply(rot, Manager::Camera::GetHeadRotation());
        } else {
            targetPos = Manager::Camera::GetHeadPosition();
            targetRot = Manager::Camera::GetHeadRotation();
        }
        self->headTransform->SetPositionAndRotation(targetPos, targetRot);
        if(customCamera)
            customCamera->get_transform()->SetPositionAndRotation(targetPos, targetRot);
    }
    PlayerTransforms_Update_Camera(self);
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

#include "GlobalNamespace/CoreGameHUDController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/AudioListener.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "GlobalNamespace/SharedCoroutineStarter.hpp"

#include "custom-types/shared/coroutine.hpp"

custom_types::Helpers::Coroutine SetCullingCoro(UnityEngine::Camera* camera, UnityEngine::Camera* mainCam) {
    co_yield nullptr;
    camera->set_cullingMask(mainCam->get_cullingMask());
    co_return;
}

// start recording when the level actually loads
MAKE_HOOK_MATCH(CoreGameHUDController_Start, &CoreGameHUDController::Start, void, CoreGameHUDController* self) {

    CoreGameHUDController_Start(self);

    if(Manager::replaying) {
        // TODO: maybe move elsewhere
        auto& player = Manager::GetCurrentInfo().playerName;
        if(player.has_value() && (!Manager::AreReplaysLocal() || !getConfig().HideText.GetValue())) {
            using namespace QuestUI;

            auto levelData = (IPreviewBeatmapLevel*) Manager::beatmap->get_level();
            std::string songName = levelData->get_songName();
            std::string mapper = levelData->get_levelAuthorName();

            std::string text = fmt::format("<color=red>REPLAY</color>    {} - {}    Player: {}", mapper, songName, player.value());

            auto canvas = BeatSaberUI::CreateCanvas();
            canvas->get_transform()->set_position({0, 3.5, getConfig().TextHeight.GetValue()});

            auto textObj = BeatSaberUI::CreateText(canvas, text);
            textObj->set_fontSize(7);
            textObj->set_alignment(TMPro::TextAlignmentOptions::Center);
        }

        // set culling matrix for moved camera modes and for rendering
        static auto set_cullingMatrix = il2cpp_utils::resolve_icall<void, UnityEngine::Camera*, UnityEngine::Matrix4x4>
            ("UnityEngine.Camera::set_cullingMatrix_Injected");
        mainCamera = UnityEngine::Camera::get_main();

        if(Manager::Camera::GetMode() != (int) CameraMode::Headset) {
            set_cullingMatrix(mainCamera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
                MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * mainCamera->get_worldToCameraMatrix());

            auto cameraGO = UnityEngine::GameObject::New_ctor("ReplayCameraRig");
            cameraRig = cameraGO->AddComponent<ReplayHelpers::CameraRig*>();
            auto trans = mainCamera->get_transform();
            cameraGO->get_transform()->SetPositionAndRotation(trans->get_position(), trans->get_rotation());
            cameraGO->get_transform()->SetParent(trans->GetParent(), false);
            trans->SetParent(cameraGO->get_transform(), false);
        }

        if(Manager::Camera::rendering) {
            if(!getConfig().AudioMode.GetValue()) {
                LOG_INFO("Beginning video capture");
                customCamera = UnityEngine::Object::Instantiate(mainCamera);
                customCamera->set_enabled(true);

                while (customCamera->get_transform()->get_childCount() > 0)
                    UnityEngine::Object::DestroyImmediate(customCamera->get_transform()->GetChild(0)->get_gameObject());
                UnityEngine::Object::DestroyImmediate(customCamera->GetComponent("CameraRenderCallbacksManager"));
                UnityEngine::Object::DestroyImmediate(customCamera->GetComponent("AudioListener"));
                UnityEngine::Object::DestroyImmediate(customCamera->GetComponent("MeshCollider"));
                
                customCamera->set_clearFlags(mainCamera->get_clearFlags());
                customCamera->set_nearClipPlane(mainCamera->get_nearClipPlane());
                customCamera->set_farClipPlane(mainCamera->get_farClipPlane());
                customCamera->set_backgroundColor(mainCamera->get_backgroundColor());
                customCamera->set_hideFlags(mainCamera->get_hideFlags());
                customCamera->set_depthTextureMode(mainCamera->get_depthTextureMode());
                // debris culling mask is set later in the frame, in a different Start() method
                SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(SetCullingCoro(customCamera, mainCamera)));
                // Makes the camera render before the main
                customCamera->set_depth(mainCamera->get_depth() - 1);

                set_cullingMatrix(customCamera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
                    MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * customCamera->get_worldToCameraMatrix());
                
                Hollywood::CameraRecordingSettings settings{
                    .width = resolutions[getConfig().Resolution.GetValue()].first,
                    .height = resolutions[getConfig().Resolution.GetValue()].second,
                    .fps = getConfig().FPS.GetValue(),
                    .bitrate = getConfig().Bitrate.GetValue(),
                    .movieModeRendering = true,
                    .fov = getConfig().FOV.GetValue()
                };
                Hollywood::SetCameraCapture(customCamera, settings)->Init(settings);

                UnityEngine::Time::set_captureDeltaTime(1.0f / settings.fps);

                if(getConfig().CameraOff.GetValue())
                    mainCamera->set_enabled(false);
            } else {
                LOG_INFO("Beginning audio capture");
                auto audioListener = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::AudioListener*>().First([](auto x) {
                    return x->get_gameObject()->get_activeInHierarchy();
                });
                audioCapture = Hollywood::SetAudioCapture(audioListener);
                audioCapture->OpenFile("/sdcard/audio.wav");
            }
        }
    }
}

#include "GlobalNamespace/PrepareLevelCompletionResults.hpp"

// undo rendering changes when exiting a level
MAKE_HOOK_MATCH(PrepareLevelCompletionResults_FillLevelCompletionResults, &PrepareLevelCompletionResults::FillLevelCompletionResults,
        LevelCompletionResults*, PrepareLevelCompletionResults* self, LevelCompletionResults::LevelEndStateType levelEndStateType, LevelCompletionResults::LevelEndAction levelEndAction) {

    if(audioCapture)
        UnityEngine::Object::Destroy(audioCapture);
    audioCapture = nullptr;
    if(customCamera)
        UnityEngine::Object::Destroy(customCamera);
    customCamera = nullptr;
    if(cameraRig)
        UnityEngine::Object::Destroy(cameraRig);
    cameraRig = nullptr;
    if(mainCamera)
        mainCamera->set_enabled(true);
    mainCamera = nullptr;

    UnityEngine::Time::set_captureDeltaTime(0);

    return PrepareLevelCompletionResults_FillLevelCompletionResults(self, levelEndStateType, levelEndAction);
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

    // Manager::Camera::bloomPresets = self->bloomPrePassGraphicsSettingsPresets;
    // Manager::Camera::bloomContainer = self->bloomPrePassEffectContainer;
    Manager::Camera::mirrorPresets = self->mirrorRendererGraphicsSettingsPresets;
    Manager::Camera::mirrorRenderer = self->mirrorRenderer;
}

HOOK_FUNC(
    INSTALL_HOOK(logger, PlayerTransforms_Update_Camera);
    INSTALL_HOOK(logger, CoreGameHUDController_Start);
    INSTALL_HOOK(logger, PrepareLevelCompletionResults_FillLevelCompletionResults);
    INSTALL_HOOK(logger, PauseController_get_canPause);
    INSTALL_HOOK(logger, MainSystemInit_Init);
)
