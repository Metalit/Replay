#include "Main.hpp"
#include "Hooks.hpp"

#include "Formats/FrameReplay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "System/Action_2.hpp"

// override score
MAKE_HOOK_MATCH(ScoreController_DespawnScoringElement, &ScoreController::DespawnScoringElement, void, ScoreController* self, ScoringElement* scoringElement) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame)
        self->multipliedScore = Manager::Frames::GetScoreFrame()->score;

    ScoreController_DespawnScoringElement(self, scoringElement);
}
MAKE_HOOK_MATCH(ScoreController_LateUpdate, &ScoreController::LateUpdate, void, ScoreController* self) {
    
    ScoreController_LateUpdate(self);

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame) {
        auto frame = Manager::Frames::GetScoreFrame();
        if(self->multipliedScore != frame->score) {
            self->multipliedScore = frame->score;
            float multiplier = self->gameplayModifiersModel->GetTotalMultiplier(self->gameplayModifierParams, frame->energy);
            self->modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(frame->score, multiplier);
            self->scoreDidChangeEvent->Invoke(frame->score, self->modifiedScore);
        }
    }
}

#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "System/Action_1.hpp"

// override combo
MAKE_HOOK_MATCH(ComboController_HandlePlayerHeadDidEnterObstacles, &ComboController::HandlePlayerHeadDidEnterObstacles, void, ComboController* self) {

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame && Manager::Frames::GetScoreFrame()->combo > 0)
        return;

    ComboController_HandlePlayerHeadDidEnterObstacles(self);
}
MAKE_HOOK_MATCH(ComboController_HandleNoteWasCut, &ComboController::HandleNoteWasCut, void, ComboController* self, NoteController* noteController, ByRef<NoteCutInfo> noteCutInfo) {

    bool speedOK = noteCutInfo->speedOK;
    bool directionOK = noteCutInfo->directionOK;
    bool saberTypeOK = noteCutInfo->saberTypeOK;
    bool wasCutTooSoon = noteCutInfo->wasCutTooSoon;

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame) {
        int combo = Manager::Frames::GetScoreFrame()->combo;
        if(combo > 0) {
            self->combo = combo - 1;
            noteCutInfo->speedOK = true;
            noteCutInfo->directionOK = true;
            noteCutInfo->saberTypeOK = true;
            noteCutInfo->wasCutTooSoon = false;
        } else {
            noteCutInfo->speedOK = false;
            noteCutInfo->directionOK = false;
            noteCutInfo->saberTypeOK = false;
            noteCutInfo->wasCutTooSoon = true;
        }
    }
    ComboController_HandleNoteWasCut(self, noteController, noteCutInfo);
    
    noteCutInfo->speedOK = speedOK;
    noteCutInfo->directionOK = directionOK;
    noteCutInfo->saberTypeOK = saberTypeOK;
    noteCutInfo->wasCutTooSoon = wasCutTooSoon;
}
MAKE_HOOK_MATCH(ComboController_HandleNoteWasMissed, &ComboController::HandleNoteWasMissed, void, ComboController* self, NoteController* noteController) {
    
    auto gameplayType = noteController->noteData->gameplayType;

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame) {
        int combo = Manager::Frames::GetScoreFrame()->combo;
        if(combo > 0) {
            self->combo = combo;
            noteController->noteData->gameplayType = NoteData::GameplayType::Bomb;
            self->comboDidChangeEvent->Invoke(combo);
        }
    }
    ComboController_HandleNoteWasMissed(self, noteController);
    
    noteController->noteData->gameplayType = gameplayType;
}

#include "GlobalNamespace/GameEnergyCounter.hpp"

// override energy
MAKE_HOOK_MATCH(GameEnergyCounter_ProcessEnergyChange, &GameEnergyCounter::ProcessEnergyChange, void, GameEnergyCounter* self, float energyChange) {

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame)
        energyChange = Manager::Frames::GetScoreFrame()->energy - self->energy;

    GameEnergyCounter_ProcessEnergyChange(self, energyChange);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, ScoreController_DespawnScoringElement);
    INSTALL_HOOK(logger, ScoreController_LateUpdate);
    INSTALL_HOOK(logger, ComboController_HandlePlayerHeadDidEnterObstacles);
    INSTALL_HOOK(logger, ComboController_HandleNoteWasCut);
    INSTALL_HOOK(logger, ComboController_HandleNoteWasMissed);
    INSTALL_HOOK(logger, GameEnergyCounter_ProcessEnergyChange);
)
