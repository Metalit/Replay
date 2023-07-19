#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"

#include "GlobalNamespace/AvatarPoseController.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, CameraRig, UnityEngine::MonoBehaviour,

    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, child);
    DECLARE_INSTANCE_FIELD(GlobalNamespace::AvatarPoseController*, avatar);

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, SetPositionAndRotation, UnityEngine::Vector3 pos, UnityEngine::Quaternion rot);

    private:
    bool pausedLastFrame;
    public:
    static CameraRig* Create(UnityEngine::Transform* cameraTransform);
)
