#pragma once

#include "Formats/FrameReplay.hpp"

#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "UnityEngine/Sprite.hpp"

enum struct ReplayType {
    REQLAY,
    BSOR
};

std::string GetReqlaysPath();

std::string GetBSORsPath();

std::string GetHash(GlobalNamespace::IPreviewBeatmapLevel* level);

std::unordered_map<std::string, ReplayType> GetReplays(GlobalNamespace::IDifficultyBeatmap* beatmap);

UnityEngine::Sprite* GetReplayIcon();

std::string SecondsToString(int value);

std::string GetStringForTimeSinceNow(std::time_t start);

std::string GetModifierString(const ReplayModifiers& modifiers, bool includeNoFail);

void GetBeatmapData(GlobalNamespace::IDifficultyBeatmap* beatmap, std::function<void(GlobalNamespace::IReadonlyBeatmapData*)> callback);
