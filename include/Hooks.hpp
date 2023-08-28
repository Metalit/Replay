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

void Camera_Pause();
void Camera_Unpause();

namespace UnityEngine { class Camera; }
extern UnityEngine::Camera* mainCamera;
