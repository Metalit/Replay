#include "Main.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"

#include "hollywood/shared/Hollywood.hpp"



#include "GlobalNamespace/LightManager.hpp"
#include "GlobalNamespace/CoreGameHUDController.hpp"
#include "GlobalNamespace/PauseController.hpp"

#include "UnityEngine/Matrix4x4.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/CameraClearFlags.hpp"
#include "UnityEngine/HideFlags.hpp"
#include "UnityEngine/DepthTextureMode.hpp"

#include "UnityEngine/Time.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"

using namespace GlobalNamespace;

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

UnityEngine::Camera *renderCamera;
UnityEngine::Camera *playerCamera;

MAKE_HOOK_MATCH(LightManager_OnCameraPreRender, &LightManager::OnCameraPreRender, void, LightManager* self, UnityEngine::Camera* camera) {
    // if (!Manager::replaying || Manager::paused || Manager::Camera::mode != Manager::Camera::Mode::HEADSET)
    //     return LightManager_OnCameraPreRender(self, camera);

    // if (camera != renderCamera)
    //     return;

    LightManager_OnCameraPreRender(self, camera);


    if(Manager::replaying && !Manager::paused && Manager::GetSongTime() >= 0 && Manager::Camera::mode != Manager::Camera::Mode::HEADSET) {
        camera->set_position(Manager::Camera::GetHeadPosition());
        camera->set_rotation(Manager::Camera::GetHeadRotation());
    }
}



Hollywood::AudioCapture* audioCapture = nullptr;

// start recording when the level actually loads
MAKE_HOOK_MATCH(CoreGameHUDController_Start, &CoreGameHUDController::Start, void, CoreGameHUDController* self) {

    CoreGameHUDController_Start(self);

    renderCamera = playerCamera = UnityEngine::Camera::get_main();



    if (Manager::replaying && Manager::Camera::rendering) {
        Hollywood::CameraRecordingSettings settings{};

        // audio
        if (!settings.movieModeRendering)
        {
            auto audioListener = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::AudioListener *>().First([](auto x)
                                                                                                                    { return x->get_gameObject()->get_activeInHierarchy(); });
            audioCapture = Hollywood::SetAudioCapture(audioListener);
            audioCapture->OpenFile("/sdcard/audio.wav");
        }
        else // video
        {
            // playerCamera->set_enabled(false);

            // auto renderCameraObject = UnityEngine::Object::Instantiate(playerCamera->get_gameObject(), playerCamera->get_transform());
            // auto renderCamera = renderCamera = renderCameraObject->GetComponent<UnityEngine::Camera*>();
            renderCamera = UnityEngine::Object::Instantiate(playerCamera);
            // UnityEngine::Object::DontDestroyOnLoad(renderCamera);
            renderCamera->set_enabled(true);
            playerCamera->set_enabled(false);

            while (renderCamera->get_transform()->get_childCount() > 0)
                UnityEngine::Object::DestroyImmediate(renderCamera->get_transform()->GetChild(0)->get_gameObject());
            // TODO: Are these needed?
            // UnityEngine::Object::DestroyImmediate(renderCamera->GetComponent("CameraRenderCallbacksManager"));
            // UnityEngine::Object::DestroyImmediate(renderCamera->GetComponent("AudioListener"));
            // UnityEngine::Object::DestroyImmediate(renderCamera->GetComponent("MeshCollider"));

            renderCamera->set_clearFlags(playerCamera->get_clearFlags());
            renderCamera->set_nearClipPlane(playerCamera->get_nearClipPlane());
            renderCamera->set_farClipPlane(playerCamera->get_farClipPlane());
            renderCamera->set_cullingMask(playerCamera->get_cullingMask());
            renderCamera->set_backgroundColor(playerCamera->get_backgroundColor());
            renderCamera->set_hideFlags(playerCamera->get_hideFlags());
            renderCamera->set_depthTextureMode(playerCamera->get_depthTextureMode());
            // Makes the camera render before the main
            renderCamera->set_depth(playerCamera->get_depth() - 1);

            Hollywood::SetCameraCapture(renderCamera, settings)->Init(settings);

            UnityEngine::Time::set_captureDeltaTime(1.0f / settings.fps);

            using cullingMatrixType = function_ptr_t<void, UnityEngine::Camera *, UnityEngine::Matrix4x4>;
            static auto set_cullingMatrix = *((cullingMatrixType)il2cpp_functions::resolve_icall("UnityEngine.Camera::set_cullingMatrix_Injected"));

            // set_cullingMatrix(renderCamera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
            //                                   MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * playerCamera->get_worldToCameraMatrix());
        }
    }

    renderCameraTransform = renderCamera->get_transform();
}

#include "GlobalNamespace/ResultsViewController.hpp"

// undo rendering changes when finishing a level
MAKE_HOOK_MATCH(LightManager_OnDestroy, &LightManager::OnDestroy, void, LightManager *self)
{
    if (playerCamera)
    {
        playerCamera->set_enabled(true);
    }

    rendercamera = nullptr;
    playerCamera = nullptr;

    LightManager_OnDestroy(self);
}

MAKE_HOOK_MATCH(ResultsViewController_Init, &ResultsViewController::Init, void, ResultsViewController* self, LevelCompletionResults* levelCompletionResults, IReadonlyBeatmapData* transformedBeatmapData, IDifficultyBeatmap* difficultyBeatmap, bool practice, bool newHighScore) {

    if(audioCapture)
        UnityEngine::Object::Destroy(audioCapture);
    audioCapture = nullptr;

    // UnityEngine::Camera::get_main()->set_enabled(true);

    UnityEngine::Time::set_captureDeltaTime(0.0f);

    ResultsViewController_Init(self, levelCompletionResults, transformedBeatmapData, difficultyBeatmap, practice, newHighScore);
}



// prevent pauses during recording
MAKE_HOOK_MATCH(PauseController_get_canPause, &PauseController::get_canPause, bool, PauseController* self) {
    
    if(Manager::replaying && Manager::Camera::rendering)
        return false;
    
    return PauseController_get_canPause(self);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, LightManager_OnDestroy;)
    INSTALL_HOOK(logger, LightManager_OnCameraPreRender);
    INSTALL_HOOK(logger, CoreGameHUDController_Start);
    INSTALL_HOOK(logger, ResultsViewController_Init);
    INSTALL_HOOK(logger, PauseController_get_canPause);
)