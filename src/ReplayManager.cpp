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
std::unordered_map<std::string, FrameReplay> currentFrameReplays;
std::unordered_map<std::string, EventReplay> currentEventReplays;

namespace Manager {
    void SetLevel(CustomDifficultyBeatmap* level) {
        beatmap = level;
        RefreshLevelReplays();
    }

    // man I really should have used an inheritance kind of thing huh
    void RefreshLevelReplays() {
        currentReplays = GetReplays(beatmap);
        currentFrameReplays.clear();
        currentEventReplays.clear();
        for(auto& pair : currentReplays) {
            switch (pair.second) {
            case ReplayType::REQLAY:
                currentFrameReplays.insert({pair.first, ReadReqlay(pair.first)});
                break;
            case ReplayType::BSOR:
                currentEventReplays.insert({pair.first, ReadBSOR(pair.first)});
                break;
            }
        }
        if(currentReplays.size() > 0) {
            Menu::SetButtonEnabled(true);
            std::vector<std::string> paths;
            std::vector<ReplayInfo*> infos;
            for(auto& pair : currentEventReplays) {
                paths.emplace_back(pair.first);
                infos.emplace_back(&pair.second.info);
            }
            for(auto& pair : currentFrameReplays) {
                paths.emplace_back(pair.first);
                infos.emplace_back(&pair.second.info);
            }
            Menu::SetReplays(infos, paths);
        } else
            Menu::SetButtonEnabled(false);
    }
}
