#include "Main.hpp"
#include "Hooks.hpp"

#include "Formats/EventReplay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"

// disable real cuts (regular notes in shared hooks)
MAKE_HOOK_MATCH(BombNoteController_HandleWasCutBySaber, &BombNoteController::HandleWasCutBySaber,
        void, BombNoteController* self, Saber* saber, UnityEngine::Vector3 cutPoint, UnityEngine::Quaternion orientation, UnityEngine::Vector3 cutDirVec) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;

    BombNoteController_HandleWasCutBySaber(self, saber, cutPoint, orientation, cutDirVec);
}
MAKE_HOOK_MATCH(BurstSliderGameNoteController_HandleCut, &BurstSliderGameNoteController::HandleCut,
        void, BurstSliderGameNoteController* self, Saber* saber, UnityEngine::Vector3 cutPoint, UnityEngine::Quaternion orientation, UnityEngine::Vector3 cutDirVec, bool allowBadCut) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;

    BurstSliderGameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}

#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "UnityEngine/Time.hpp"

// disable real obstacle interactions
MAKE_HOOK_MATCH(PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles, &PlayerHeadAndObstacleInteraction::RefreshIntersectingObstacles,
        void, PlayerHeadAndObstacleInteraction* self, UnityEngine::Vector3 worldPos) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;

    PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles(self, worldPos);
}
// ensure energy changes for obstacles are accurate
MAKE_HOOK_MATCH(GameEnergyCounter_LateUpdate, &GameEnergyCounter::LateUpdate, void, GameEnergyCounter* self) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event) {
        float& actualEnergyLoss = Manager::Events::wallEnergyLoss;
        if(actualEnergyLoss > 0) {
            float gameEnergyLoss = UnityEngine::Time::get_deltaTime() * 1.3;
            if(gameEnergyLoss >= actualEnergyLoss) {
                self->ProcessEnergyChange(-actualEnergyLoss);
                actualEnergyLoss = 0;
            } else {
                self->ProcessEnergyChange(-gameEnergyLoss);
                actualEnergyLoss -= gameEnergyLoss;
            }
        }
    }
    GameEnergyCounter_LateUpdate(self);
}

#include "GlobalNamespace/BeatmapObjectManager.hpp"

// keep track of all notes
MAKE_HOOK_MATCH(BeatmapObjectManager_AddSpawnedNoteController, &BeatmapObjectManager::AddSpawnedNoteController,
        void, BeatmapObjectManager* self, NoteController* noteController, BeatmapObjectSpawnMovementData::NoteSpawnData noteSpawnData, float rotation) {

    BeatmapObjectManager_AddSpawnedNoteController(self, noteController, noteSpawnData, rotation);

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        Manager::Events::AddNoteController(noteController);
}
MAKE_HOOK_MATCH(BeatmapObjectManager_DespawnNoteController, static_cast<void(BeatmapObjectManager::*)(NoteController*)>(&BeatmapObjectManager::Despawn),
        void, BeatmapObjectManager* self, NoteController* noteController) {

    BeatmapObjectManager_DespawnNoteController(self, noteController);

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        Manager::Events::RemoveNoteController(noteController);
}

#include "GlobalNamespace/GameplayCoreInstaller.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"

// recalculate notes based on map data if necessary
MAKE_HOOK_MATCH(GameplayCoreInstaller_InstallBindings, &GameplayCoreInstaller::InstallBindings, void, GameplayCoreInstaller* self) {

    GameplayCoreInstaller_InstallBindings(self);

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        RecalculateNotes(Manager::currentReplay, self->sceneSetupData->transformedBeatmapData);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, BombNoteController_HandleWasCutBySaber);
    INSTALL_HOOK(logger, BurstSliderGameNoteController_HandleCut);
    INSTALL_HOOK(logger, PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles);
    INSTALL_HOOK(logger, GameEnergyCounter_LateUpdate);
    INSTALL_HOOK(logger, BeatmapObjectManager_AddSpawnedNoteController);
    INSTALL_HOOK(logger, BeatmapObjectManager_DespawnNoteController);
    INSTALL_HOOK(logger, GameplayCoreInstaller_InstallBindings);
)
