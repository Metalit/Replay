#include "Main.hpp"
#include "Hooks.hpp"

#include "Formats/EventReplay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"

// disable real cuts
MAKE_HOOK_MATCH(GameNoteController_Awake, &GameNoteController::Awake, void, GameNoteController* self) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event) {
        static auto method = il2cpp_utils::FindMethodUnsafe("", "NoteController", "Awake", 0);
        il2cpp_utils::RunMethodRethrow(self, method);
        return;
    }

    GameNoteController_Awake(self);
}
MAKE_HOOK_MATCH(BombNoteController_Awake, &BombNoteController::Awake, void, BombNoteController* self) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event) {
        static auto method = il2cpp_utils::FindMethodUnsafe("", "NoteController", "Awake", 0);
        il2cpp_utils::RunMethodRethrow(self, method);
        return;
    }

    BombNoteController_Awake(self);
}
MAKE_HOOK_MATCH(BurstSliderGameNoteController_Awake, &BurstSliderGameNoteController::Awake, void, BurstSliderGameNoteController* self) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event) {
        static auto method = il2cpp_utils::FindMethodUnsafe("", "NoteController", "Awake", 0);
        il2cpp_utils::RunMethodRethrow(self, method);
        return;
    }

    BurstSliderGameNoteController_Awake(self);
}

#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"

// disable real obstacle interactions
MAKE_HOOK_MATCH(PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles, &PlayerHeadAndObstacleInteraction::RefreshIntersectingObstacles,
        void, PlayerHeadAndObstacleInteraction* self, UnityEngine::Vector3 worldPos) {

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;
    
    PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles(self, worldPos);
}

#include "GlobalNamespace/BeatmapObjectManager.hpp"

// keep track of all notes
MAKE_HOOK_MATCH(BeatmapObjectManager_AddSpawnedNoteController, &BeatmapObjectManager::AddSpawnedNoteController,
        void, BeatmapObjectManager* self, NoteController* noteController, BeatmapObjectSpawnMovementData::NoteSpawnData noteSpawnData, float rotation) {
    
    BeatmapObjectManager_AddSpawnedNoteController(self, noteController, noteSpawnData, rotation);

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        Manager::Events::AddNoteController(noteController);
}
// keep track of all walls
MAKE_HOOK_MATCH(BeatmapObjectManager_AddSpawnedObstacleController, &BeatmapObjectManager::AddSpawnedObstacleController,
        void, BeatmapObjectManager* self, ObstacleController* obstacleController, BeatmapObjectSpawnMovementData::ObstacleSpawnData obstacleSpawnData, float rotation) {
    
    BeatmapObjectManager_AddSpawnedObstacleController(self, obstacleController, obstacleSpawnData, rotation);

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        Manager::Events::AddObstacleController(obstacleController);
}
MAKE_HOOK_MATCH(ObstacleController_ManualUpdate, &ObstacleController::ManualUpdate, void, ObstacleController* self) {

    bool hadPassedAvoided = self->passedAvoidedMarkReported;

    ObstacleController_ManualUpdate(self);

    if(self->passedAvoidedMarkReported && !hadPassedAvoided && Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        Manager::Events::ObstacleControllerFinished(self);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, GameNoteController_Awake);
    INSTALL_HOOK(logger, BombNoteController_Awake);
    INSTALL_HOOK(logger, BurstSliderGameNoteController_Awake);
    INSTALL_HOOK(logger, BeatmapObjectManager_AddSpawnedNoteController);
    INSTALL_HOOK(logger, BeatmapObjectManager_AddSpawnedObstacleController);
    INSTALL_HOOK(logger, ObstacleController_ManualUpdate);
)
