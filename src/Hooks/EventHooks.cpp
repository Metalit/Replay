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
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "UnityEngine/Time.hpp"

// disable real obstacle interactions
MAKE_HOOK_MATCH(PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles, &PlayerHeadAndObstacleInteraction::RefreshIntersectingObstacles,
        void, PlayerHeadAndObstacleInteraction* self, UnityEngine::Vector3 worldPos) {

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;
    
    PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles(self, worldPos);
}
// ensure energy changes for obstacles are accurate
MAKE_HOOK_MATCH(GameEnergyCounter_LateUpdate, &GameEnergyCounter::LateUpdate, void, GameEnergyCounter* self) {

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event) {
        float& actualEnergyLoss = Manager::Events::GetWallEnergyLoss();
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

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        Manager::Events::AddNoteController(noteController);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, GameNoteController_Awake);
    INSTALL_HOOK(logger, BombNoteController_Awake);
    INSTALL_HOOK(logger, BurstSliderGameNoteController_Awake);
    INSTALL_HOOK(logger, PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles);
    INSTALL_HOOK(logger, GameEnergyCounter_LateUpdate);
    INSTALL_HOOK(logger, BeatmapObjectManager_AddSpawnedNoteController);
)
