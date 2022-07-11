#include "Main.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "UnityEngine/Matrix4x4.hpp"
#include "GlobalNamespace/LightManager.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Transform.hpp"

UnityEngine::Matrix4x4 MatrixTranslate(UnityEngine::Vector3 vector) {
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

MAKE_HOOK_MATCH(LightManager_OnCameraPreRender, &LightManager::OnCameraPreRender, void, LightManager* self, UnityEngine::Camera* camera) {

    LightManager_OnCameraPreRender(self, camera);

    if(Manager::replaying && !Manager::paused && Manager::GetSongTime() >= 0 && Manager::Camera::mode != Manager::Camera::Mode::HEADSET) {
        auto mainCamera = UnityEngine::Camera::get_main();
        mainCamera->get_transform()->set_position(Manager::Camera::GetHeadPosition());
        mainCamera->get_transform()->set_rotation(Manager::Camera::GetHeadRotation());
        
        using cullingMatrixType = function_ptr_t<void, UnityEngine::Camera *, UnityEngine::Matrix4x4>;
        auto set_cullingMatrix = *((cullingMatrixType) il2cpp_functions::resolve_icall("UnityEngine.Camera::set_cullingMatrix_Injected"));

        set_cullingMatrix(camera, UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001f, 99999) *
            MatrixTranslate(UnityEngine::Vector3::get_forward() * -99999 / 2) * camera->get_worldToCameraMatrix());
    }
}

HOOK_FUNC(
    INSTALL_HOOK(logger, LightManager_OnCameraPreRender);
)