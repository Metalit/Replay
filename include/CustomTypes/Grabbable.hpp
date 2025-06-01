#pragma once

#include "GlobalNamespace/VRController.hpp"
#include "UnityEngine/GameObject.hpp"
#include "VRUIControls/VRInputModule.hpp"
#include "custom-types/shared/macros.hpp"
#include "math.hpp"

DECLARE_CLASS_CODEGEN(Replay, Grabbable, UnityEngine::MonoBehaviour) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);

    DECLARE_INSTANCE_FIELD(GlobalNamespace::VRController*, controller);
    DECLARE_INSTANCE_FIELD(VRUIControls::VRInputModule*, inputModule);
    DECLARE_INSTANCE_FIELD(Vector3, grabPos);
    DECLARE_INSTANCE_FIELD(Quaternion, grabRot);

   public:
    std::function<void(UnityEngine::GameObject*)> onRelease = nullptr;
};
