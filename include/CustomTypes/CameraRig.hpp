#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/GameObject.hpp"

#include "GlobalNamespace/AvatarPoseController.hpp"

#include "TMPro/TextMeshProUGUI.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, CameraRig, UnityEngine::MonoBehaviour,

    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, child);
    DECLARE_INSTANCE_FIELD(GlobalNamespace::AvatarPoseController*, avatar);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, progress);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, progressText);

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, SetPositionAndRotation, UnityEngine::Vector3 pos, UnityEngine::Quaternion rot);
    DECLARE_INSTANCE_METHOD(void, UpdateProgress);

    private:
    bool pausedLastFrame;
    public:
    static CameraRig* Create(UnityEngine::Transform* cameraTransform);
)
