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
#include "config.hpp"
#include "replay.hpp"

namespace Utils {
    bool LowerVersion(std::string version, std::string compare);

    std::string GetDifficultyName(GlobalNamespace::BeatmapDifficulty difficulty);
    std::string GetDifficultyName(int difficulty);

    GlobalNamespace::BeatmapCharacteristicSO* GetCharacteristic(std::string serializedName);
    std::string GetCharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic);
    std::string GetCharacteristicName(std::string serializedName);

    std::string GetMapString();

    std::string GetModifierString(Replay::Modifiers const& modifiers, bool includeNoFail);

    GlobalNamespace::NoteCutInfo
    GetNoteCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber, Replay::Events::CutInfo const& info);
    GlobalNamespace::NoteCutInfo GetBombCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber);

    bool ScoringTypeMatches(int replayType, GlobalNamespace::NoteData::ScoringType noteType, bool oldScoringType);

    float EnergyForNote(Replay::Events::NoteInfo const& note, bool oldScoringType);
    float AccuracyForDistance(float distance);
    std::array<int, 4> ScoreForNote(Replay::Events::Note const& note, bool oldScoringType, bool max = false);

    int BSORNoteID(GlobalNamespace::NoteData* note);
    int BSORNoteID(Replay::Events::NoteInfo const& note);

    bool IsButtonDown(Button const& button);
    int IsButtonDown(ButtonPair const& button);

    void PlayDing();
}
