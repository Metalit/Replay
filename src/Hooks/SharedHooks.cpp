#include "Config.hpp"
#include "Formats/FrameReplay.hpp"
#include "Hooks.hpp"
#include "Main.hpp"
#include "Replay.hpp"
#include "ReplayManager.hpp"

using namespace GlobalNamespace;

#include "CustomTypes/ScoringElement.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Time.hpp"

// override max score in frame replays and prevent crashing on attempting to despawn fake scoring elements from event replays
MAKE_AUTO_HOOK_MATCH(
    ScoreController_DespawnScoringElement, &ScoreController::DespawnScoringElement, void, ScoreController* self, ScoringElement* scoringElement
) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && Manager::Frames::AllowScoreOverride()) {
        auto frame = Manager::Frames::GetScoreFrame();
        // modified scores are calculated based on these after this function is called
        self->_multipliedScore = frame->score;
        if (frame->percent > 0)
            self->_immediateMaxPossibleMultipliedScore = frame->score / frame->percent;
    }
    if (il2cpp_utils::try_cast<ReplayHelpers::ScoringElement>(scoringElement))
        return;

    ScoreController_DespawnScoringElement(self, scoringElement);
}

// disable all cuts on event replays or bad ones at inaccurate times on frame replays
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
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        allowBadCut = false;

    GameNoteController_HandleCut(self, saber, cutPoint, orientation, cutDirVec, allowBadCut);
}

// misses too
MAKE_AUTO_HOOK_MATCH(
    NoteController_HandleNoteDidPassMissedMarkerEvent, &NoteController::HandleNoteDidPassMissedMarkerEvent, void, NoteController* self
) {
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Event)
        return;
    if (Manager::replaying && Manager::currentReplay.type & ReplayType::Frame && !Manager::Frames::AllowComboDrop())
        return;

    NoteController_HandleNoteDidPassMissedMarkerEvent(self);
}

// keep song time and current frame up to date, plus controller inputs, and delay ending of renders
MAKE_AUTO_HOOK_MATCH(AudioTimeSyncController_Update, &AudioTimeSyncController::Update, void, AudioTimeSyncController* self) {

    if (Manager::replaying && !Manager::paused) {
        Manager::UpdateTime(self->songTime, self->get_songLength());
        Manager::CheckInputs();
    }
    auto state = self->_state;
    bool customTiming = Manager::replaying && Manager::Camera::rendering;
    if (customTiming)
        self->_state = AudioTimeSyncController::State::Stopped;

    AudioTimeSyncController_Update(self);

    // remove _songTime clamp
    if (customTiming && state == AudioTimeSyncController::State::Playing) {
        self->_lastFrameDeltaSongTime = UnityEngine::Time::get_deltaTime() * self->timeScale;
        self->_songTime += self->_lastFrameDeltaSongTime;
        self->_dspTimeOffset = UnityEngine::AudioSettings::get_dspTime() - self->_songTime;
        self->_isReady = true;
    }

    self->_state = state;
}

// enable camera when pausing and track it for the manager
MAKE_AUTO_HOOK_MATCH(PauseMenuManager_ShowMenu, &PauseMenuManager::ShowMenu, void, PauseMenuManager* self) {

    if (Manager::replaying)
        Manager::ReplayPaused();
    Camera_Pause();

    PauseMenuManager_ShowMenu(self);
}

MAKE_AUTO_HOOK_MATCH(
    PauseMenuManager_HandleResumeFromPauseAnimationDidFinish, &PauseMenuManager::HandleResumeFromPauseAnimationDidFinish, void, PauseMenuManager* self
) {
    if (Manager::replaying)
        Manager::ReplayUnpaused();
    Camera_Unpause();

    PauseMenuManager_HandleResumeFromPauseAnimationDidFinish(self);
}
