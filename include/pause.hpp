#pragma once

#include "main.hpp"

namespace Pause {
    void OnPause();
    void OnUnpause();
    void SceneEnded();

    void SetSpeed(float value);
    void SetTime(float value);

    void UpdateInputs();
}
