#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, CameraRig, UnityEngine::MonoBehaviour,

    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, child);

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, SetPositionAndRotation, UnityEngine::Vector3 pos, UnityEngine::Quaternion rot);

    public:
    static CameraRig* Create(UnityEngine::Transform* cameraTransform);
)
