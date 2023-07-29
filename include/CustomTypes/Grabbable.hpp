#pragma once

#include "UnityEngine/GameObject.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "VRUIControls/VRInputModule.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, Grabbable, UnityEngine::MonoBehaviour,
    DECLARE_DEFAULT_CTOR();
    DECLARE_SIMPLE_DTOR();
    DECLARE_INSTANCE_METHOD(void, Update);

    DECLARE_INSTANCE_FIELD(GlobalNamespace::VRController*, controller);
    DECLARE_INSTANCE_FIELD(VRUIControls::VRInputModule*, inputModule);
    DECLARE_INSTANCE_FIELD(UnityEngine::Vector3, grabPos);
    DECLARE_INSTANCE_FIELD(UnityEngine::Quaternion, grabRot);

    public:
    std::function<void(UnityEngine::GameObject*)> onRelease = nullptr;
)
