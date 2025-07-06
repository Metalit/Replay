#pragma once

#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "replay.hpp"

namespace Playback {
    void UpdateTime();

    void SeekTo(float time);

    Replay::Pose const& GetPose();

    bool DisableRealEvent(bool bad);
    bool DisableListSorting();

    void AddNoteController(GlobalNamespace::NoteController* note);
    void RemoveNoteController(GlobalNamespace::NoteController* note);

    void ProcessStart(
        GlobalNamespace::GameplayModifiers*& modifiers, GlobalNamespace::PracticeSettings*& practice, GlobalNamespace::PlayerSpecificSettings* player
    );
    void ProcessSaber(GlobalNamespace::Saber* saber);
    void ProcessScore(GlobalNamespace::ScoreController* controller);
    void ProcessMaxScore(GlobalNamespace::ScoreController* controller);
    bool ProcessEnergy(GlobalNamespace::GameEnergyCounter* counter);
}
