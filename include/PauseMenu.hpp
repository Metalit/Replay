#pragma once

#include "GlobalNamespace/PauseMenuManager.hpp"

namespace Pause {
    void EnsureSetup(GlobalNamespace::PauseMenuManager* pauseMenu);
    void OnUnpause();

    void SetSpeed(float speed);
    void PreviewTime(float time);
    void SetTime(float time);
}
