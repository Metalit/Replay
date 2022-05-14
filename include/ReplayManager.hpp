#pragma once

#include "GlobalNamespace/IDifficultyBeatmap.hpp"

namespace Manager {
    void SetLevel(GlobalNamespace::IDifficultyBeatmap* level);
    
    void RefreshLevelReplays();
}
