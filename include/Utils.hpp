#pragma once

#include "Replay.hpp"

#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "UnityEngine/Sprite.hpp"

std::string GetReqlaysPath();

std::string GetBSORsPath();

std::string GetHash(GlobalNamespace::IPreviewBeatmapLevel* level);

std::unordered_map<std::string, ReplayWrapper> GetReplays(GlobalNamespace::IDifficultyBeatmap* beatmap);

UnityEngine::Sprite* GetReplayIcon();

std::string SecondsToString(int value);

std::string GetStringForTimeSinceNow(std::time_t start);

std::string GetModifierString(const ReplayModifiers& modifiers, bool includeNoFail);

void GetBeatmapData(GlobalNamespace::IDifficultyBeatmap* beatmap, std::function<void(GlobalNamespace::IReadonlyBeatmapData*)> callback);

GlobalNamespace::NoteCutInfo GetNoteCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber, const class ReplayNoteCutInfo& info);

float ModifierMultiplier(const ReplayWrapper& replay, bool failed);

float EnergyForNote(const class NoteEventInfo& noteEvent);

int ScoreForNote(const class NoteEvent& note, bool max = false);

int IdForNoteData(GlobalNamespace::NoteData *noteData);
int IdForNoteEventInfo(NoteEventInfo eventInfo);
int NoteDataEqualToEventInfo(GlobalNamespace::NoteData *noteData, NoteEventInfo eventInfo);

struct MapPreview {
    float energy;
    int combo;
    int score;
    int maxScore;
};

MapPreview MapAtTime(const ReplayWrapper& replay, float time);
