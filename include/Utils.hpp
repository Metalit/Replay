#pragma once

#include "Replay.hpp"

#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/IPreviewBeatmapLevel.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "UnityEngine/Sprite.hpp"

long EpochTime();

std::string SanitizedPath(std::string path);

std::string GetReqlaysPath();

std::string GetBSORsPath();

std::string GetSSReplaysPath();

std::string GetDifficultyName(GlobalNamespace::BeatmapDifficulty difficulty);

std::string GetDifficultyName(int difficulty);

std::string GetCharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic);

std::string GetCharacteristicName(std::string characteristicName);

std::string GetHash(GlobalNamespace::IPreviewBeatmapLevel* level);

std::vector<std::pair<std::string, ReplayWrapper>> GetReplays(GlobalNamespace::IDifficultyBeatmap* beatmap);

std::string SecondsToString(int value);

std::string GetStringForTimeSinceNow(std::time_t start);

std::string GetModifierString(const ReplayModifiers& modifiers, bool includeNoFail);

void GetBeatmapData(GlobalNamespace::IDifficultyBeatmap* beatmap, std::function<void(GlobalNamespace::IReadonlyBeatmapData*)> callback);

GlobalNamespace::NoteCutInfo GetNoteCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber, const class ReplayNoteCutInfo& info);

GlobalNamespace::NoteCutInfo GetBombCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber);

float ModifierMultiplier(const ReplayWrapper& replay, bool failed);

float EnergyForNote(const class NoteEventInfo& noteEvent);

int ScoreForNote(const class NoteEvent& note, bool max = false);

int BSORNoteID(GlobalNamespace::NoteData* note);
int BSORNoteID(NoteEventInfo note);

struct MapPreview {
    float energy;
    int combo;
    int score;
    int maxScore;
};

MapPreview MapAtTime(const ReplayWrapper& replay, float time);

bool IsButtonDown(const class Button& button);

int IsButtonDown(const class ButtonPair& button);

void PlayDing();
