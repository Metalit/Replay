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

    void GetSSPlayerName(std::string id, std::function<void(std::optional<std::string>)> callback);

    std::string GetDifficultyName(GlobalNamespace::BeatmapDifficulty difficulty);
    std::string GetDifficultyName(int difficulty);

    GlobalNamespace::BeatmapCharacteristicSO* GetCharacteristic(std::string serializedName);
    std::string GetCharacteristicName(GlobalNamespace::BeatmapCharacteristicSO* characteristic);
    std::string GetCharacteristicName(std::string serializedName);

    std::string GetMapString();

    std::string GetStatusString(Replay::Info const& info, bool color = false, float songLength = -1);

    std::string GetModifierString(Replay::Modifiers const& modifiers, bool includeNoFail);

    GlobalNamespace::NoteCutInfo
    GetNoteCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber, Replay::Events::CutInfo const& info);
    GlobalNamespace::NoteCutInfo GetBombCutInfo(GlobalNamespace::NoteController* note, GlobalNamespace::Saber* saber);

    bool IsLeft(Replay::Events::Note const& note, bool hasBombCutInfo);

    float EnergyForNote(Replay::Events::NoteInfo const& note, bool oldScoringType);
    std::array<int, 4> ScoreForNote(Replay::Events::Note const& note, bool max = false);

    int BSORNoteID(Replay::Events::NoteInfo const& note);
    bool Matches(GlobalNamespace::NoteData* data, Replay::Events::NoteInfo const& info, bool oldScoringTypes = false, bool checkME = false);

    bool IsButtonDown(Button const& button);
    int IsButtonDown(ButtonPair const& button);

    void PlayDing();
}
