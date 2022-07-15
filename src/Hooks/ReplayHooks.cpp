#include "Main.hpp"
#include "Hooks.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"

SafePtr<GameplayModifiers> modifierHolder{};
PlayerSpecificSettings* playerSpecificSettings = nullptr;
bool wasLeftHanded = false;

// set modifiers on replay start
MAKE_HOOK_MATCH(MenuTransitionsHelper_StartStandardLevel, static_cast<void(MenuTransitionsHelper::*)(StringW, IDifficultyBeatmap*, IPreviewBeatmapLevel*, OverrideEnvironmentSettings*, ColorScheme*, GameplayModifiers*, PlayerSpecificSettings*, PracticeSettings*, StringW, bool, bool, System::Action*, System::Action_1<Zenject::DiContainer*>*, System::Action_2<StandardLevelScenesTransitionSetupDataSO*, LevelCompletionResults*>*)>(&MenuTransitionsHelper::StartStandardLevel),
        void, MenuTransitionsHelper* self, StringW f1, IDifficultyBeatmap* f2, IPreviewBeatmapLevel* f3, OverrideEnvironmentSettings* f4, ColorScheme* f5, GameplayModifiers* f6, PlayerSpecificSettings* f7, PracticeSettings* f8, StringW f9, bool f10, bool f11, System::Action* f12, System::Action_1<Zenject::DiContainer*>* f13, System::Action_2<StandardLevelScenesTransitionSetupDataSO*, LevelCompletionResults*>* f14) {
    
    if(Manager::replaying) {
        const auto& modifiers = Manager::currentReplay.replay->info.modifiers;
        auto energyType = modifiers.fourLives ? GameplayModifiers::EnergyType::Battery : GameplayModifiers::EnergyType::Bar;
        bool noFail = modifiers.noFail;
        bool instaFail = modifiers.oneLife;
        bool saberClash = false;
        auto obstacleType = modifiers.noObstacles ? GameplayModifiers::EnabledObstacleType::NoObstacles : GameplayModifiers::EnabledObstacleType::All;
        bool noBombs = modifiers.noBombs;
        bool fastNotes = false;
        bool strictAngles = modifiers.strictAngles;
        bool disappearingArrows = modifiers.disappearingArrows;
        auto songSpeed = modifiers.superFastSong ? GameplayModifiers::SongSpeed::SuperFast : (
            modifiers.fasterSong ? GameplayModifiers::SongSpeed::Faster : (
            modifiers.slowerSong ? GameplayModifiers::SongSpeed::Slower : GameplayModifiers::SongSpeed::Normal));
        bool noArrows = modifiers.noArrows;
        bool ghostNotes = modifiers.ghostNotes;
        bool proMode = modifiers.proMode;
        bool zenMode = false;
        bool smallNotes = modifiers.smallNotes;
        modifierHolder.emplace(CRASH_UNLESS(il2cpp_utils::New<GameplayModifiers*>(energyType, noFail, instaFail, saberClash, obstacleType, noBombs, fastNotes, strictAngles, disappearingArrows, songSpeed, noArrows, ghostNotes, proMode, zenMode, smallNotes)));
        f6 = static_cast<GameplayModifiers*>(modifierHolder);
        playerSpecificSettings = f7;
        wasLeftHanded = f7->leftHanded;
        f7->leftHanded = modifiers.leftHanded;
    }
    MenuTransitionsHelper_StartStandardLevel(self, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14);
}

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
    
    if(Manager::replaying && playerSpecificSettings)
        playerSpecificSettings->leftHanded = wasLeftHanded;

    SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish(self, standardLevelScenesTransitionSetupData, levelCompletionResults);
    
    if(Manager::replaying && levelCompletionResults->levelEndAction != LevelCompletionResults::LevelEndAction::Restart)
        Manager::ReplayEnded();
    else
        Manager::ReplayRestarted();
}
MAKE_HOOK_MATCH(PauseMenuManager_MenuButtonPressed, &PauseMenuManager::MenuButtonPressed, void, PauseMenuManager* self) {

    if(Manager::replaying && playerSpecificSettings)
        playerSpecificSettings->leftHanded = wasLeftHanded;

    PauseMenuManager_MenuButtonPressed(self);
    
    if(Manager::replaying)
        Manager::ReplayEnded();
}

HOOK_FUNC(
    INSTALL_HOOK(logger, MenuTransitionsHelper_StartStandardLevel);
    INSTALL_HOOK(logger, AudioTimeSyncController_Update);
    INSTALL_HOOK(logger, PauseMenuManager_ShowMenu);
    INSTALL_HOOK(logger, PauseMenuManager_HandleResumeFromPauseAnimationDidFinish);
    INSTALL_HOOK(logger, Saber_ManualUpdate);
    INSTALL_HOOK(logger, PlayerTransforms_Update);
    INSTALL_HOOK(logger, HapticFeedbackController_PlayHapticFeedback);
    INSTALL_HOOK(logger, NoteController_HandleNoteDidPassMissedMarkerEvent);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish);
    INSTALL_HOOK(logger, PauseMenuManager_MenuButtonPressed);
)
