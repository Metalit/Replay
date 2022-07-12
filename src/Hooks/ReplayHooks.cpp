#include "Main.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/AudioTimeSyncController.hpp"

// keep song time and current frame up to date
MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {

    if(Manager::replaying && !Manager::paused)
        Manager::UpdateTime(self->songTime);

    AudioTimeSyncController_Update(self);
}

#include "GlobalNamespace/PauseMenuManager.hpp"

// handle pause and resume
MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu, &PauseMenuManager::ShowMenu, void, PauseMenuManager* self) {
    Manager::paused = true;
    PauseMenuManager_ShowMenu(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_HandleResumeFromPauseAnimationDidFinish, &PauseMenuManager::HandleResumeFromPauseAnimationDidFinish, void, PauseMenuManager* self) {
    Manager::paused = false;
    PauseMenuManager_HandleResumeFromPauseAnimationDidFinish(self);
}

#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "UnityEngine/Transform.hpp"

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

#include "GlobalNamespace/PlayerTransforms.hpp"

// set head position
MAKE_HOOK_MATCH(PlayerTransforms_Update, &PlayerTransforms::Update, void, PlayerTransforms* self) {

    if(Manager::replaying) {
        auto& transform = Manager::GetFrame();
        self->headTransform->set_rotation(transform.head.rotation);
        self->headTransform->set_position(transform.head.position);
    }
    PlayerTransforms_Update(self);
}

#include "GlobalNamespace/HapticFeedbackController.hpp"

// disable vibrations during replays
MAKE_HOOK_MATCH(HapticFeedbackController_PlayHapticFeedback, &HapticFeedbackController::PlayHapticFeedback,
        void, HapticFeedbackController* self, UnityEngine::XR::XRNode node, Libraries::HM::HMLib::VR::HapticPresetSO* hapticPreset) {

    if(!Manager::replaying)
        HapticFeedbackController_PlayHapticFeedback(self, node, hapticPreset);
}

#include "GlobalNamespace/NoteController.hpp"

// disable misses for cases in both replay types
MAKE_HOOK_MATCH(NoteController_HandleNoteDidPassMissedMarkerEvent, &NoteController::HandleNoteDidPassMissedMarkerEvent, void, NoteController* self) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;
        
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        return;

    NoteController_HandleNoteDidPassMissedMarkerEvent(self);
}

#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"

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

#include "GlobalNamespace/CoreGameHUDController.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/GameObject.hpp"
#include "questui/shared/BeatSaberUI.hpp"

MAKE_HOOK_MATCH(CoreGameHUDController_Start, &GlobalNamespace::CoreGameHUDController::Start, void, GlobalNamespace::CoreGameHUDController* self) {
    CoreGameHUDController_Start(self);

    if(Manager::replaying && Manager::currentReplay.replay->info.playerName != std::nullopt) {
        auto comboPanel = UnityEngine::GameObject::Find("ComboPanel");

        IPreviewBeatmapLevel* levelData = reinterpret_cast<IPreviewBeatmapLevel*>(Manager::beatmap->get_level());
        auto songName = (std::string)levelData->get_songName();
        auto mapper = (std::string)levelData->get_levelAuthorName();

        auto text = QuestUI::BeatSaberUI::CreateText(comboPanel->get_transform(), 
        "<color=red>REPLAY</color>    " + 
        mapper + " - " + songName + 
        "    Player: " + Manager::currentReplay.replay->info.playerName.value(), {300, 140});
        text->set_fontSize(20);
        text->set_alignment(TMPro::TextAlignmentOptions::Center);
    }
}

HOOK_FUNC(
    INSTALL_HOOK(logger, AudioTimeSyncController_Update);
    INSTALL_HOOK(logger, PauseMenuManager_ShowMenu);
    INSTALL_HOOK(logger, PauseMenuManager_HandleResumeFromPauseAnimationDidFinish);
    INSTALL_HOOK(logger, Saber_ManualUpdate);
    INSTALL_HOOK(logger, PlayerTransforms_Update);
    INSTALL_HOOK(logger, HapticFeedbackController_PlayHapticFeedback);
    INSTALL_HOOK(logger, NoteController_HandleNoteDidPassMissedMarkerEvent);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish);
    INSTALL_HOOK(logger, PauseMenuManager_MenuButtonPressed);
    INSTALL_HOOK(logger, CoreGameHUDController_Start);
)
