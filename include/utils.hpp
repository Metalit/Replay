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
#include "config.hpp"

namespace Utils {
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

    float ModifierMultiplier(Replay::Replay const& replay, bool failed);
    float EnergyForNote(Replay::Events::NoteInfo const& noteEvent);
    int ScoreForNote(Replay::Events::Note const& note, bool max = false);

    int BSORNoteID(GlobalNamespace::NoteData* note);
    int BSORNoteID(Replay::Events::NoteInfo const& note);

    bool IsButtonDown(Button const& button);
    int IsButtonDown(ButtonPair const& button);

    void PlayDing();
}
