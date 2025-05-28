#include "Formats/FrameReplay.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "hooks.hpp"
#include "main.hpp"
#include "manager.hpp"

using namespace GlobalNamespace;

// override score
MAKE_AUTO_HOOK_MATCH(ScoreController_LateUpdate, &ScoreController::LateUpdate, void, ScoreController* self) {

    ScoreController_LateUpdate(self);

    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && Manager::Frames::AllowScoreOverride()) {
        auto frame = Manager::Frames::GetScoreFrame();
        if (self->_multipliedScore != frame->score) {
            self->_multipliedScore = frame->score;
            if (frame->percent > 0)
                self->_immediateMaxPossibleMultipliedScore = frame->score / frame->percent;
            float multiplier = self->_gameplayModifiersModel->GetTotalMultiplier(self->_gameplayModifierParams, frame->energy);
            self->_modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(frame->score, multiplier);
            self->_immediateMaxPossibleModifiedScore =
                ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->immediateMaxPossibleMultipliedScore, multiplier);
            if (!self->scoreDidChangeEvent->Equals(nullptr))
                self->scoreDidChangeEvent->Invoke(frame->score, self->modifiedScore);
        }
    }
}

// override combo
MAKE_AUTO_HOOK_MATCH(
    ComboController_HandlePlayerHeadDidEnterObstacles, &ComboController::HandlePlayerHeadDidEnterObstacles, void, ComboController* self
) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        return;

    ComboController_HandlePlayerHeadDidEnterObstacles(self);
}

MAKE_AUTO_HOOK_MATCH(
    ComboController_HandleNoteWasCut,
    &ComboController::HandleNoteWasCut,
    void,
    ComboController* self,
    NoteController* noteController,
    ByRef<NoteCutInfo> noteCutInfo
) {
    bool speedOK = noteCutInfo->speedOK;
    bool directionOK = noteCutInfo->directionOK;
    bool saberTypeOK = noteCutInfo->saberTypeOK;
    bool wasCutTooSoon = noteCutInfo->wasCutTooSoon;

    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !(Manager::currentReplay.type & ReplayType::Event)) {
        if (!Manager::Frames::AllowComboDrop()) {
            self->_combo = Manager::Frames::GetScoreFrame()->combo - 1;
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
MAKE_AUTO_HOOK_MATCH(
    ComboController_HandleNoteWasMissed, &ComboController::HandleNoteWasMissed, void, ComboController* self, NoteController* noteController
) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop()) {
        self->_combo = Manager::Frames::GetScoreFrame()->combo;
        if (!self->comboDidChangeEvent->Equals(nullptr))
            self->comboDidChangeEvent->Invoke(self->_combo);
        return;
    }
    ComboController_HandleNoteWasMissed(self, noteController);
}

#include "GlobalNamespace/GameEnergyCounter.hpp"

// override energy
MAKE_AUTO_HOOK_MATCH(
    GameEnergyCounter_ProcessEnergyChange, &GameEnergyCounter::ProcessEnergyChange, void, GameEnergyCounter* self, float energyChange
) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame) {
        float energy = Manager::Frames::GetScoreFrame()->energy;
        if (self->energyType != GameplayModifiers::EnergyType::Bar) {
            self->energy = energy;
            if (energy == 0)
                energyChange = -1;
            else
                energyChange = 0;
        } else
            energyChange = energy - self->energy;
    }
    GameEnergyCounter_ProcessEnergyChange(self, energyChange);
}
