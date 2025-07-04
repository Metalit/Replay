#include "hooks.hpp"

#include <GLES3/gl3.h>

#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/DeactivateVRControllersOnFocusCapture.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
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
        byref(directionOK),
        byref(speedOK),
        byref(saberTypeOK),
        byref(cutDirDeviation),
        byref(cutDirAngle)
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
#define HashSet_Contains_MInfo                                                                                                      \
    il2cpp_utils::FindMethod(                                                                                                       \
        il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_class<System::Collections::Generic::HashSet_1<ObstacleController*>*>::get(), \
        "Contains",                                                                                                                 \
        std::span<Il2CppClass*>{},                                                                                                  \
        std::array{il2cpp_utils::il2cpp_type_check::il2cpp_no_arg_type<ObstacleController*>::get()}                                 \
    )

MAKE_AUTO_HOOK_FIND(
    HashSet_Contains, HashSet_Contains_MInfo, bool, System::Collections::Generic::HashSet_1<ObstacleController*>* self, ObstacleController* item
) {
    return disableObstacleEntry ? true : HashSet_Contains(self, item);
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
MAKE_AUTO_HOOK_MATCH(
    MenuTransitionsHelper_StartStandardLevel,
    &MenuTransitionsHelper::StartStandardLevel,
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
    Playback::ProcessStart(f8, f10, f9);
    MenuTransitionsHelper_StartStandardLevel(self, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15, f16, f17, f18, f19);
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

// fix timing during renders
MAKE_AUTO_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {
    auto state = self->_state;
    if (!Camera::UpdateTime(self))
        self->_state = AudioTimeSyncController::State::Stopped;
    AudioTimeSyncController_Update(self);
    self->_state = state;
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

// delay ending of replays by one second
MAKE_AUTO_HOOK_MATCH(AudioTimeSyncController_get_songEndTime, &AudioTimeSyncController::get_songEndTime, float, AudioTimeSyncController* self) {
    return (Manager::Replaying() ? 1 : 0) + AudioTimeSyncController_get_songEndTime(self);
}

// fix distortions in renders
MAKE_HOOK_NO_CATCH(initialize_blit_fbo, 0x0, void, void* drawQuad, int conversion, int passMode) {
    int& programId = *(int*) drawQuad;
    if (Camera::reinitDistortions != -1) {
        glDeleteProgram(programId);
        programId = 0;
        passMode = Camera::reinitDistortions;
    }
    Camera::reinitDistortions = -1;

    if (programId != 0)
        return;

    initialize_blit_fbo(drawQuad, conversion, passMode);
}

AUTO_HOOK_FUNCTION(blit_fbo) {
    uintptr_t initialize_blit_fbo_addr = findPattern(
        baseAddr("libunity.so"), "fd 7b ba a9 fc 6f 01 a9 fa 67 02 a9 f8 5f 03 a9 f6 57 04 a9 f4 4f 05 a9 ff 83 0b d1 58 d0 3b d5 08", 0x2000000
    );
    INSTALL_HOOK_DIRECT(logger, initialize_blit_fbo, (void*) initialize_blit_fbo_addr);
}
