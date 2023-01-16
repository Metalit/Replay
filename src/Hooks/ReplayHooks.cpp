#include "Main.hpp"
#include "Hooks.hpp"
#include "Config.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/VRCenterAdjust.hpp"
#include "GlobalNamespace/Vector3SO.hpp"
#include "GlobalNamespace/FloatSO.hpp"
#include "UnityEngine/Resources.hpp"

SafePtr<GameplayModifiers> modifierHolder{};
PlayerSpecificSettings* playerSpecificSettings = nullptr;
bool wasLeftHanded = false;
VRCenterAdjust* roomAdjust = nullptr;
Vector3 oldPosAdj;
float oldRotAdj;

// set modifiers on replay start
MAKE_HOOK_MATCH(MenuTransitionsHelper_StartStandardLevel, static_cast<void(MenuTransitionsHelper::*)(StringW, IDifficultyBeatmap*, IPreviewBeatmapLevel*, OverrideEnvironmentSettings*, ColorScheme*, GameplayModifiers*, PlayerSpecificSettings*, PracticeSettings*, StringW, bool, bool, System::Action*, System::Action_1<Zenject::DiContainer*>*, System::Action_2<StandardLevelScenesTransitionSetupDataSO*, LevelCompletionResults*>*, System::Action_2<LevelScenesTransitionSetupDataSO*, LevelCompletionResults*>*)>(&MenuTransitionsHelper::StartStandardLevel),
        void, MenuTransitionsHelper* self, StringW f1, IDifficultyBeatmap* f2, IPreviewBeatmapLevel* f3, OverrideEnvironmentSettings* f4, ColorScheme* f5, GameplayModifiers* f6, PlayerSpecificSettings* f7, PracticeSettings* f8, StringW f9, bool f10, bool f11, System::Action* f12, System::Action_1<Zenject::DiContainer*>* f13, System::Action_2<StandardLevelScenesTransitionSetupDataSO*, LevelCompletionResults*>* f14, System::Action_2<LevelScenesTransitionSetupDataSO*, LevelCompletionResults*>* f15) {

    if(Manager::replaying) {
        auto& info = Manager::GetCurrentInfo();
        const auto& modifiers = info.modifiers;
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
        modifierHolder.emplace(GameplayModifiers::New_ctor(energyType, noFail, instaFail, saberClash, obstacleType, noBombs, fastNotes, strictAngles, disappearingArrows, songSpeed, noArrows, ghostNotes, proMode, zenMode, smallNotes));
        f6 = static_cast<GameplayModifiers*>(modifierHolder);
        playerSpecificSettings = f7;
        wasLeftHanded = f7->leftHanded;
        f7->leftHanded = modifiers.leftHanded;

        // setting the local transform doesn't work for some reason, even after the scene change
        roomAdjust = UnityEngine::Resources::FindObjectsOfTypeAll<VRCenterAdjust*>().First();
        oldPosAdj = roomAdjust->roomCenter->get_value();
        oldRotAdj = roomAdjust->roomRotation->get_value();
        roomAdjust->ResetRoom();

        if(info.practice)
            f8 = PracticeSettings::New_ctor(info.startTime, info.speed);
    }
    MenuTransitionsHelper_StartStandardLevel(self, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15);
}

#include "GlobalNamespace/AudioTimeSyncController.hpp"

// keep song time and current frame up to date, plus controller inputs
MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {

    if(Manager::replaying && !Manager::paused) {
        Manager::UpdateTime(self->songTime);
        Manager::CheckInputs();
    }
    AudioTimeSyncController_Update(self);
}

#include "GlobalNamespace/PauseMenuManager.hpp"

// handle pause, resume, and restart
MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu_Replay, &PauseMenuManager::ShowMenu, void, PauseMenuManager* self) {
    if(Manager::replaying)
        Manager::ReplayPaused();
    PauseMenuManager_ShowMenu_Replay(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_HandleResumeFromPauseAnimationDidFinish_Replay, &PauseMenuManager::HandleResumeFromPauseAnimationDidFinish, void, PauseMenuManager* self) {
    if(Manager::replaying)
        Manager::ReplayUnpaused();
    PauseMenuManager_HandleResumeFromPauseAnimationDidFinish_Replay(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_RestartButtonPressed, &PauseMenuManager::RestartButtonPressed, void, PauseMenuManager* self) {
    if(Manager::replaying)
        Manager::ReplayRestarted();
    PauseMenuManager_RestartButtonPressed(self);
}

#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "UnityEngine/Transform.hpp"

// set saber positions
MAKE_HOOK_MATCH(Saber_ManualUpdate, &Saber::ManualUpdate, void, Saber* self) {

    if(Manager::replaying) {
        auto saberTransform = self->get_transform()->GetParent();
        int saberType = (int) self->get_saberType();

        const Transform *transform, *nextTransform = nullptr;
        if(saberType == 0) {
            transform = &Manager::GetFrame().leftHand;
            if(Manager::GetFrameProgress() > 0)
                nextTransform = &Manager::GetNextFrame().leftHand;
        } else {
            transform = &Manager::GetFrame().rightHand;
            if(Manager::GetFrameProgress() > 0)
                nextTransform = &Manager::GetNextFrame().rightHand;
        }
        Quaternion rot = transform->rotation;
        Vector3 pos = transform->position;
        if(nextTransform) {
            rot = Quaternion::Lerp(rot, nextTransform->rotation, Manager::GetFrameProgress());
            pos = Vector3::Lerp(pos, nextTransform->position, Manager::GetFrameProgress());
        }
        if(Manager::GetCurrentInfo().positionsAreLocal) {
            saberTransform->set_localRotation(rot);
            saberTransform->set_localPosition(pos);
        } else {
            saberTransform->set_rotation(rot);
            saberTransform->set_position(pos);
        }
    }
    Saber_ManualUpdate(self);
}

#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/TransformExtensions.hpp"

// set head position
MAKE_HOOK_MATCH(PlayerTransforms_Update_Replay, &PlayerTransforms::Update, void, PlayerTransforms* self) {

    PlayerTransforms_Update_Replay(self);

    if(Manager::replaying) {
        auto& transform = Manager::GetFrame();
        auto targetRot = transform.head.rotation;
        auto targetPos = transform.head.position;
        auto originParent = self->originParentTransform;
        // both world pos and pseudo local pos are used in other places
        if(Manager::GetCurrentInfo().positionsAreLocal) {
            self->headPseudoLocalRot = targetRot;
            self->headPseudoLocalPos = targetPos;
            if(originParent) {
                self->headWorldRot = Sombrero::QuaternionMultiply(originParent->get_rotation(), targetRot);
                self->headWorldPos = targetPos + originParent->get_position();
            } else {
                auto headParent = self->headTransform->GetParent();
                self->headWorldRot = Sombrero::QuaternionMultiply(headParent->get_rotation(), targetRot);
                self->headWorldPos = targetPos + headParent->get_position();
            }
        } else {
            self->headWorldRot = targetRot;
            self->headWorldPos = targetPos;
            if(originParent) {
                self->headPseudoLocalRot = TransformExtensions::InverseTransformRotation(originParent, targetRot);
                self->headPseudoLocalPos = targetPos - originParent->get_position();
            } else {
                auto headParent = self->headTransform->GetParent();
                self->headPseudoLocalRot = TransformExtensions::InverseTransformRotation(headParent, targetRot);
                self->headPseudoLocalPos = targetPos - headParent->get_position();
            }
        }
    }
}

#include "GlobalNamespace/HapticFeedbackController.hpp"

// disable vibrations during replays
MAKE_HOOK_MATCH(HapticFeedbackController_PlayHapticFeedback, &HapticFeedbackController::PlayHapticFeedback,
        void, HapticFeedbackController* self, UnityEngine::XR::XRNode node, Libraries::HM::HMLib::VR::HapticPresetSO* hapticPreset) {

    if(!Manager::replaying)
        HapticFeedbackController_PlayHapticFeedback(self, node, hapticPreset);
}

#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"

// watch for level ending
MAKE_HOOK_MATCH(SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish, &SinglePlayerLevelSelectionFlowCoordinator::HandleStandardLevelDidFinish,
        void, SinglePlayerLevelSelectionFlowCoordinator* self, StandardLevelScenesTransitionSetupDataSO* standardLevelScenesTransitionSetupData, LevelCompletionResults* levelCompletionResults) {

    SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish(self, standardLevelScenesTransitionSetupData, levelCompletionResults);

    if(Manager::replaying) {
        if(playerSpecificSettings)
            playerSpecificSettings->leftHanded = wasLeftHanded;
        playerSpecificSettings = nullptr;
        if(roomAdjust) {
            roomAdjust->roomCenter->set_value(oldPosAdj);
            roomAdjust->roomRotation->set_value(oldRotAdj);
        }
        roomAdjust = nullptr;
        if(levelCompletionResults->levelEndAction != LevelCompletionResults::LevelEndAction::Restart) {
            auto viewController = self->mainScreenViewControllers->get_Item(self->mainScreenViewControllers->get_Count() - 1);
            self->DismissViewController(viewController, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, true);
            Manager::ReplayEnded();
        }
    }
}

HOOK_FUNC(
    INSTALL_HOOK(logger, MenuTransitionsHelper_StartStandardLevel);
    INSTALL_HOOK(logger, AudioTimeSyncController_Update);
    INSTALL_HOOK(logger, PauseMenuManager_ShowMenu_Replay);
    INSTALL_HOOK(logger, PauseMenuManager_HandleResumeFromPauseAnimationDidFinish_Replay);
    INSTALL_HOOK(logger, PauseMenuManager_RestartButtonPressed);
    INSTALL_HOOK(logger, Saber_ManualUpdate);
    INSTALL_HOOK(logger, PlayerTransforms_Update_Replay);
    INSTALL_HOOK(logger, HapticFeedbackController_PlayHapticFeedback);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish);
)
