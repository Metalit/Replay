#pragma once

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "main.hpp"

namespace Camera {
    extern int reinitDistortions;

    void SetMode(int value);

    void SetupCamera();
    void FinishReplay();

    void OnPause();
    void OnUnpause();
    void OnRestart();

    bool UpdateTime(GlobalNamespace::AudioTimeSyncController* controller);
    void UpdateCamera(GlobalNamespace::PlayerTransforms* player);

    void SetMoving(bool value);
    void Travel(int direction);

    void CreateReplayText();
}
