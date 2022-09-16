#include "Main.hpp"
#include "Hooks.hpp"

#include "Formats/EventReplay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/BombNoteController.hpp"
#include "GlobalNamespace/BurstSliderGameNoteController.hpp"

// disable real cuts
MAKE_HOOK_MATCH(GameNoteController_HandleCut_Event, &GameNoteController::HandleCut,
        void, GameNoteController* self, Saber* saber, UnityEngine::Vector3 cutPoint, UnityEngine::Quaternion orientation, UnityEngine::Vector3 cutDirVec, bool allowBadCut) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;

    GameNoteController_HandleCut_Event(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}
MAKE_HOOK_MATCH(BombNoteController_HandleWasCutBySaber, &BombNoteController::HandleWasCutBySaber,
        void, BombNoteController* self, Saber* saber, UnityEngine::Vector3 cutPoint, UnityEngine::Quaternion orientation, UnityEngine::Vector3 cutDirVec) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;

    BombNoteController_HandleWasCutBySaber(self, saber, cutPoint, orientation, cutDirVec);
}
MAKE_HOOK_MATCH(BurstSliderGameNoteController_HandleCut, &BurstSliderGameNoteController::HandleCut,
        void, BurstSliderGameNoteController* self, Saber* saber, UnityEngine::Vector3 cutPoint, UnityEngine::Quaternion orientation, UnityEngine::Vector3 cutDirVec, bool allowBadCut) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;

    BurstSliderGameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
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

#include "GlobalNamespace/NoteController.hpp"

// disable misses
MAKE_HOOK_MATCH(NoteController_HandleNoteDidPassMissedMarkerEvent_Event, &NoteController::HandleNoteDidPassMissedMarkerEvent, void, NoteController* self) {
    
    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        return;

    NoteController_HandleNoteDidPassMissedMarkerEvent_Event(self);
}

#include "GlobalNamespace/BeatmapObjectManager.hpp"

// keep track of all notes
MAKE_HOOK_MATCH(BeatmapObjectManager_AddSpawnedNoteController, &BeatmapObjectManager::AddSpawnedNoteController,
        void, BeatmapObjectManager* self, NoteController* noteController, BeatmapObjectSpawnMovementData::NoteSpawnData noteSpawnData, float rotation) {
    
    BeatmapObjectManager_AddSpawnedNoteController(self, noteController, noteSpawnData, rotation);

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        Manager::Events::AddNoteController(noteController);
}
MAKE_HOOK_MATCH(BeatmapObjectManager_DespawnNoteController, static_cast<void(BeatmapObjectManager::*)(NoteController*)>(&BeatmapObjectManager::Despawn),
        void, BeatmapObjectManager* self, NoteController* noteController) {
    
    BeatmapObjectManager_DespawnNoteController(self, noteController);

    if(Manager::replaying && Manager::currentReplay.type == ReplayType::Event)
        Manager::Events::RemoveNoteController(noteController);
}

#include "GlobalNamespace/ScoreController.hpp"
#include "CustomTypes/ScoringElement.hpp"

// prevent crasing on attempting to despawn fake scoring elements
MAKE_HOOK_MATCH(ScoreController_DespawnScoringElement_Event, &ScoreController::DespawnScoringElement, void, ScoreController* self, ScoringElement* scoringElement) {
    
    if(il2cpp_utils::try_cast<ReplayHelpers::ScoringElement>(scoringElement))
        return;

    ScoreController_DespawnScoringElement_Event(self, scoringElement);
}

HOOK_FUNC(
    INSTALL_HOOK(logger, GameNoteController_HandleCut_Event);
    INSTALL_HOOK(logger, BombNoteController_HandleWasCutBySaber);
    INSTALL_HOOK(logger, BurstSliderGameNoteController_HandleCut);
    INSTALL_HOOK(logger, PlayerHeadAndObstacleInteraction_RefreshIntersectingObstacles);
    INSTALL_HOOK(logger, GameEnergyCounter_LateUpdate);
    INSTALL_HOOK(logger, NoteController_HandleNoteDidPassMissedMarkerEvent_Event);
    INSTALL_HOOK(logger, BeatmapObjectManager_AddSpawnedNoteController);
    INSTALL_HOOK(logger, BeatmapObjectManager_DespawnNoteController);
    INSTALL_HOOK(logger, ScoreController_DespawnScoringElement_Event);
)
