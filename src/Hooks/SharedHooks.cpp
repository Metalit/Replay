#include "Main.hpp"
#include "Hooks.hpp"
#include "Config.hpp"

#include "Replay.hpp"
#include "ReplayManager.hpp"
#include "Formats/FrameReplay.hpp"

using namespace GlobalNamespace;

#include "GlobalNamespace/ScoreController.hpp"
#include "CustomTypes/ScoringElement.hpp"

// override max score in frame replays and prevent crashing on attempting to despawn fake scoring elements from event replays
MAKE_HOOK_MATCH(ScoreController_DespawnScoringElement, &ScoreController::DespawnScoringElement, void, ScoreController* self, ScoringElement* scoringElement) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame) {
        auto frame = Manager::Frames::GetScoreFrame();
        // modified scores are calculated based on these after this function is called
        self->multipliedScore = frame->score;
        if(frame->percent > 0)
            self->immediateMaxPossibleMultipliedScore = frame->score / frame->percent;
    }
    if(il2cpp_utils::try_cast<ReplayHelpers::ScoringElement>(scoringElement))
        return;

    ScoreController_DespawnScoringElement(self, scoringElement);
}

#include "GlobalNamespace/GameNoteController.hpp"

// disable all cuts on event replays or bad ones at inaccurate times on frame replays
MAKE_HOOK_MATCH(GameNoteController_HandleCut, &GameNoteController::HandleCut,
        void, GameNoteController* self, Saber* saber, UnityEngine::Vector3 cutPoint, UnityEngine::Quaternion orientation, UnityEngine::Vector3 cutDirVec, bool allowBadCut) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;
    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        allowBadCut = false;

    GameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}
// misses too
MAKE_HOOK_MATCH(NoteController_HandleNoteDidPassMissedMarkerEvent, &NoteController::HandleNoteDidPassMissedMarkerEvent, void, NoteController* self) {

    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;
    if(Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        return;

    NoteController_HandleNoteDidPassMissedMarkerEvent(self);
}

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "UnityEngine/Time.hpp"

// keep song time and current frame up to date, plus controller inputs, and delay ending of renders
MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {

    if(Manager::replaying && !Manager::paused) {
        Manager::UpdateTime(self->songTime);
        Manager::CheckInputs();
    }
    int state = self->state;
    if(Manager::replaying && Manager::Camera::rendering && !Manager::Camera::GetAudioMode())
        self->state = AudioTimeSyncController::State::Stopped;

    AudioTimeSyncController_Update(self);

    // remove min check
    if(Manager::replaying && Manager::Camera::rendering && !Manager::Camera::GetAudioMode()) {
		self->lastFrameDeltaSongTime = UnityEngine::Time::get_deltaTime() * self->timeScale;
        self->songTime += self->lastFrameDeltaSongTime;
        self->isReady = true;
    }
    self->state = state;
}

#include "GlobalNamespace/PlayerTransforms.hpp"

MAKE_HOOK_MATCH(PlayerTransforms_Update, &PlayerTransforms::Update, void, PlayerTransforms* self) {

    Camera_PlayerTransformsUpdate_Pre(self);
    PlayerTransforms_Update(self);
    Replay_PlayerTransformsUpdate_Post(self);
}

#include "GlobalNamespace/PauseMenuManager.hpp"
#include "UnityEngine/Camera.hpp"

// enable camera when pausing and track it for the manager
MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu, &PauseMenuManager::ShowMenu, void, PauseMenuManager* self) {

    if(Manager::replaying)
        Manager::ReplayPaused();
    if(Manager::replaying && Manager::Camera::rendering && getConfig().CameraOff.GetValue() && mainCamera)
        mainCamera->set_enabled(true);

    PauseMenuManager_ShowMenu(self);
}
MAKE_HOOK_MATCH(PauseMenuManager_HandleResumeFromPauseAnimationDidFinish, &PauseMenuManager::HandleResumeFromPauseAnimationDidFinish, void, PauseMenuManager* self) {

    if(Manager::replaying)
        Manager::ReplayUnpaused();
    if(Manager::replaying && Manager::Camera::rendering && getConfig().CameraOff.GetValue() && mainCamera)
        mainCamera->set_enabled(false);

    PauseMenuManager_HandleResumeFromPauseAnimationDidFinish(self);
}

HOOK_FUNC(
    // event and frame
    INSTALL_HOOK(logger, ScoreController_DespawnScoringElement);
    INSTALL_HOOK(logger, GameNoteController_HandleCut);
    INSTALL_HOOK(logger, NoteController_HandleNoteDidPassMissedMarkerEvent);
    // general and camera
    INSTALL_HOOK(logger, AudioTimeSyncController_Update);
    INSTALL_HOOK(logger, PlayerTransforms_Update);
    INSTALL_HOOK(logger, PauseMenuManager_ShowMenu);
    INSTALL_HOOK(logger, PauseMenuManager_HandleResumeFromPauseAnimationDidFinish);
)
