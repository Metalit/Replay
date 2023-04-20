#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

class Hooks {
    private:
    static inline std::vector<void(*)(Logger&)> InstallFuncs;

    public:
    Hooks(void(*installFunc)(Logger& logger)) {
        InstallFuncs.push_back(installFunc);
    }

    static void Install(Logger& logger) {
        for(auto& func : InstallFuncs)
            func(logger);
    }
};

#define HOOK_FUNC(...) static Hooks hook(*[](Logger& logger) { __VA_ARGS__ });

namespace GlobalNamespace { class PlayerTransforms; }
void Camera_PlayerTransformsUpdate_Pre(GlobalNamespace::PlayerTransforms* self);
void Replay_PlayerTransformsUpdate_Post(GlobalNamespace::PlayerTransforms* self);

namespace UnityEngine { class Camera; }
extern UnityEngine::Camera* mainCamera;
