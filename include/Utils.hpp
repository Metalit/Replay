#pragma once

#include "Formats/FrameReplay.hpp"

#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/CustomDifficultyBeatmap.hpp"
#include "UnityEngine/Sprite.hpp"

enum struct ReplayType {
    REQLAY,
    BSOR
};

std::string GetReqlaysPath();

std::string GetBSORsPath();

std::string GetHash(GlobalNamespace::IPreviewBeatmapLevel* level);

std::unordered_map<std::string, ReplayType> GetReplays(GlobalNamespace::CustomDifficultyBeatmap* beatmap);

UnityEngine::Sprite* GetReplayIcon();

std::string SecondsToString(int value);

std::string GetStringForTimeSinceNow(std::time_t start);

std::string GetModifierString(const ReplayModifiers& modifiers, bool includeNoFail);
