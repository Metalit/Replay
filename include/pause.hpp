#pragma once

#include "GlobalNamespace/ScoreMultiplierUIController.hpp"
#include "main.hpp"

namespace Pause {
    void OnPause();
    void OnUnpause();

    bool AllowAnimation(GlobalNamespace::ScoreMultiplierUIController* multiplier);

    void SetSpeed(float value);
    void SetTime(float value);

    void UpdateInputs();
}
