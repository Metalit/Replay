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
#include "UnityEngine/Sprite.hpp"
#include "replay.hpp"

std::string GetReqlaysPath();

std::string GetBSORsPath();

std::string GetSSReplaysPath();

std::string GetDifficultyName(GlobalNamespace::BeatmapDifficulty difficulty);

std::string GetDifficultyName(int difficulty);

GlobalNamespace::BeatmapCharacteristicSO* GetCharacteristic(std::string serializedName);

std::string GetCharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic);

std::string GetCharacteristicName(std::string serializedName);

std::string GetMapString();

std::string GetModifierString(Replay::Modifiers const& modifiers, bool includeNoFail);

GlobalNamespace::NoteCutInfo
GetNoteCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber, const class ReplayNoteCutInfo& info);

GlobalNamespace::NoteCutInfo GetBombCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber);

float ModifierMultiplier(Replay::Replay const& replay, bool failed);

float EnergyForNote(Replay::Events::NoteInfo const& noteEvent);

int ScoreForNote(Replay::Events::Note const& note, bool max = false);

int BSORNoteID(GlobalNamespace::NoteData* note);
int BSORNoteID(Replay::Events::NoteInfo const& note);

struct MapPreview {
    float energy;
    int combo;
    int score;
    int maxScore;
};

MapPreview MapAtTime(Replay::Replay const& replay, float time);

bool IsButtonDown(const class Button& button);

int IsButtonDown(const class ButtonPair& button);

void PlayDing();
