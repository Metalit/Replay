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
MAKE_HOOK_MATCH(ScoreController_LateUpdate, &ScoreController::LateUpdate, void, ScoreController* self) {

    ScoreController_LateUpdate(self);

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame) {
        auto frame = Manager::Frames::GetScoreFrame();
        if(self->multipliedScore != frame->score) {
            self->multipliedScore = frame->score;
            self->immediateMaxPossibleMultipliedScore = frame->score / frame->percent;
            float multiplier = self->gameplayModifiersModel->GetTotalMultiplier(self->gameplayModifierParams, frame->energy);
            self->modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(frame->score, multiplier);
            self->immediateMaxPossibleModifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(frame->score / frame->percent, multiplier);
            if(!self->scoreDidChangeEvent->Equals(nullptr))
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

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        return;

    ComboController_HandlePlayerHeadDidEnterObstacles(self);
}
MAKE_HOOK_MATCH(ComboController_HandleNoteWasCut, &ComboController::HandleNoteWasCut, void, ComboController* self, NoteController* noteController, ByRef<NoteCutInfo> noteCutInfo) {

    bool speedOK = noteCutInfo->speedOK;
    bool directionOK = noteCutInfo->directionOK;
    bool saberTypeOK = noteCutInfo->saberTypeOK;
    bool wasCutTooSoon = noteCutInfo->wasCutTooSoon;

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame) {
        if(!Manager::Frames::AllowComboDrop()) {
            self->combo = Manager::Frames::GetScoreFrame()->combo - 1;
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

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop()) {
        self->combo = Manager::Frames::GetScoreFrame()->combo;
        if(!self->comboDidChangeEvent->Equals(nullptr))
            self->comboDidChangeEvent->Invoke(self->combo);
        return;
    }
    ComboController_HandleNoteWasMissed(self, noteController);
}

#include "GlobalNamespace/GameEnergyCounter.hpp"

// override energy
MAKE_HOOK_MATCH(GameEnergyCounter_ProcessEnergyChange, &GameEnergyCounter::ProcessEnergyChange, void, GameEnergyCounter* self, float energyChange) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame) {
        float energy = Manager::Frames::GetScoreFrame()->energy;
        if(self->energyType != GameplayModifiers::EnergyType::Bar) {
            self->energy = energy;
            if(energy == 0)
                energyChange = -1;
            else
                energyChange = 0;
        } else
            energyChange = energy - self->energy;
    }
    GameEnergyCounter_ProcessEnergyChange(self, energyChange);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, ScoreController_LateUpdate);
    INSTALL_HOOK(logger, ComboController_HandlePlayerHeadDidEnterObstacles);
    INSTALL_HOOK(logger, ComboController_HandleNoteWasCut);
    INSTALL_HOOK(logger, ComboController_HandleNoteWasMissed);
    INSTALL_HOOK(logger, GameEnergyCounter_ProcessEnergyChange);
)
