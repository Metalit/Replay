#include "Main.hpp"
#include "Config.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "UnityEngine/Matrix4x4.hpp"
#include "GlobalNamespace/LightManager.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Transform.hpp"

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

#include "hollywood/shared/Hollywood.hpp"

UnityEngine::Camera* mainCamera = nullptr;
Hollywood::AudioCapture* audioCapture = nullptr;
UnityEngine::Camera* customCamera = nullptr;

MAKE_HOOK_MATCH(LightManager_OnCameraPreRender, &LightManager::OnCameraPreRender, void, LightManager* self, UnityEngine::Camera* camera) {

    LightManager_OnCameraPreRender(self, camera);

    if(Manager::replaying && !Manager::paused && Manager::GetSongTime() >= 0 && Manager::Camera::GetMode() != (int) CameraMode::Headset) {
        if(!mainCamera)
            mainCamera = UnityEngine::Camera::get_main();
        if(camera != mainCamera && camera != customCamera)
            return;
        camera->get_transform()->set_rotation(Manager::Camera::GetHeadRotation());
        camera->get_transform()->set_position(Manager::Camera::GetHeadPosition());
    } else
        mainCamera = nullptr;
}

#include "GlobalNamespace/CoreGameHUDController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/DepthTextureMode.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/AudioListener.hpp"

#include "questui/shared/BeatSaberUI.hpp"

// start recording when the level actually loads
MAKE_HOOK_MATCH(CoreGameHUDController_Start, &CoreGameHUDController::Start, void, CoreGameHUDController* self) {

    CoreGameHUDController_Start(self);

    if(Manager::replaying) {
        // TODO: maybe move elsewhere
        auto& player = Manager::currentReplay.replay->info.playerName;
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
        if(Manager::Camera::GetMode() == (int) CameraMode::Headset)
            return;
        
        using cullingMatrixType = function_ptr_t<void, UnityEngine::Camera *, UnityEngine::Matrix4x4>;
        static auto set_cullingMatrix = *((cullingMatrixType) il2cpp_functions::resolve_icall("UnityEngine.Camera::set_cullingMatrix_Injected"));

        auto mainCamera = UnityEngine::Camera::get_main();
        set_cullingMatrix(mainCamera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
            MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * mainCamera->get_worldToCameraMatrix());
        // mainCamera->set_enabled(false);

        if(!Manager::Camera::rendering)
            return;

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
        customCamera->set_cullingMask(mainCamera->get_cullingMask());
        customCamera->set_backgroundColor(mainCamera->get_backgroundColor());
        customCamera->set_hideFlags(mainCamera->get_hideFlags());
        customCamera->set_depthTextureMode(mainCamera->get_depthTextureMode());
        // Makes the camera render before the main
        customCamera->set_depth(mainCamera->get_depth() - 1);

        set_cullingMatrix(customCamera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
            MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * customCamera->get_worldToCameraMatrix());
        
        Hollywood::CameraRecordingSettings settings{
            .width = resolutions[getConfig().Resolution.GetValue()].first,
            .height = resolutions[getConfig().Resolution.GetValue()].second,
            .fps = getConfig().FPS.GetValue(),
            .bitrate = getConfig().Bitrate.GetValue(),
            .fov = getConfig().FOV.GetValue()
        };
        Hollywood::SetCameraCapture(customCamera, settings)->Init(settings);

        UnityEngine::Time::set_captureDeltaTime(1.0f / settings.fps);

        if(!settings.movieModeRendering) {
            auto audioListener = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::AudioListener*>().First([](auto x) { return x->get_gameObject()->get_activeInHierarchy(); });
            audioCapture = Hollywood::SetAudioCapture(audioListener);
            audioCapture->OpenFile("/sdcard/audio.wav");
        }
    }
}

#include "GlobalNamespace/PrepareLevelCompletionResults.hpp"

// undo rendering changes when exiting a level
MAKE_HOOK_MATCH(PrepareLevelCompletionResults_FillLevelCompletionResults_Camera, &PrepareLevelCompletionResults::FillLevelCompletionResults,
        LevelCompletionResults*, PrepareLevelCompletionResults* self, LevelCompletionResults::LevelEndStateType levelEndStateType, LevelCompletionResults::LevelEndAction levelEndAction) {

    if(audioCapture)
        UnityEngine::Object::Destroy(audioCapture);
    audioCapture = nullptr;
    if(customCamera)
        UnityEngine::Object::Destroy(customCamera);
    customCamera = nullptr;

    // UnityEngine::Camera::get_main()->set_enabled(true);

    UnityEngine::Time::set_captureDeltaTime(0);

    return PrepareLevelCompletionResults_FillLevelCompletionResults_Camera(self, levelEndStateType, levelEndAction);
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
    INSTALL_HOOK(logger, LightManager_OnCameraPreRender);
    INSTALL_HOOK(logger, CoreGameHUDController_Start);
    INSTALL_HOOK(logger, PrepareLevelCompletionResults_FillLevelCompletionResults_Camera);
    INSTALL_HOOK(logger, PauseController_get_canPause);
    INSTALL_HOOK(logger, MainSystemInit_Init);
)