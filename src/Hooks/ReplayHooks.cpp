#include "Config.hpp"
#include "CustomTypes/ReplayMenu.hpp"
#include "GlobalNamespace/FloatSO.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/HapticFeedbackManager.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/MenuLightsManager.hpp"
#include "GlobalNamespace/MenuLightsPresetSO.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/PlayerSpecificSettings.hpp"
#include "GlobalNamespace/PracticeSettings.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SaberType.hpp"
#include "GlobalNamespace/SettingsManager.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/VRCenterAdjust.hpp"
#include "GlobalNamespace/VRController.hpp"
#include "HMUI/ViewController.hpp"
#include "Hooks.hpp"
#include "Main.hpp"
#include "Replay.hpp"
#include "ReplayManager.hpp"
#include "Unity/Mathematics/float3.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Transform.hpp"
#include "Utils.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "metacore/shared/game.hpp"

using namespace GlobalNamespace;

SafePtr<GameplayModifiers> modifierHolder{};
PlayerSpecificSettings* playerSpecificSettings = nullptr;
bool wasLeftHanded = false;
VRCenterAdjust* roomAdjust = nullptr;
Vector3 oldPosAdj;
float oldRotAdj;

// set modifiers on replay start
MAKE_AUTO_HOOK_MATCH(
    MenuTransitionsHelper_StartStandardLevel,
    static_cast<
        void (MenuTransitionsHelper::*)(StringW, ByRef<BeatmapKey>, BeatmapLevel*, OverrideEnvironmentSettings*, ColorScheme*, bool, ColorScheme*, GameplayModifiers*, PlayerSpecificSettings*, PracticeSettings*, EnvironmentsListModel*, StringW, bool, bool, System::Action*, System::Action_1<Zenject::DiContainer*>*, System::Action_2<UnityW<StandardLevelScenesTransitionSetupDataSO>, LevelCompletionResults*>*, System::Action_2<UnityW<StandardLevelScenesTransitionSetupDataSO>, LevelCompletionResults*>*, System::Nullable_1<RecordingToolManager::SetupData>)>(
        &MenuTransitionsHelper::StartStandardLevel
    ),
    void,
    MenuTransitionsHelper* self,
    StringW f1,
    ByRef<BeatmapKey> f2,
    BeatmapLevel* f3,
    OverrideEnvironmentSettings* f4,
    ColorScheme* f5,
    bool f6,
    ColorScheme* f7,
    GameplayModifiers* f8,
    PlayerSpecificSettings* f9,
    PracticeSettings* f10,
    EnvironmentsListModel* f11,
    StringW f12,
    bool f13,
    bool f14,
    System::Action* f15,
    System::Action_1<Zenject::DiContainer*>* f16,
    System::Action_2<UnityW<StandardLevelScenesTransitionSetupDataSO>, LevelCompletionResults*>* f17,
    System::Action_2<UnityW<StandardLevelScenesTransitionSetupDataSO>, LevelCompletionResults*>* f18,
    System::Nullable_1<RecordingToolManager::SetupData> f19
) {
    if (Manager::replaying) {
        auto& info = Manager::GetCurrentInfo();
        auto const& modifiers = info.modifiers;
        auto energyType = modifiers.fourLives ? GameplayModifiers::EnergyType::Battery : GameplayModifiers::EnergyType::Bar;
        bool noFail = modifiers.noFail;
        bool instaFail = modifiers.oneLife;
        bool saberClash = false;
        auto obstacleType = modifiers.noObstacles ? GameplayModifiers::EnabledObstacleType::NoObstacles : GameplayModifiers::EnabledObstacleType::All;
        bool noBombs = modifiers.noBombs;
        bool fastNotes = false;
        bool strictAngles = modifiers.strictAngles;
        bool disappearingArrows = modifiers.disappearingArrows;
        auto songSpeed =
            modifiers.superFastSong
                ? GameplayModifiers::SongSpeed::SuperFast
                : (modifiers.fasterSong ? GameplayModifiers::SongSpeed::Faster
                                        : (modifiers.slowerSong ? GameplayModifiers::SongSpeed::Slower : GameplayModifiers::SongSpeed::Normal));
        bool noArrows = modifiers.noArrows;
        bool ghostNotes = modifiers.ghostNotes;
        bool proMode = modifiers.proMode;
        bool zenMode = false;
        bool smallNotes = modifiers.smallNotes;
        modifierHolder = GameplayModifiers::New_ctor(
            energyType,
            noFail,
            instaFail,
            saberClash,
            obstacleType,
            noBombs,
            fastNotes,
            strictAngles,
            disappearingArrows,
            songSpeed,
            noArrows,
            ghostNotes,
            proMode,
            zenMode,
            smallNotes
        );
        f8 = static_cast<GameplayModifiers*>(modifierHolder);
        playerSpecificSettings = f9;
        wasLeftHanded = f9->leftHanded;
        f9->_leftHanded = modifiers.leftHanded;

        // setting the local transform doesn't work for some reason, even after the scene change
        roomAdjust = UnityEngine::Resources::FindObjectsOfTypeAll<VRCenterAdjust*>()->First();
        oldPosAdj = Unity::Mathematics::float3::op_Implicit___UnityEngine__Vector3(roomAdjust->_settingsManager->settings.room.center);
        oldRotAdj = roomAdjust->_settingsManager->settings.room.rotation;
        roomAdjust->ResetRoom();

        if (info.practice)
            f10 = PracticeSettings::New_ctor(info.startTime, info.speed);
    }
    MenuTransitionsHelper_StartStandardLevel(self, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19);
}

// handle restarts
MAKE_AUTO_HOOK_MATCH(PauseMenuManager_RestartButtonPressed, &PauseMenuManager::RestartButtonPressed, void, PauseMenuManager* self) {

    if (Manager::replaying)
        Manager::ReplayRestarted();

    PauseMenuManager_RestartButtonPressed(self);
}

// set saber positions
MAKE_AUTO_HOOK_MATCH(Saber_ManualUpdate, &Saber::ManualUpdate, void, Saber* self) {

    if (Manager::replaying) {
        // controller offset
        self->transform->SetLocalPositionAndRotation(Vector3::zero(), Quaternion::identity());
        auto saberTransform = self->transform->parent;
        int saberType = (int) self->saberType;

        Transform const *transform, *nextTransform = nullptr;
        if (saberType == 0) {
            transform = &Manager::GetFrame().leftHand;
            if (Manager::GetFrameProgress() > 0)
                nextTransform = &Manager::GetNextFrame().leftHand;
        } else {
            transform = &Manager::GetFrame().rightHand;
            if (Manager::GetFrameProgress() > 0)
                nextTransform = &Manager::GetNextFrame().rightHand;
        }
        Quaternion rot = transform->rotation;
        Vector3 pos = transform->position;
        if (nextTransform) {
            rot = Quaternion::Lerp(rot, nextTransform->rotation, Manager::GetFrameProgress());
            pos = Vector3::Lerp(pos, nextTransform->position, Manager::GetFrameProgress());
        }
        if (Manager::GetCurrentInfo().positionsAreLocal) {
            saberTransform->localRotation = rot;
            saberTransform->localPosition = pos;
        } else {
            saberTransform->rotation = rot;
            saberTransform->position = pos;
        }
    }
    Saber_ManualUpdate(self);
}
// make sure saber positions aren't overridden
MAKE_AUTO_HOOK_MATCH(VRController_Update, &VRController::Update, void, VRController* self) {

    if (!Manager::replaying || Manager::paused)  // I should really just disable them instead of having this hook
        VRController_Update(self);
}

// disable vibrations during replays
MAKE_AUTO_HOOK_MATCH(
    HapticFeedbackManager_PlayHapticFeedback,
    &HapticFeedbackManager::PlayHapticFeedback,
    void,
    HapticFeedbackManager* self,
    UnityEngine::XR::XRNode node,
    Libraries::HM::HMLib::VR::HapticPresetSO* hapticPreset
) {
    if (!Manager::replaying)
        HapticFeedbackManager_PlayHapticFeedback(self, node, hapticPreset);
}

static bool cancelPresent = false;
// watch for level ending
MAKE_AUTO_HOOK_MATCH(
    SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish,
    &SinglePlayerLevelSelectionFlowCoordinator::HandleStandardLevelDidFinish,
    void,
    SinglePlayerLevelSelectionFlowCoordinator* self,
    StandardLevelScenesTransitionSetupDataSO* standardLevelScenesTransitionSetupData,
    LevelCompletionResults* levelCompletionResults
) {
    if (Manager::replaying)
        cancelPresent = true;

    SinglePlayerLevelSelectionFlowCoordinator_HandleStandardLevelDidFinish(self, standardLevelScenesTransitionSetupData, levelCompletionResults);

    cancelPresent = false;
    bool quit = levelCompletionResults->levelEndAction == LevelCompletionResults::LevelEndAction::Quit;
    if (Manager::replaying) {
        MetaCore::Game::SetCameraFadeOut(MOD_ID, true, 0);
        if (playerSpecificSettings)
            playerSpecificSettings->_leftHanded = wasLeftHanded;
        playerSpecificSettings = nullptr;
        if (roomAdjust) {
            roomAdjust->_settingsManager->settings.room.center = Unity::Mathematics::float3::op_Implicit___Unity__Mathematics__float3(oldPosAdj);
            roomAdjust->_settingsManager->settings.room.rotation = oldRotAdj;
        }
        roomAdjust = nullptr;
        if (!quit) {
            auto lights = *il2cpp_utils::GetFieldValue<MenuLightsManager*>(self, "_menuLightsManager");
            lights->SetColorPreset(*il2cpp_utils::GetFieldValue<MenuLightsPresetSO*>(self, "_defaultLightsPreset"), false, 0);
        }
        BSML::MainThreadScheduler::ScheduleUntil(
            []() { return Manager::Camera::muxingFinished; },
            [quit]() {
                MetaCore::Game::SetCameraFadeOut(MOD_ID, false);
                Manager::ReplayEnded(quit);
            }
        );
    } else if (!quit && !recorderInstalled)
        Menu::SetButtonUninteractable("Install BeatLeader or ScoreSaber to record replays!");
}

MAKE_AUTO_HOOK_MATCH(
    FlowCoordinator_PresentViewController,
    &HMUI::FlowCoordinator::PresentViewController,
    void,
    HMUI::FlowCoordinator* self,
    HMUI::ViewController* viewController,
    System::Action* finishedCallback,
    HMUI::ViewController::AnimationDirection animationDirection,
    bool immediately
) {
    if (!cancelPresent)
        FlowCoordinator_PresentViewController(self, viewController, finishedCallback, animationDirection, immediately);
}
