#include "Main.hpp"
#include "ReplayHooks.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "ReplayManager.hpp"

#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "GlobalNamespace/CutScoreBuffer.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/HapticFeedbackController.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "UnityEngine/Transform.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"

using namespace GlobalNamespace;

// keep song time and current frame up to date
MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {

    if(Manager::replaying && !Manager::paused)
        Manager::UpdateTime(self->songTime);

    AudioTimeSyncController_Update(self);
}

// handle pause and resume
MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu, &PauseMenuManager::ShowMenu, void, PauseMenuManager* self) {
    Manager::paused = true;
    PauseMenuManager_ShowMenu(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_HandleResumeFromPauseAnimationDidFinish, &PauseMenuManager::HandleResumeFromPauseAnimationDidFinish, void, PauseMenuManager* self) {
    Manager::paused = false;
    PauseMenuManager_HandleResumeFromPauseAnimationDidFinish(self);
}

// set saber positions
MAKE_HOOK_MATCH(Saber_ManualUpdate, &Saber::ManualUpdate, void, Saber* self) {

    if(Manager::replaying) {
        auto saberTransform = self->get_transform();
        int saberType = (int) self->get_saberType();

        auto& transform = Manager::GetFrame();
        if(saberType == 0) {
            Quaternion leftRot = transform.leftHand.rotation;
            Vector3 leftPos = transform.leftHand.position;
            if(Manager::GetFrameProgress() > 0) {
                auto& nextTransform = Manager::GetNextFrame();
                leftRot = Quaternion::Lerp(leftRot, nextTransform.leftHand.rotation, Manager::GetFrameProgress());
                leftPos = Vector3::Lerp(leftPos, nextTransform.leftHand.position, Manager::GetFrameProgress());
            }
            saberTransform->set_rotation(leftRot);
            saberTransform->set_position(leftPos);
        } else {
            Quaternion rightRot = transform.rightHand.rotation;
            Vector3 rightPos = transform.rightHand.position;
            if(Manager::GetFrameProgress() > 0) {
                auto& nextTransform = Manager::GetNextFrame();
                rightRot = Quaternion::Lerp(rightRot, nextTransform.rightHand.rotation, Manager::GetFrameProgress());
                rightPos = Vector3::Lerp(rightPos, nextTransform.rightHand.position, Manager::GetFrameProgress());
            }
            saberTransform->set_rotation(rightRot);
            saberTransform->set_position(rightPos);
        }
    }
    Saber_ManualUpdate(self);
}

// set head position
MAKE_HOOK_MATCH(PlayerTransforms_Update, &PlayerTransforms::Update, void, PlayerTransforms* self) {

    if(Manager::replaying) {
        auto& transform = Manager::GetFrame();
        self->headTransform->set_rotation(transform.head.rotation);
        self->headTransform->set_position(transform.head.position);
    }
    PlayerTransforms_Update(self);
}

// override score for frame replays
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

// override combo for frame replays
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

// override energy for frame replays
MAKE_HOOK_MATCH(GameEnergyCounter_ProcessEnergyChange, &GameEnergyCounter::ProcessEnergyChange, void, GameEnergyCounter* self, float energyChange) {

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame)
        energyChange = Manager::Frames::GetScoreFrame()->energy - self->energy;

    GameEnergyCounter_ProcessEnergyChange(self, energyChange);
}

// disable vibrations during replays
MAKE_HOOK_MATCH(HapticFeedbackController_PlayHapticFeedback, &HapticFeedbackController::PlayHapticFeedback,
        void, HapticFeedbackController* self, UnityEngine::XR::XRNode node, Libraries::HM::HMLib::VR::HapticPresetSO* hapticPreset) {

    if(!Manager::replaying)
        HapticFeedbackController_PlayHapticFeedback(self, node, hapticPreset);
}

// watch for the end of a replay
MAKE_HOOK_MATCH(SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish, &SinglePlayerLevelSelectionFlowCoordinator::HandleStandardLevelDidFinish,
        void, SinglePlayerLevelSelectionFlowCoordinator* self, StandardLevelScenesTransitionSetupDataSO* standardLevelScenesTransitionSetupData, LevelCompletionResults* levelCompletionResults) {
    
    SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish(self, standardLevelScenesTransitionSetupData, levelCompletionResults);
    
    if(Manager::replaying && levelCompletionResults->levelEndAction != LevelCompletionResults::LevelEndAction::Restart)
        Manager::ReplayEnded();
    else
        Manager::ReplayRestarted();
}
MAKE_HOOK_MATCH(PauseMenuManager_MenuButtonPressed, &PauseMenuManager::MenuButtonPressed, void, PauseMenuManager* self) {

    PauseMenuManager_MenuButtonPressed(self);

    if(Manager::replaying)
        Manager::ReplayEnded();
}

namespace Hooks {
    void Install(Logger& logger) {
        INSTALL_HOOK(logger, AudioTimeSyncController_Update);
        INSTALL_HOOK(logger, PauseMenuManager_ShowMenu);
        INSTALL_HOOK(logger, PauseMenuManager_HandleResumeFromPauseAnimationDidFinish);
        INSTALL_HOOK(logger, Saber_ManualUpdate);
        INSTALL_HOOK(logger, PlayerTransforms_Update);
        INSTALL_HOOK(logger, ScoreController_DespawnScoringElement);
        INSTALL_HOOK(logger, ScoreController_LateUpdate);
        INSTALL_HOOK(logger, ComboController_HandlePlayerHeadDidEnterObstacles);
        INSTALL_HOOK(logger, ComboController_HandleNoteWasCut);
        INSTALL_HOOK(logger, ComboController_HandleNoteWasMissed);
        INSTALL_HOOK(logger, GameEnergyCounter_ProcessEnergyChange);
        INSTALL_HOOK(logger, HapticFeedbackController_PlayHapticFeedback);
        INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish);
        INSTALL_HOOK(logger, PauseMenuManager_MenuButtonPressed);
    }
}
