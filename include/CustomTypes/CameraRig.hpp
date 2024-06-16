#pragma once

#include "BeatSaber/BeatAvatarSDK/BeatAvatarPoseController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/SpatialTracking/TrackedPoseDriver.hpp"
#include "UnityEngine/Transform.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, CameraRig, UnityEngine::MonoBehaviour,

    DECLARE_INSTANCE_FIELD(UnityEngine::Transform*, fakeHead);
    DECLARE_INSTANCE_FIELD(UnityEngine::SpatialTracking::TrackedPoseDriver*, cameraTracker);
    DECLARE_INSTANCE_FIELD(BeatSaber::BeatAvatarSDK::BeatAvatarPoseController*, avatar);
    DECLARE_INSTANCE_FIELD(UnityEngine::GameObject*, progress);
    DECLARE_INSTANCE_FIELD(TMPro::TextMeshProUGUI*, progressText);

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_INSTANCE_METHOD(void, SetPositionAndRotation, UnityEngine::Vector3 pos, UnityEngine::Quaternion rot);
    DECLARE_INSTANCE_METHOD(void, UpdateProgress);

   private:
    bool pausedLastFrame;
    std::string mapString;
   public:
    static CameraRig* Create(UnityEngine::Transform* cameraTransform);
)
