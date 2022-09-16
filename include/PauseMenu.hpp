#pragma once

#include "GlobalNamespace/PauseMenuManager.hpp"

namespace Pause {
    void EnsureSetup(GlobalNamespace::PauseMenuManager* pauseMenu);

    void SetTime(float time);
}
