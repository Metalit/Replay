#include "Main.hpp"
#include "Config.hpp"
#include "ReplayManager.hpp"
#include "MathUtils.hpp"
#include "Utils.hpp"
#include "MenuSelection.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "CustomTypes/ReplayMenu.hpp"
#include "PauseMenu.hpp"

#include "bs-utils/shared/utils.hpp"

#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/ObstacleData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/PrepareLevelCompletionResults.hpp"
#include "GlobalNamespace/GameEnergyUIPanel.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/AudioManagerSO.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/MainSettingsModelSO.hpp"
#include "GlobalNamespace/IntSO.hpp"
#include "GlobalNamespace/BoolSO.hpp"
#include "UnityEngine/QualitySettings.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Time.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "System/Action_1.hpp"
#include "System/Action.hpp"

using namespace GlobalNamespace;

struct NoteCompare {
    constexpr bool operator()(const NoteController* const& lhs, const NoteController* const& rhs) const {
        if(lhs->noteData->time == rhs->noteData->time)
            return lhs < rhs;
        return lhs->noteData->time < rhs->noteData->time;
    }
};

std::vector<std::pair<std::string, ReplayWrapper>> currentReplays;

namespace Manager {

    int currentFrame = 0;
    int frameCount = 0;
    float songTime = -1;
    float lerpAmount = 0;
    float lastCutTime = -1;

    namespace Objects {
        Saber *leftSaber, *rightSaber;
        PlayerHeadAndObstacleInteraction* obstacleChecker;
        PlayerTransforms* playerTransforms;
        PauseMenuManager* pauseManager;
        ScoreController* scoreController;
        ComboController* comboController;
        GameEnergyCounter* gameEnergyCounter;
        GameEnergyUIPanel* energyBar;
        NoteCutSoundEffectManager* noteSoundManager;
        AudioManagerSO* audioManager;
        BeatmapObjectManager* beatmapObjectManager;
        BeatmapCallbacksController* callbackController;

        void GetObjects() {
            auto sabers = UnityEngine::Resources::FindObjectsOfTypeAll<Saber*>();
            leftSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberA; });
            rightSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberB; });
            obstacleChecker = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerHeadAndObstacleInteraction*>().First();
            playerTransforms = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerTransforms*>().First();
            pauseManager = UnityEngine::Resources::FindObjectsOfTypeAll<PauseMenuManager*>().First();
            auto hasOtherObjects = UnityEngine::Resources::FindObjectsOfTypeAll<PrepareLevelCompletionResults*>().First();
            scoreController = (ScoreController*) hasOtherObjects->scoreController;
            comboController = hasOtherObjects->comboController;
            gameEnergyCounter = hasOtherObjects->gameEnergyCounter;
            energyBar = UnityEngine::Resources::FindObjectsOfTypeAll<GameEnergyUIPanel*>().First();
            noteSoundManager = UnityEngine::Resources::FindObjectsOfTypeAll<NoteCutSoundEffectManager*>().First();
            audioManager = UnityEngine::Resources::FindObjectsOfTypeAll<AudioManagerSO*>().First();
            beatmapObjectManager = noteSoundManager->beatmapObjectManager;
            callbackController = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapObjectSpawnController*>().First()->beatmapCallbacksController;
        }
    }

    namespace Camera {
        bool rendering = false;
        bool moving = false;

        Vector3 smoothPosition;
        Quaternion smoothRotation;

        GlobalNamespace::MainEffectGraphicsSettingsPresetsSO* bloomPresets;
        GlobalNamespace::MainEffectContainerSO* bloomContainer;
        MirrorRendererGraphicsSettingsPresets* mirrorPresets;
        MirrorRendererSO* mirrorRenderer;

        void SetFromConfig() {
            smoothPosition = (Vector3) getConfig().ThirdPerPos.GetValue();
            smoothRotation = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
        }

        void UpdateTime() {
            if(GetMode() == (int) CameraMode::Smooth) {
                float deltaTime = UnityEngine::Time::get_deltaTime();
                smoothPosition = EaseLerp(smoothPosition, GetFrame().head.position, UnityEngine::Time::get_time(), deltaTime * 2 / getConfig().Smoothing.GetValue());
                smoothRotation = Slerp(smoothRotation, GetFrame().head.rotation, deltaTime * 2 / getConfig().Smoothing.GetValue());
            } else if(GetMode() == (int) CameraMode::ThirdPerson)
                SetFromConfig();
        }

        void Move(int direction) {
            if(direction == 0)
                return;
            static const Vector3 move = {0, 0, 1.5};
            if(GetMode() == (int) CameraMode::ThirdPerson) {
                float delta = UnityEngine::Time::get_deltaTime() * getConfig().TravelSpeed.GetValue();
                smoothPosition += Sombrero::QuaternionMultiply(smoothRotation, move * delta * direction);
                getConfig().ThirdPerPos.SetValue(smoothPosition);
            }
        }

        Vector3 GetHeadPosition() {
            if(GetMode() == (int) CameraMode::Smooth) {
                auto offset = getConfig().Offset.GetValue();
                if(getConfig().Relative.GetValue())
                    offset = Sombrero::QuaternionMultiply(GetHeadRotation(), offset);
                return smoothPosition + offset;
            }
            return smoothPosition;
        }
        Quaternion GetHeadRotation() {
            if(GetMode() == (int) CameraMode::Smooth) {
                if(getConfig().Correction.GetValue())
                    return Sombrero::QuaternionMultiply(smoothRotation, currentReplay.replay->info.averageOffset);
            }
            return smoothRotation;
        }

        bool GetAudioMode() {
            return getConfig().AudioMode.GetValue();
        }

        int GetMode() {
            CameraMode mode = (CameraMode) getConfig().CamMode.GetValue();
            if(mode == CameraMode::Headset && !GetAudioMode() && rendering)
                return (int) CameraMode::Smooth;
            return (int) mode;
        }

        void SetMode(int value) {
            CameraMode oldMode = (CameraMode) getConfig().CamMode.GetValue();
            CameraMode mode = (CameraMode) value;
            if(mode == oldMode)
                return;
            getConfig().CamMode.SetValue(value);
            if(mode == CameraMode::Smooth) {
                smoothPosition = GetFrame().head.position;
                smoothRotation = GetFrame().head.rotation;
            } else if(GetMode() == (int) CameraMode::ThirdPerson)
                SetFromConfig();
        }

        void SetGraphicsSettings() {
            if(GetAudioMode())
                return;
            auto settings = UnityEngine::Resources::FindObjectsOfTypeAll<MainSettingsModelSO*>().First();
            int shockwaves = getConfig().ShockwavesOn.GetValue() ? getConfig().Shockwaves.GetValue() : 0;
            settings->maxShockwaveParticles->set_value(shockwaves);
            settings->screenDisplacementEffectsEnabled->set_value(getConfig().Walls.GetValue());

            int antiAlias = 0;
            switch(getConfig().AntiAlias.GetValue()) {
            case 0:
                antiAlias = 0;
                break;
            case 1:
                antiAlias = 2;
                break;
            case 2:
                antiAlias = 4;
                break;
            case 3:
                antiAlias = 8;
                break;
            }
            UnityEngine::QualitySettings::set_antiAliasing(antiAlias);

            if(getConfig().Bloom.GetValue())
                bloomContainer->Init(bloomPresets->presets[1]->mainEffect);

            if(getConfig().Mirrors.GetValue() != 0) {
                int length = mirrorPresets->presets.Length();
                auto preset = mirrorPresets->presets[std::min(getConfig().Mirrors.GetValue(), length - 1)];
                mirrorRenderer->Init(preset->reflectLayers, preset->stereoTextureWidth, preset->stereoTextureHeight, preset->monoTextureWidth,
                    preset->monoTextureHeight, preset->maxAntiAliasing, preset->enableBloomPrePassFog);
            }
        }
        void UnsetGraphicsSettings() {
            if(GetAudioMode())
                return;
            auto settings = UnityEngine::Resources::FindObjectsOfTypeAll<MainSettingsModelSO*>().First();
            settings->maxShockwaveParticles->set_value(0);
            settings->screenDisplacementEffectsEnabled->set_value(false);
            // I don't think this actually affects things and could probably be left alone, but idk
            UnityEngine::QualitySettings::set_antiAliasing(settings->antiAliasingLevel->get_value());

            if(getConfig().Bloom.GetValue())
                bloomContainer->Init(bloomPresets->presets[0]->mainEffect);

            if(getConfig().Mirrors.GetValue() != 0) {
                auto preset = mirrorPresets->presets[settings->mirrorGraphicsSettings->get_value()];
                mirrorRenderer->Init(preset->reflectLayers, preset->stereoTextureWidth, preset->stereoTextureHeight, preset->monoTextureWidth,
                    preset->monoTextureHeight, preset->maxAntiAliasing, preset->enableBloomPrePassFog);
            }
        }

        void ReplayStarted() {
            if(rendering)
                SetGraphicsSettings();
            if(GetMode() == (int) CameraMode::Smooth) {
                smoothRotation = GetFrame().head.rotation;
                // undo rotation by average rotation offset
                if(getConfig().Correction.GetValue())
                    smoothRotation = Sombrero::QuaternionMultiply(smoothRotation, currentReplay.replay->info.averageOffset);
                // add position offset, potentially relative to rotation
                auto offset = getConfig().Offset.GetValue();
                if(getConfig().Relative.GetValue())
                    offset = Sombrero::QuaternionMultiply(smoothRotation, offset);
                smoothPosition = GetFrame().head.position + offset;
            } else if(GetMode() == (int) CameraMode::ThirdPerson)
                SetFromConfig();
        }

        void ReplayEnded() {
            if(rendering)
                UnsetGraphicsSettings();
        }
    }

    namespace Frames {
        decltype(FrameReplay::scoreFrames)::iterator scoreFrame;
        ScoreFrame currentValues;
        FrameReplay* replay;

        void Increment() {
            if(scoreFrame->score >= 0)
                currentValues.score = scoreFrame->score;
            if(scoreFrame->percent >= 0)
                currentValues.percent = scoreFrame->percent;
            if(scoreFrame->combo >= 0)
                currentValues.combo = scoreFrame->combo;
            if(scoreFrame->energy >= 0)
                currentValues.energy = scoreFrame->energy;
            if(scoreFrame->offset >= 0)
                currentValues.offset = scoreFrame->offset;
            scoreFrame++;
        }

        void ReplayStarted() {
            replay = dynamic_cast<FrameReplay*>(currentReplay.replay.get());
            scoreFrame = replay->scoreFrames.begin();
            currentValues = {-1, -1, -1, -1, -1, -1};
            while(currentValues.score < 0 || currentValues.combo < 0 || currentValues.energy < 0 || currentValues.offset < 0)
                Increment();
        }

        void UpdateTime() {
            while(scoreFrame != replay->scoreFrames.end() && scoreFrame->time < songTime)
                Increment();
        }

        ScoreFrame* GetScoreFrame() {
            return &currentValues;
        }

        bool AllowComboDrop() {
            static int frameSearchRadius = 1;
            int combo = 1;
            auto iter = scoreFrame;
            iter -= frameSearchRadius + 1;
            for(int i = 0; i < frameSearchRadius; i++) {
                iter--;
                if(iter->combo < 0)
                    i--;
                if(iter == replay->scoreFrames.begin())
                    break;
            }
            for(int i = 0; i < frameSearchRadius * 2; i++) {
                iter++;
                if(iter == replay->scoreFrames.end())
                    break;
                if(iter->combo < 0) {
                    i--;
                    continue;
                }
                if(iter->combo < combo)
                    return true;
                combo = iter->combo;
            }
            return false;
        }

        bool AllowScoreOverride() {
            if(currentValues.percent >= 0)
                return true;
            // fix scoresaber replays having incorrect max score before cut finishes
            return songTime - lastCutTime > 0.4;
        }
    }

    namespace Events {
        std::set<NoteController*, NoteCompare> notes;
        decltype(EventReplay::events)::iterator event;
        EventReplay* replay;
        float wallEndTime = 0;
        float wallEnergyLoss = 0;

        void ReplayStarted() {
            notes.clear();
            replay = dynamic_cast<EventReplay*>(currentReplay.replay.get());
            event = replay->events.begin();
            wallEndTime = 0;
            wallEnergyLoss = 0;
        }

        void AddNoteController(NoteController* note) {
            if(note->noteData->scoringType > NoteData::ScoringType::NoScore || note->noteData->gameplayType == NoteData::GameplayType::Bomb)
                notes.insert(note);
        }
        void RemoveNoteController(NoteController* note) {
            auto iter = notes.find(note);
            if(iter != notes.end())
                notes.erase(iter);
        }

        void ProcessNoteEvent(const NoteEvent& event) {
            auto& info = event.info;
            bool found = false;
            for(auto iter = notes.begin(); iter != notes.end(); iter++) {
                auto controller = *iter;
                auto noteData = controller->noteData;
                if((noteData->scoringType == info.scoringType || info.scoringType == -2)
                        && noteData->lineIndex == info.lineIndex
                        && noteData->noteLineLayer == info.lineLayer
                        && noteData->colorType == info.colorType
                        && noteData->cutDirection == info.cutDirection) {
                    found = true;
                    bool isLeftSaber = event.noteCutInfo.saberType == SaberType::SaberA;
                    Saber* saber = isLeftSaber ? Objects::leftSaber : Objects::rightSaber;
                    if(info.eventType == NoteEventInfo::Type::GOOD || info.eventType == NoteEventInfo::Type::BAD) {
                        auto cutInfo = GetNoteCutInfo(controller, saber, event.noteCutInfo);
                        if(replay->cutInfoMissingOKs) {
                            cutInfo.speedOK = cutInfo.saberSpeed > 2;
                            bool isLeftColor = noteData->colorType == ColorType::ColorA;
                            cutInfo.saberTypeOK = isLeftColor == isLeftSaber;
                            cutInfo.timeDeviation = noteData->time - event.time;
                        }
                        SetLastCutTime(event.time);
                        il2cpp_utils::RunMethodUnsafe(controller, "SendNoteWasCutEvent", byref(cutInfo));
                    } else if(info.eventType == NoteEventInfo::Type::MISS) {
                        controller->SendNoteWasMissedEvent();
                        notes.erase(iter); // note will despawn and be removed in the other cases
                    } else if(info.eventType == NoteEventInfo::Type::BOMB) {
                        auto cutInfo = GetBombCutInfo(controller, saber);
                        il2cpp_utils::RunMethodUnsafe(controller, "SendNoteWasCutEvent", byref(cutInfo));
                    }
                    break;
                }
            }
            if(!found) {
                int bsorID = (event.info.scoringType + 2)*10000 + event.info.lineIndex*1000 + event.info.lineLayer*100 + event.info.colorType*10 + event.info.cutDirection;
                LOG_ERROR("Could not find note for event! time: {}, bsor id: {}", event.time, bsorID);
            }
        }

        void ProcessWallEvent(const WallEvent& event) {
            Objects::obstacleChecker->headDidEnterObstacleEvent->Invoke(nullptr);
            Objects::obstacleChecker->headDidEnterObstaclesEvent->Invoke();
            float diffStartTime = std::max(wallEndTime, event.time);
            wallEndTime = std::max(wallEndTime, event.endTime);
            wallEnergyLoss += (wallEndTime - diffStartTime) * 1.3;
        }

        void UpdateTime() {
            while(event != replay->events.end() && event->time < songTime) {
                switch(event->eventType) {
                case EventRef::Note:
                    if(!notes.empty())
                        ProcessNoteEvent(replay->notes[event->index]);
                    break;
                case EventRef::Wall:
                    ProcessWallEvent(replay->walls[event->index]);
                    break;
                default:
                    break;
                }
                event++;
            }
        }
    }

    bool replaying = false;
    bool paused = false;
    ReplayWrapper currentReplay;
    IDifficultyBeatmap* beatmap = nullptr;

    const ReplayInfo& GetCurrentInfo() {
        return currentReplay.replay->info;
    }

    void SetLevel(IDifficultyBeatmap* level) {
        beatmap = level;
        if(!replaying)
            RefreshLevelReplays();
    }

    void SetReplays(std::vector<std::pair<std::string, ReplayWrapper>> replays, bool external) {
        currentReplays = replays;
        if(currentReplays.size() > 0) {
            if(!external)
                Menu::SetButtonEnabled(true);
            std::vector<std::pair<std::string, ReplayInfo*>> replayInfos;
            for(auto& pair : currentReplays)
                replayInfos.emplace_back(pair.first, &pair.second.replay->info);
            Menu::SetReplays(replayInfos, external);
        } else if(!external) {
            Menu::SetButtonEnabled(false);
            Menu::DismissMenu();
        }
    }

    bool AreReplaysLocal() {
        return Menu::AreReplaysLocal();
    }

    void RefreshLevelReplays() {
        SetReplays(GetReplays(beatmap));
    }

    void ReplayStarted(ReplayWrapper& wrapper) {
        currentReplay = wrapper;
        frameCount = currentReplay.replay->frames.size();
        bs_utils::Submission::disable(modInfo);
        replaying = true;
        paused = false;
        currentFrame = 0;
        songTime = -1;
        lerpAmount = 0;
        lastCutTime = -1;
        if(currentReplay.type & ReplayType::Event)
            Events::ReplayStarted();
        if(currentReplay.type & ReplayType::Frame)
            Frames::ReplayStarted();
        Camera::ReplayStarted();
    }

    void ReplayStarted(const std::string& path) {
        for(auto& pair : currentReplays) {
            if(pair.first == path)
                ReplayStarted(pair.second);
        }
    }

    void ReplayRestarted(bool full) {
        if(full)
            paused = false;
        currentFrame = 0;
        songTime = full ? -1 : 0;
        lerpAmount = 0;
        lastCutTime = -1;
        if(currentReplay.type & ReplayType::Event)
            Events::ReplayStarted();
        if(currentReplay.type & ReplayType::Frame)
            Frames::ReplayStarted();
    }

    void ReplayEnded(bool quit) {
        Camera::ReplayEnded();
        bs_utils::Submission::enable(modInfo);
        replaying = false;
        if(Camera::rendering && !quit) {
            // render audio after all video renders
            if(getConfig().AudioMode.GetValue()) {
                getConfig().AudioMode.SetValue(false);
                if(!getConfig().LevelsToSelect.GetValue().empty()) {
                    if(getConfig().Restart.GetValue()) {
                        getConfig().RenderLaunch.SetValue(true);
                        RestartGame();
                    } else
                        RenderLevelInConfig();
                } else if(getConfig().Ding.GetValue())
                    PlayDing();
            } else {
                getConfig().AudioMode.SetValue(true);
                RenderCurrentLevel();
            }
        }
    }

    void ReplayPaused() {
        Pause::EnsureSetup(Objects::pauseManager);
        paused = true;
        Camera::moving = false;
    }

    void ReplayUnpaused() {
        Pause::OnUnpause();
        paused = false;
    }

    void UpdateTime(float time) {
        if(songTime < 0) {
            if(time != 0)
                return;
            Objects::GetObjects();
        }
        if(currentReplay.type == ReplayType::Frame)
            time += 0.01;
        songTime = time;
        auto& frames = currentReplay.replay->frames;

        while(currentFrame < frameCount && frames[currentFrame].time <= songTime)
            currentFrame++;
        if(currentFrame > 0)
            currentFrame--;

        if(currentFrame == frameCount - 1)
            lerpAmount = 0;
        else {
            float timeDiff = songTime - frames[currentFrame].time;
            float frameDur = frames[currentFrame + 1].time - frames[currentFrame].time;
            lerpAmount = timeDiff / frameDur;
        }
        if(currentReplay.type & ReplayType::Event)
            Events::UpdateTime();
        if(currentReplay.type & ReplayType::Frame)
            Frames::UpdateTime();
        Camera::UpdateTime();
    }

    void SetLastCutTime(float value) {
        lastCutTime = value;
    }

    bool timeForwardPressed = false, timeBackPressed = false;
    bool speedUpPressed = false, slowDownPressed = false;

    void CheckInputs() {
        if(Camera::rendering)
            return;

        int timeState = IsButtonDown(getConfig().TimeButton.GetValue());
        if(timeState == 1 && !timeForwardPressed)
            Pause::SetTime(GetSongTime() + getConfig().TimeSkip.GetValue());
        else if(timeState == -1 && !timeBackPressed)
            Pause::SetTime(GetSongTime() - getConfig().TimeSkip.GetValue());
        timeForwardPressed = timeState == 1;
        timeBackPressed = timeState == -1;

        int speedState = IsButtonDown(getConfig().SpeedButton.GetValue());
        if(speedState == 1 && !speedUpPressed)
            Pause::SetSpeed(Objects::scoreController->audioTimeSyncController->timeScale + 0.1);
        else if(speedState == -1 && !slowDownPressed)
            Pause::SetSpeed(Objects::scoreController->audioTimeSyncController->timeScale - 0.1);
        speedUpPressed = speedState == 1;
        slowDownPressed = speedState == -1;

        Camera::moving = IsButtonDown(getConfig().MoveButton.GetValue());

        Camera::Move(IsButtonDown(getConfig().TravelButton.GetValue()));
    }

    float GetSongTime() {
        return songTime;
    }

    const Frame& GetFrame() {
        return currentReplay.replay->frames[currentFrame];
    }

    const Frame& GetNextFrame() {
        if(currentFrame == frameCount - 1)
            return currentReplay.replay->frames[currentFrame];
        else
            return currentReplay.replay->frames[currentFrame + 1];
    }

    float GetFrameProgress() {
        return lerpAmount;
    }
}
