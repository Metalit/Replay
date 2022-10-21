#pragma once

#include "UnityEngine/MonoBehaviour.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/XR/InputDevice.hpp"

#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, CameraRig, UnityEngine::MonoBehaviour,

    bool tracking = true;

    DECLARE_DEFAULT_CTOR();

    DECLARE_INSTANCE_METHOD(void, Update);
    DECLARE_DTOR(OnDestroy);
)
