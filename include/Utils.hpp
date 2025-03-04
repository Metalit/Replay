#pragma once

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapKey.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "Replay.hpp"
#include "UnityEngine/Sprite.hpp"

std::string GetReqlaysPath();

std::string GetBSORsPath();

std::string GetSSReplaysPath();

std::string GetDifficultyName(GlobalNamespace::BeatmapDifficulty difficulty);

std::string GetDifficultyName(int difficulty);

GlobalNamespace::BeatmapCharacteristicSO* GetCharacteristic(std::string serializedName);

std::string GetCharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic);

std::string GetCharacteristicName(std::string serializedName);

struct DifficultyBeatmap {
    GlobalNamespace::BeatmapKey difficulty;
    GlobalNamespace::BeatmapLevel* level;

    DifficultyBeatmap() = default;
    DifficultyBeatmap(GlobalNamespace::BeatmapKey diff, GlobalNamespace::BeatmapLevel* lev) : difficulty(diff), level(lev) {};

    bool operator==(DifficultyBeatmap const& rhs) const {
        return level == rhs.level && GlobalNamespace::BeatmapKey::op_Equality(difficulty, rhs.difficulty);
    };
};

std::string GetMapString(DifficultyBeatmap beatmap);

std::vector<std::pair<std::string, ReplayWrapper>> GetReplays(DifficultyBeatmap beatmap);

std::string GetModifierString(ReplayModifiers const& modifiers, bool includeNoFail);

GlobalNamespace::NoteCutInfo
GetNoteCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber, const class ReplayNoteCutInfo& info);

GlobalNamespace::NoteCutInfo GetBombCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber);

float ModifierMultiplier(ReplayWrapper const& replay, bool failed);

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

MapPreview MapAtTime(ReplayWrapper const& replay, float time);

bool IsButtonDown(const class Button& button);

int IsButtonDown(const class ButtonPair& button);

void PlayDing();
