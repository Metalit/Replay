#include "CustomTypes/ScoringElement.hpp"
#include "Formats/FrameReplay.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Time.hpp"
#include "config.hpp"
#include "hooks.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/events.hpp"
#include "replay.hpp"

using namespace GlobalNamespace;

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

    auto state = self->_state;
    bool customTiming = Manager::replaying && Manager::Camera::rendering;
    if (customTiming)
        self->_state = AudioTimeSyncController::State::Stopped;

    AudioTimeSyncController_Update(self);

    // copy default time sync logic because it probably works
    if (customTiming && state == AudioTimeSyncController::State::Playing) {
        float estimatedTimeIncrease = UnityEngine::Time::get_deltaTime() * self->_timeScale;

        int timeSamples = self->_audioSource->timeSamples;
        if (self->_prevAudioSamplePos > timeSamples)
            self->_playbackLoopIndex++;
        if (self->_prevAudioSamplePos == timeSamples)
            self->_inBetweenDSPBufferingTimeEstimate += estimatedTimeIncrease;
        else
            self->_inBetweenDSPBufferingTimeEstimate = 0;
        self->_prevAudioSamplePos = timeSamples;

        // add regular delta time after audio ends
        float audioSourceTime = self->_songTime + estimatedTimeIncrease;
        if (self->_audioSource->isPlaying)
            audioSourceTime =
                self->_audioSource->time + self->_playbackLoopIndex * self->_audioSource->clip->length + self->_inBetweenDSPBufferingTimeEstimate;
        float unityClockTime = self->timeSinceStart - self->_audioStartTimeOffsetSinceStart;

        self->_dspTimeOffset = UnityEngine::AudioSettings::get_dspTime() - audioSourceTime / self->_timeScale;
        float timeDifference = std::abs(unityClockTime - audioSourceTime);
        if (timeDifference > self->_forcedSyncDeltaTime) {
            self->_audioStartTimeOffsetSinceStart = self->timeSinceStart - audioSourceTime;
            unityClockTime = audioSourceTime;
        } else {
            if (self->_fixingAudioSyncError) {
                if (timeDifference < self->_stopSyncDeltaTime)
                    self->_fixingAudioSyncError = false;
            } else if (timeDifference > self->_startSyncDeltaTime)
                self->_fixingAudioSyncError = true;

            if (self->_fixingAudioSyncError)
                self->_audioStartTimeOffsetSinceStart = std::lerp(
                    self->_audioStartTimeOffsetSinceStart, self->timeSinceStart - audioSourceTime, estimatedTimeIncrease * self->_audioSyncLerpSpeed
                );
        }

        // ignore songTimeOffset and audioLatency
        float time = std::max(self->_songTime, unityClockTime);
        self->_lastFrameDeltaSongTime = time - self->_songTime;
        self->_songTime = time;
        self->_isReady = true;
    }

    self->_state = state;
}
