#include "beatsaber-hook/shared/binary.hpp"
#include "hooks.hpp"

#include <GLES3/gl3.h>

#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/ConditionalActivation.hpp"
#include "GlobalNamespace/DeactivateVRControllersOnFocusCapture.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/GameSongController.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/ListExtensions.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/NoteBasicCutInfoHelper.hpp"
#include "GlobalNamespace/ObstacleMaterialSetter.hpp"
#include "GlobalNamespace/PauseController.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/ShockwaveEffect.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "UnityEngine/Renderer.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "manager.hpp"
#include "pause.hpp"
#include "playback.hpp"

using namespace GlobalNamespace;


static bool disableObstacleEntry = false;

// TODO: move to utils
static bool IsBadCut(NoteController* note, Saber* saber, Vector3 cutDirVec, float angle) {
    bool directionOK;
    bool speedOK;
    bool saberTypeOK;
    float cutDirDeviation;
    float cutDirAngle;
    NoteBasicCutInfoHelper::GetBasicCutInfo(
        note->_noteTransform,
        note->_noteData->colorType,
        note->_noteData->cutDirection,
        saber->saberType,
        saber->bladeSpeedForLogic,
        cutDirVec,
        angle,
        by_ref(directionOK),
        by_ref(speedOK),
        by_ref(saberTypeOK),
        by_ref(cutDirDeviation),
        by_ref(cutDirAngle)
    );
    return !directionOK || !speedOK || !saberTypeOK;
}

// disable real bomb cuts
MAKE_AUTO_HOOK_MATCH(
    BombNoteController_HandleWasCutBySaber,
    &BombNoteController::HandleWasCutBySaber,
    void,
    BombNoteController* self,
    Saber* saber,
    UnityEngine::Vector3 cutPoint,
    UnityEngine::Quaternion orientation,
    UnityEngine::Vector3 cutDirVec
) {
    if (!Playback::DisableRealEvent(true))
        BombNoteController_HandleWasCutBySaber(self, saber, cutPoint, orientation, cutDirVec);
}

// disable real note cuts
MAKE_AUTO_HOOK_MATCH(
    GameNoteController_HandleCut,
    &GameNoteController::HandleCut,
    void,
    GameNoteController* self,
    Saber* saber,
    UnityEngine::Vector3 cutPoint,
    UnityEngine::Quaternion orientation,
    UnityEngine::Vector3 cutDirVec,
    bool allowBadCut
) {
    if (!Playback::DisableRealEvent(allowBadCut && IsBadCut(self, saber, cutDirVec, self->_cutAngleTolerance)))
        GameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}

// disable real chain cuts
MAKE_AUTO_HOOK_MATCH(
    BurstSliderGameNoteController_HandleCut,
    &BurstSliderGameNoteController::HandleCut,
    void,
    BurstSliderGameNoteController* self,
    Saber* saber,
    UnityEngine::Vector3 cutPoint,
    UnityEngine::Quaternion orientation,
    UnityEngine::Vector3 cutDirVec,
    bool allowBadCut
) {
    if (!Playback::DisableRealEvent(allowBadCut && IsBadCut(self, saber, cutDirVec, 360)))
        BurstSliderGameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}

// disable real misses
MAKE_AUTO_HOOK_MATCH(
    NoteController_HandleNoteDidPassMissedMarkerEvent, &NoteController::HandleNoteDidPassMissedMarkerEvent, void, NoteController* self
) {
    if (!Playback::DisableRealEvent(true))
        NoteController_HandleNoteDidPassMissedMarkerEvent(self);
}

// disable real obstacle interactions
MAKE_AUTO_HOOK_MATCH(
    PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles,
    &PlayerHeadAndObstacleInteraction::RefreshIntersectingObstacles,
    void,
    PlayerHeadAndObstacleInteraction* self,
    UnityEngine::Vector3 worldPos
) {
    if (!Playback::DisableRealEvent(false)) {
        disableObstacleEntry = Playback::DisableRealEvent(true);
        PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles(self, worldPos);
        disableObstacleEntry = false;
    }
}

// optionally only disable entry into obstacles
#define HashSet_Contains_MInfo \
    ::i2c::find_method(::i2c::class_of<System::Collections::Generic::HashSet_1<ObstacleController*>*>(), {"Contains", 1})

MAKE_AUTO_HOOK_FIND(
    HashSet_Contains, HashSet_Contains_MInfo, bool, System::Collections::Generic::HashSet_1<ObstacleController*>* self, ObstacleController* item
) {
    return disableObstacleEntry ? true : HashSet_Contains(self, item);
}

// disable resorting for replayed events
// (regular FindMethod still doesn't work for methods with a nested generic type)
#define ListExtensions_InsertIntoSortedListFromEnd_MInfo \
    ::i2c::make_generic(                                    \
        ::i2c::find_method(::i2c::class_of<ListExtensions*>(), {"InsertIntoSortedListFromEnd", 2}), {::i2c::class_of<ScoringElement*>()})

MAKE_AUTO_HOOK_FIND(
    ListExtensions_InsertIntoSortedListFromEnd,
    ListExtensions_InsertIntoSortedListFromEnd_MInfo,
    void,
    System::Collections::Generic::List_1<ScoringElement*>* list,
    ScoringElement* item
) {
    if (!Playback::DisableListSorting())
        ListExtensions_InsertIntoSortedListFromEnd(list, item);
    else
        list->Add(item);
}

// ensure energy changes for obstacles are accurate
MAKE_AUTO_HOOK_MATCH(GameEnergyCounter_LateUpdate, &GameEnergyCounter::LateUpdate, void, GameEnergyCounter* self) {
    if (Playback::ProcessEnergy(self))
        GameEnergyCounter_LateUpdate(self);
}

// override score
MAKE_AUTO_HOOK_MATCH(ScoreController_LateUpdate, &ScoreController::LateUpdate, void, ScoreController* self) {
    ScoreController_LateUpdate(self);
    Playback::ProcessScore(self);
}

// override max score
MAKE_AUTO_HOOK_MATCH(
    ScoreController_DespawnScoringElement, &ScoreController::DespawnScoringElement, void, ScoreController* self, ScoringElement* scoringElement
) {
    // modified scores are calculated based on max scores after this function is called
    Playback::ProcessMaxScore(self);
    ScoreController_DespawnScoringElement(self, scoringElement);
}

// keep track of all notes
MAKE_AUTO_HOOK_MATCH(
    BeatmapObjectManager_AddSpawnedNoteController,
    &BeatmapObjectManager::AddSpawnedNoteController,
    void,
    BeatmapObjectManager* self,
    NoteController* noteController,
    NoteSpawnData noteSpawnData
) {
    BeatmapObjectManager_AddSpawnedNoteController(self, noteController, noteSpawnData);
    Playback::AddNoteController(noteController);
}

MAKE_AUTO_HOOK_MATCH(
    BeatmapObjectManager_DespawnNoteController, &BeatmapObjectManager::Despawn, void, BeatmapObjectManager* self, NoteController* noteController
) {
    BeatmapObjectManager_DespawnNoteController(self, noteController);
    Playback::RemoveNoteController(noteController);
}

// set modifiers on replay start
MAKE_AUTO_HOOK_MATCH(MenuTransitionsHelper_StartStandardLevel, &GlobalNamespace::MenuTransitionsHelper::StartStandardLevel, void,
                GlobalNamespace::MenuTransitionsHelper* self,
                ::GlobalNamespace::GameMode gameMode,
                ::by_ref<::GlobalNamespace::BeatmapKey const> beatmapKey,
                ::GlobalNamespace::BeatmapLevel* beatmapLevel,
                ::GlobalNamespace::OverrideEnvironmentSettings* overrideEnvironmentSettings,
                ::GlobalNamespace::ColorScheme* playerOverrideColorScheme,
                bool playerOverrideLightshowColors,
                ::GlobalNamespace::GameplayModifiers* gameplayModifiers,
                ::GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings,
                ::GlobalNamespace::PracticeSettings* practiceSettings,
                ::GlobalNamespace::EnvironmentsListModel* environmentsListModel,
                ::GlobalNamespace::GameplayAdditionalInformation* gameplayAdditionalInformation,
                ::System::Action* beforeSceneSwitchToGameplayCallback,
                ::System::Action_1<::Zenject::DiContainer*>* afterSceneSwitchToGameplayCallback,
                ::System::Action_2<::GlobalNamespace::StandardLevelScenesTransitionSetupData*, ::GlobalNamespace::LevelCompletionResults*>* levelFinishedCallback,
                ::System::Action_2<::GlobalNamespace::StandardLevelScenesTransitionSetupData*, ::GlobalNamespace::LevelCompletionResults*>* levelRestartedCallback,
                ::GlobalNamespace::IBeatmapLevelData* beatmapLevelData) {
    Playback::ProcessStart(gameplayModifiers, practiceSettings, playerSpecificSettings);
    MenuTransitionsHelper_StartStandardLevel(self, gameMode, beatmapKey, beatmapLevel, overrideEnvironmentSettings, playerOverrideColorScheme, playerOverrideLightshowColors, gameplayModifiers, playerSpecificSettings, practiceSettings, environmentsListModel, gameplayAdditionalInformation, beforeSceneSwitchToGameplayCallback, afterSceneSwitchToGameplayCallback, levelFinishedCallback, levelRestartedCallback, beatmapLevelData);
}

// set saber positions
MAKE_AUTO_HOOK_MATCH(Saber_ManualUpdate, &Saber::ManualUpdate, void, Saber* self) {
    Playback::ProcessSaber(self);
    Saber_ManualUpdate(self);
}

// force sabers enabled in replays
MAKE_AUTO_HOOK_MATCH(
    DeactivateVRControllersOnFocusCapture_UpdateVRControllerActiveState,
    &DeactivateVRControllersOnFocusCapture::UpdateVRControllerActiveState,
    void,
    DeactivateVRControllersOnFocusCapture* self
) {
    if (Manager::Replaying()) {
        for (auto controller : self->_vrControllerGameObjects)
            controller->active = true;
    } else
        DeactivateVRControllersOnFocusCapture_UpdateVRControllerActiveState(self);
}

// prevent multiplier animations while seeking
MAKE_AUTO_HOOK_MATCH(
    ScoreMultiplierUIController_HandleMultiplierDidChange,
    &ScoreMultiplierUIController::HandleMultiplierDidChange,
    void,
    ScoreMultiplierUIController* self,
    int multiplier,
    float progress
) {
    if (Pause::AllowAnimation(self))
        ScoreMultiplierUIController_HandleMultiplierDidChange(self, multiplier, progress);
}

// fix timing during renders
MAKE_AUTO_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {
    auto state = self->_state;
    if (!Camera::UpdateTime(self))
        self->_state = ::GlobalNamespace::IAudioTimeSource_State::Stopped;
    AudioTimeSyncController_Update(self);
    self->_state = state;
}

// prevent replays from adding to stats
MAKE_AUTO_HOOK_MATCH(LevelCompletionResults_ctor, &GlobalNamespace::LevelCompletionResults::_ctor, void,
                GlobalNamespace::LevelCompletionResults* self,
                ::GlobalNamespace::GameplayModifiers* gameplayModifiers,
                ::GlobalNamespace::LevelScore score,
                ::GlobalNamespace::RankModel_Rank rank,
                bool fullCombo,
                float_t leftSaberMovementDistance,
                float_t rightSaberMovementDistance,
                float_t leftHandMovementDistance,
                float_t rightHandMovementDistance,
                ::GlobalNamespace::LevelCompletionResultsBase_LevelEndStateType levelEndStateType,
                ::GlobalNamespace::LevelCompletionResultsBase_LevelEndAction levelEndAction,
                float_t energy,
                int32_t goodCutsCount,
                int32_t badCutsCount,
                int32_t missedCutsCount,
                int32_t notGoodCount,
                int32_t okCount,
                int32_t maxCutScore,
                int32_t totalCutScore,
                int32_t goodCutsCountForNotesWithFullScoreScoringType,
                float_t averageCenterDistanceCutScoreForNotesWithFullScoreScoringType,
                float_t averageCutScoreForNotesWithFullScoreScoringType,
                int32_t maxCombo,
                float_t endSongTime,
                bool invalidated,
                float_t startSongTime) {
    if (Manager::Replaying())
        invalidated = true;

    LevelCompletionResults_ctor(self, gameplayModifiers, score, rank, fullCombo, leftSaberMovementDistance, rightSaberMovementDistance, leftHandMovementDistance, rightHandMovementDistance, levelEndStateType, levelEndAction, energy, goodCutsCount, badCutsCount, missedCutsCount, notGoodCount, okCount, maxCutScore, totalCutScore, goodCutsCountForNotesWithFullScoreScoringType, averageCenterDistanceCutScoreForNotesWithFullScoreScoringType, averageCutScoreForNotesWithFullScoreScoringType, maxCombo, endSongTime, invalidated, startSongTime);
}

// disable results view controller for replays
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
    if (!Manager::CancelPresentation())
        FlowCoordinator_PresentViewController(self, viewController, finishedCallback, animationDirection, immediately);
}

// make the replay view controller properly act like its own menu
MAKE_AUTO_HOOK_MATCH(
    SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange,
    &SinglePlayerLevelSelectionFlowCoordinator::LevelSelectionFlowCoordinatorTopViewControllerWillChange,
    void,
    SinglePlayerLevelSelectionFlowCoordinator* self,
    HMUI::ViewController* oldViewController,
    HMUI::ViewController* newViewController,
    HMUI::ViewController::AnimationType animationType
) {
    if (newViewController->name == "ReplayMenuViewController") {
        self->SetLeftScreenViewController(nullptr, animationType);
        self->SetRightScreenViewController(nullptr, animationType);
        self->SetBottomScreenViewController(nullptr, animationType);
        self->SetTitle("REPLAY", animationType);
        self->showBackButton = true;
        return;
    }

    SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange(
        self, oldViewController, newViewController, animationType
    );
}

MAKE_AUTO_HOOK_MATCH(
    SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed,
    &SinglePlayerLevelSelectionFlowCoordinator::BackButtonWasPressed,
    void,
    SinglePlayerLevelSelectionFlowCoordinator* self,
    HMUI::ViewController* topViewController
) {
    if (topViewController->name == "ReplayMenuViewController")
        self->DismissViewController(topViewController, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
    else
        SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed(self, topViewController);
}

// update camera view
MAKE_AUTO_HOOK_MATCH(PlayerTransforms_Update, &PlayerTransforms::Update, void, PlayerTransforms* self) {
    Camera::UpdateCamera(self);
    PlayerTransforms_Update(self);
}

// apply wall quality without modifying the normal preset
MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetCoreMaterial,
    &ObstacleMaterialSetter::SetCoreMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer,
    BeatSaber::Settings::QualitySettings::ObstacleQuality obstacleQuality,
    bool screenDisplacementEffects
) {
    if (Manager::Rendering()) {
        switch (getConfig().Walls.GetValue()) {
            case 1:
                renderer->sharedMaterial = self->_texturedCoreMaterial;
                break;
            case 2:
                renderer->sharedMaterial = self->_hwCoreMaterial;
                break;
            default:
                renderer->sharedMaterial = self->_lwCoreMaterial;
                break;
        }
    } else
        ObstacleMaterialSetter_SetCoreMaterial(self, renderer, obstacleQuality, screenDisplacementEffects);
}

MAKE_AUTO_HOOK_MATCH(
    ObstacleMaterialSetter_SetFakeGlowMaterial,
    &ObstacleMaterialSetter::SetFakeGlowMaterial,
    void,
    ObstacleMaterialSetter* self,
    UnityEngine::Renderer* renderer,
    BeatSaber::Settings::QualitySettings::ObstacleQuality obstacleQuality
) {
    if (Manager::Rendering())
        renderer->sharedMaterial = getConfig().Walls.GetValue() == 0 ? self->_fakeGlowLWMaterial : self->_fakeGlowTexturedMaterial;
    else
        ObstacleMaterialSetter_SetFakeGlowMaterial(self, renderer, obstacleQuality);
}

// not sure why this doesn't seem to get done by setting mainEffect in SetGraphicsSettings
MAKE_AUTO_HOOK_MATCH(ConditionalActivation_Awake, &ConditionalActivation::Awake, void, ConditionalActivation* self) {
    if (Manager::Rendering()) {
        if (self->name == "ObstacleFrame")
            self->gameObject->active = getConfig().Bloom.GetValue();
        else if (self->name == "ObstacleFakeGlow")
            self->gameObject->active = !getConfig().Bloom.GetValue();
        else
            ConditionalActivation_Awake(self);
    } else
        ConditionalActivation_Awake(self);
}

// override shockwaves too
MAKE_AUTO_HOOK_MATCH(ShockwaveEffect_Start, &ShockwaveEffect::Start, void, ShockwaveEffect* self) {
    ShockwaveEffect_Start(self);

    if (Manager::Rendering())
        self->_shockwavePS->main.maxParticles = getConfig().ShockwavesOn.GetValue() ? getConfig().Shockwaves.GetValue() : 0;
}

// I think graphics tweaks is what unnecessarily disables the component in addition to the game object
MAKE_AUTO_HOOK_MATCH(ShockwaveEffect_SpawnShockwave, &ShockwaveEffect::SpawnShockwave, void, ShockwaveEffect* self, UnityEngine::Vector3 pos) {
    if (getConfig().ShockwavesOn.GetValue() && getConfig().Shockwaves.GetValue() > 0) {
        self->gameObject->active = true;
        self->enabled = true;
    }
    ShockwaveEffect_SpawnShockwave(self, pos);
}

// prevent pauses during renders
MAKE_AUTO_HOOK_MATCH(PauseController_get_canPause, &PauseController::get_canPause, bool, PauseController* self) {
    return Manager::Rendering() ? getConfig().Pauses.GetValue() : PauseController_get_canPause(self);
}

// prevent replays ending in pause menu
MAKE_AUTO_HOOK_MATCH(GameSongController_LateUpdate, &GameSongController::LateUpdate, void, GameSongController* self) {
    if (!Manager::Paused())
        GameSongController_LateUpdate(self);
}

// fix distortions in renders
// Unity 6000.3: gles::InitializeBlitFramebuffer(BlitFramebufferDrawQuad&, BlitFramebufferColorSpaceConversion, SinglePassStereo)
// returns whether the draw quad program is ready; the program id is still the first field of the draw quad.
MAKE_HOOK(initialize_blit_fbo, (nullptr), bool, void* drawQuad, int conversion, int passMode) {
    int& programId = *(int*) drawQuad;
    if (Camera::reinitDistortions != -1) {
        glDeleteProgram(programId);
        programId = 0;
        passMode = Camera::reinitDistortions;
    }
    Camera::reinitDistortions = -1;

    if (programId != 0)
        return true;

    return initialize_blit_fbo(drawQuad, conversion, passMode);
}

AUTO_HOOK_FUNCTION(blit_fbo) {
    // gles::InitializeBlitFramebuffer, verified in the 1.46.0 libunity.
    bool multiple = false;
    uintptr_t initialize_blit_fbo_addr = i2c::binary::libunity_unique_pattern(
        multiple,
        "fd 0f 1b f8 fe 67 01 a9 f8 5f 02 a9 f6 57 03 a9 f4 4f 04 a9 ff 43 0b d1 57 d0 3b d5 e8 16 40 f9 e8 67 01 f9 08 00 40 b9 68 00 00 34",
        "Replay InitializeBlitFramebuffer"
    );
    if (!initialize_blit_fbo_addr || multiple) {
        logger.warn("Unique InitializeBlitFramebuffer not found in libunity.so, render distortion fix disabled");
        return;
    }
    INSTALL_HOOK(logger, initialize_blit_fbo, (void*) initialize_blit_fbo_addr);
}
