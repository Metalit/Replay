#include "Formats/EventReplay.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/GameplayCoreInstaller.hpp"
#include "GlobalNamespace/GameplayCoreSceneSetupData.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "UnityEngine/Time.hpp"
#include "hooks.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/internals.hpp"

using namespace GlobalNamespace;

// disable real cuts (regular notes in shared hooks)
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
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;

    BombNoteController_HandleWasCutBySaber(self, saber, cutPoint, orientation, cutDirVec);
}
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
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;

    BurstSliderGameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}

// disable real obstacle interactions
MAKE_AUTO_HOOK_MATCH(
    PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles,
    &PlayerHeadAndObstacleInteraction::RefreshIntersectingObstacles,
    void,
    PlayerHeadAndObstacleInteraction* self,
    UnityEngine::Vector3 worldPos
) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;

    PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles(self, worldPos);
}
// ensure energy changes for obstacles are accurate
MAKE_AUTO_HOOK_MATCH(GameEnergyCounter_LateUpdate, &GameEnergyCounter::LateUpdate, void, GameEnergyCounter* self) {

    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event) {
        float& actualEnergyLoss = Manager::Events::wallEnergyLoss;
        if (actualEnergyLoss > 0) {
            float gameEnergyLoss = UnityEngine::Time::get_deltaTime() * 1.3;
            if (gameEnergyLoss >= actualEnergyLoss) {
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

    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        Manager::Events::AddNoteController(noteController);
}

MAKE_AUTO_HOOK_MATCH(
    BeatmapObjectManager_DespawnNoteController, &BeatmapObjectManager::Despawn, void, BeatmapObjectManager* self, NoteController* noteController
) {
    BeatmapObjectManager_DespawnNoteController(self, noteController);

    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        Manager::Events::RemoveNoteController(noteController);
}

// recalculate notes based on map data if necessary
ON_EVENT(MetaCore::Events::MapStarted) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        RecalculateNotes(Manager::currentReplay, MetaCore::Internals::currentState.beatmapData->i___GlobalNamespace__IReadonlyBeatmapData());
}
