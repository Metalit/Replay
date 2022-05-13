#include "Main.hpp"
#include "ReplayManager.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "ReplayMenu.hpp"
#include "Utils.hpp"

using namespace GlobalNamespace;

CustomDifficultyBeatmap* beatmap = nullptr;
std::unordered_map<std::string, ReplayType> currentReplays;

ReplayType currentReplayType;
FrameReplay currentFrameReplay;
EventReplay currentEventReplay;

namespace Manager {
    void SetLevel(CustomDifficultyBeatmap* level) {
        beatmap = level;
        RefreshLevelReplays();
    }

    void RefreshLevelReplays() {
        currentReplays = GetReplays(beatmap);
    }
}
