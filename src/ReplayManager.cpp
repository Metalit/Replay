#include "ReplayManager.hpp"

#include "BeatSaber/Settings/Settings.hpp"
#include "Config.hpp"
#include "CustomTypes/ReplayMenu.hpp"
#include "Formats/EventReplay.hpp"
#include "Formats/FrameReplay.hpp"
#include "GlobalNamespace/AudioManagerSO.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/BoolSO.hpp"
#include "GlobalNamespace/GameEnergyUIPanel.hpp"
#include "GlobalNamespace/IRenderingParamsApplicator.hpp"
#include "GlobalNamespace/LevelCollectionNavigationController.hpp"
#include "GlobalNamespace/LevelSelectionNavigationController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/ObstacleData.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/PrepareLevelCompletionResults.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/SceneType.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/SettingsApplicatorSO.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "GlobalNamespace/StandardLevelDetailViewController.hpp"
#include "GlobalNamespace/VRRenderingParamsSetup.hpp"
#include "Main.hpp"
#include "MathUtils.hpp"
#include "MenuSelection.hpp"
#include "PauseMenu.hpp"
#include "System/Action.hpp"
#include "System/Action_1.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "UnityEngine/Application.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/QualitySettings.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRSettings.hpp"
#include "Utils.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/game.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/stats.hpp"

using namespace GlobalNamespace;

struct NoteCompare {
    constexpr bool operator()(NoteController const* const& lhs, NoteController const* const& rhs) const {
        if (lhs->_noteData->_time_k__BackingField == rhs->_noteData->_time_k__BackingField)
            return lhs < rhs;
        return lhs->_noteData->_time_k__BackingField < rhs->_noteData->_time_k__BackingField;
    }
};

std::vector<std::pair<std::string, ReplayWrapper>> currentReplays;

Quaternion ApplyTilt(Quaternion const& rotation, float tilt) {
    if (tilt == 0)
        return rotation;
    // rotate on local x axis
    return Sombrero::QuaternionMultiply(rotation, Quaternion::Euler({tilt, 0, 0}));
}

namespace Manager {

    int currentFrame = 0;
    int frameCount = 0;
    float lerpAmount = 0;
    float lastCutTime = -1;

    std::map<std::string, std::vector<CustomDataCallbackType>> customDataCallbacks;

    namespace Objects {
        bool hasObjects = false;
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
            leftSaber = sabers->First([](Saber* s) { return s->get_saberType() == SaberType::SaberA; });
            rightSaber = sabers->First([](Saber* s) { return s->get_saberType() == SaberType::SaberB; });
            obstacleChecker = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerHeadAndObstacleInteraction*>()->First();
            playerTransforms = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerTransforms*>()->First();
            pauseManager = UnityEngine::Resources::FindObjectsOfTypeAll<PauseMenuManager*>()->First();
            auto hasOtherObjects = UnityEngine::Resources::FindObjectsOfTypeAll<PrepareLevelCompletionResults*>()->First();
            scoreController = (ScoreController*) hasOtherObjects->_scoreController;
            comboController = hasOtherObjects->_comboController;
            gameEnergyCounter = hasOtherObjects->_gameEnergyCounter;
            energyBar = UnityEngine::Resources::FindObjectsOfTypeAll<GameEnergyUIPanel*>()->First();
            noteSoundManager = UnityEngine::Resources::FindObjectsOfTypeAll<NoteCutSoundEffectManager*>()->First();
            audioManager = UnityEngine::Resources::FindObjectsOfTypeAll<AudioManagerSO*>()->First();
            beatmapObjectManager = noteSoundManager->_beatmapObjectManager;
            callbackController = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapObjectSpawnController*>()->First()->_beatmapCallbacksController;
            hasObjects = true;
        }
    }

    namespace Camera {
        bool rendering = false;
        bool muxingFinished = true;
        bool moving = false;

        long lastProgressUpdate = 0;

        Vector3 smoothPosition;
        Quaternion smoothRotation;

        void UpdateThirdPerson() {
            smoothPosition = (Vector3) getConfig().ThirdPerPos.GetValue();
            smoothRotation = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
        }

        void UpdateTime() {
            if (rendering && UnityEngine::Time::get_unscaledTime() - lastProgressUpdate >= 15) {
                LOG_INFO("Current song time: {:.2f}", MetaCore::Stats::GetSongTime());
                lastProgressUpdate = UnityEngine::Time::get_unscaledTime();
            }
            if (GetMode() == (int) CameraMode::Smooth) {
                float deltaTime = UnityEngine::Time::get_deltaTime();
                smoothPosition = EaseLerp(
                    smoothPosition, GetFrame().head.position, UnityEngine::Time::get_time(), deltaTime * 2 / getConfig().Smoothing.GetValue()
                );
                smoothRotation = Slerp(smoothRotation, GetFrame().head.rotation, deltaTime * 2 / getConfig().Smoothing.GetValue());
            } else if (GetMode() == (int) CameraMode::ThirdPerson)
                UpdateThirdPerson();
        }

        void Move(int direction) {
            if (direction == 0)
                return;
            static Vector3 const move = {0, 0, 1.5};
            if (GetMode() == (int) CameraMode::ThirdPerson) {
                float delta = UnityEngine::Time::get_deltaTime() * getConfig().TravelSpeed.GetValue();
                smoothPosition += Sombrero::QuaternionMultiply(smoothRotation, move * delta * direction);
                getConfig().ThirdPerPos.SetValue(smoothPosition);
            }
        }

        Vector3 GetHeadPosition() {
            if (GetMode() == (int) CameraMode::Smooth) {
                auto offset = getConfig().Offset.GetValue();
                if (getConfig().Relative.GetValue())
                    offset = Sombrero::QuaternionMultiply(GetHeadRotation(), offset);
                return smoothPosition + offset;
            }
            return smoothPosition;
        }
        Quaternion GetHeadRotation() {
            if (GetMode() == (int) CameraMode::Smooth) {
                if (getConfig().Correction.GetValue())
                    return ApplyTilt(
                        Sombrero::QuaternionMultiply(smoothRotation, currentReplay.replay->info.averageOffset), getConfig().TargetTilt.GetValue()
                    );
            }
            return smoothRotation;
        }

        int GetMode() {
            CameraMode mode = (CameraMode) getConfig().CamMode.GetValue();
            if (mode == CameraMode::Headset && rendering)
                return (int) CameraMode::Smooth;
            return (int) mode;
        }

        void SetMode(int value) {
            CameraMode oldMode = (CameraMode) getConfig().CamMode.GetValue();
            CameraMode mode = (CameraMode) value;
            if (mode == oldMode)
                return;
            getConfig().CamMode.SetValue(value);
            if (mode == CameraMode::Smooth) {
                smoothPosition = GetFrame().head.position;
                smoothRotation = GetFrame().head.rotation;
            } else if (GetMode() == (int) CameraMode::ThirdPerson)
                UpdateThirdPerson();
        }

        void SetGraphicsSettings() {
            auto applicator = UnityEngine::Resources::FindObjectsOfTypeAll<SettingsApplicatorSO*>()->First();
            auto settings = BeatSaber::Settings::Settings();
            settings.quality.vrResolutionScale = 0.8;

            // unchanged
            settings.quality.renderViewportScale = 1;
            settings.quality.maxQueuedFrames = -1;
            settings.quality.vSyncCount = 0;
            settings.quality.targetFramerate = UnityEngine::Application::get_targetFrameRate();

            // shockwaves and walls unused in ApplyPerformancePreset

            int antiAlias = 0;
            bool distortions = (getConfig().Walls.GetValue() == 2) || (getConfig().ShockwavesOn.GetValue() && getConfig().Shockwaves.GetValue() > 0);
            if (!distortions) {
                switch (getConfig().AntiAlias.GetValue()) {
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
            }
            settings.quality.antiAliasingLevel = antiAlias;

            settings.quality.bloom = BeatSaber::Settings::QualitySettings::BloomQuality::Game;
            settings.quality.mainEffect = getConfig().Bloom.GetValue() ? BeatSaber::Settings::QualitySettings::MainEffectOption::Game
                                                                       : BeatSaber::Settings::QualitySettings::MainEffectOption::Off;
            settings.quality.mirror = getConfig().Mirrors.GetValue();

            applicator->ApplyGraphicSettings(settings, GlobalNamespace::SceneType::Game);
            LOG_INFO("applied custom graphics settings");
        }
        void UnsetGraphicsSettings() {
            auto renderSetup = UnityEngine::Resources::FindObjectsOfTypeAll<VRRenderingParamsSetup*>()->First();
            renderSetup->_applicator->Apply(GlobalNamespace::SceneType::Menu, "");
            LOG_INFO("reset graphics settings");
        }

        void ReplayStarted() {
            if (rendering) {
                SetGraphicsSettings();
                lastProgressUpdate = UnityEngine::Time::get_unscaledTime();
            }
            // muxing only needs to be done after a render
            muxingFinished = !rendering;
            if (GetMode() == (int) CameraMode::Smooth) {
                smoothRotation = GetFrame().head.rotation;
                // undo rotation by average rotation offset
                if (getConfig().Correction.GetValue()) {
                    smoothRotation = Sombrero::QuaternionMultiply(smoothRotation, currentReplay.replay->info.averageOffset);
                    smoothRotation = ApplyTilt(smoothRotation, getConfig().TargetTilt.GetValue());
                }
                // add position offset, potentially relative to rotation
                auto offset = getConfig().Offset.GetValue();
                if (getConfig().Relative.GetValue())
                    offset = Sombrero::QuaternionMultiply(smoothRotation, offset);
                smoothPosition = GetFrame().head.position + offset;
            } else if (GetMode() == (int) CameraMode::ThirdPerson)
                UpdateThirdPerson();
        }

        void ReplayEnded() {
            if (rendering)
                UnsetGraphicsSettings();
        }
    }

    namespace Frames {
        decltype(FrameReplay::scoreFrames)::iterator scoreFrame;
        ScoreFrame currentValues;
        FrameReplay* replay;

        void Increment() {
            currentValues.time = scoreFrame->time;
            if (scoreFrame->score >= 0)
                currentValues.score = scoreFrame->score;
            if (scoreFrame->percent >= 0)
                currentValues.percent = scoreFrame->percent;
            if (scoreFrame->combo >= 0)
                currentValues.combo = scoreFrame->combo;
            if (scoreFrame->energy >= 0)
                currentValues.energy = scoreFrame->energy;
            if (scoreFrame->offset >= 0)
                currentValues.offset = scoreFrame->offset;
            scoreFrame++;
        }

        void ReplayStarted() {
            replay = dynamic_cast<FrameReplay*>(currentReplay.replay.get());
            scoreFrame = replay->scoreFrames.begin();
            currentValues = {-1, -1, -1, -1, -1, -1};
            while (currentValues.score < 0 || currentValues.combo < 0 || currentValues.energy < 0 || currentValues.offset < 0)
                Increment();
        }

        void UpdateTime(float time) {
            while (scoreFrame != replay->scoreFrames.end() && scoreFrame->time < time)
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
            for (int i = 0; i < frameSearchRadius; i++) {
                iter--;
                if (iter->combo < 0)
                    i--;
                if (iter == replay->scoreFrames.begin())
                    break;
            }
            for (int i = 0; i < frameSearchRadius * 2; i++) {
                iter++;
                if (iter == replay->scoreFrames.end())
                    break;
                if (iter->combo < 0) {
                    i--;
                    continue;
                }
                if (iter->combo < combo)
                    return true;
                combo = iter->combo;
            }
            return false;
        }

        bool AllowScoreOverride() {
            if (currentValues.percent >= 0)
                return true;
            // fix scoresaber replays having incorrect max score before cut finishes
            return MetaCore::Stats::GetSongTime() - lastCutTime > 0.4;
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
            if (note->noteData->scoringType > NoteData::ScoringType::NoScore || note->noteData->gameplayType == NoteData::GameplayType::Bomb)
                notes.insert(note);
        }
        void RemoveNoteController(NoteController* note) {
            auto iter = notes.find(note);
            if (iter != notes.end())
                notes.erase(iter);
        }

        void ProcessNoteEvent(NoteEvent const& event) {
            static auto sendCut = il2cpp_utils::FindMethodUnsafe(classof(NoteController*), "SendNoteWasCutEvent", 1);

            auto& info = event.info;
            bool found = false;
            for (auto iter = notes.begin(); iter != notes.end(); iter++) {
                auto controller = *iter;
                auto noteData = controller->noteData;
                if (((int) noteData->scoringType == info.scoringType || info.scoringType == -2) && (int) noteData->lineIndex == info.lineIndex &&
                    (int) noteData->noteLineLayer == info.lineLayer && (int) noteData->colorType == info.colorType &&
                    (int) noteData->cutDirection == info.cutDirection) {
                    found = true;
                    bool isLeftSaber = event.noteCutInfo.saberType == (int) SaberType::SaberA;
                    Saber* saber = isLeftSaber ? Objects::leftSaber : Objects::rightSaber;
                    if (info.eventType == NoteEventInfo::Type::GOOD || info.eventType == NoteEventInfo::Type::BAD) {
                        auto cutInfo = GetNoteCutInfo(controller, saber, event.noteCutInfo);
                        if (replay->cutInfoMissingOKs) {
                            cutInfo.speedOK = cutInfo.saberSpeed > 2;
                            bool isLeftColor = noteData->colorType == ColorType::ColorA;
                            cutInfo.saberTypeOK = isLeftColor == isLeftSaber;
                            cutInfo.timeDeviation = noteData->time - event.time;
                        }
                        SetLastCutTime(event.time);
                        il2cpp_utils::RunMethod<void, false>(controller, sendCut, byref(cutInfo));
                    } else if (info.eventType == NoteEventInfo::Type::MISS) {
                        controller->SendNoteWasMissedEvent();
                        notes.erase(iter);  // note will despawn and be removed in the other cases
                    } else if (info.eventType == NoteEventInfo::Type::BOMB) {
                        auto cutInfo = GetBombCutInfo(controller, saber);
                        il2cpp_utils::RunMethod<void, false>(controller, sendCut, byref(cutInfo));
                    }
                    break;
                }
            }
            if (!found) {
                int bsorID = (event.info.scoringType + 2) * 10000 + event.info.lineIndex * 1000 + event.info.lineLayer * 100 +
                             event.info.colorType * 10 + event.info.cutDirection;
                LOG_ERROR("Could not find note for event! time: {}, bsor id: {}", event.time, bsorID);
            }
        }

        void ProcessWallEvent(WallEvent const& event) {
            Objects::obstacleChecker->headDidEnterObstacleEvent->Invoke(nullptr);
            Objects::obstacleChecker->headDidEnterObstaclesEvent->Invoke();
            float diffStartTime = std::max(wallEndTime, event.time);
            wallEndTime = std::max(wallEndTime, event.endTime);
            wallEnergyLoss += (wallEndTime - diffStartTime) * 1.3;
        }

        void UpdateTime(float time) {
            while (event != replay->events.end() && event->time < time) {
                switch (event->eventType) {
                    case EventRef::Note:
                        if (!notes.empty())
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

    ReplayInfo const& GetCurrentInfo() {
        return currentReplay.replay->info;
    }

    ON_EVENT(MetaCore::Events::MapSelected) {
        Menu::EnsureSetup(MetaCore::Game::GetMainFlowCoordinator()
                              ->_soloFreePlayFlowCoordinator->levelSelectionNavigationController->_levelCollectionNavigationController
                              ->_levelDetailViewController->_standardLevelDetailView);
        RefreshLevelReplays();
        Menu::CheckMultiplayer();
    }

    void SetReplays(std::vector<std::pair<std::string, ReplayWrapper>> replays, bool external) {
        LOG_DEBUG("setting {} replays {}", replays.size(), external);
        currentReplays = replays;
        if (currentReplays.size() > 0) {
            if (!external)
                Menu::SetButtonEnabled(true);
            std::vector<std::pair<std::string, ReplayInfo*>> replayInfos;
            for (auto& pair : currentReplays)
                replayInfos.emplace_back(pair.first, &pair.second.replay->info);
            Menu::SetReplays(replayInfos, external);
        } else if (!external) {
            Menu::SetButtonEnabled(false);
            Menu::DismissMenu();
        }
    }

    bool AreReplaysLocal() {
        return Menu::AreReplaysLocal();
    }

    void RefreshLevelReplays() {
        LOG_DEBUG("Refresh replays");
        SetReplays(GetReplays(MetaCore::Songs::GetSelectedKey()));
    }

    void ReplayStarted(ReplayWrapper& wrapper) {
        LOG_INFO("Replay started");
        currentReplay = wrapper;
        frameCount = currentReplay.replay->frames.size();
        MetaCore::Game::SetScoreSubmission(MOD_ID, false);
        Objects::hasObjects = false;
        replaying = true;
        paused = false;
        currentFrame = 0;
        lerpAmount = 0;
        lastCutTime = -1;
        if (currentReplay.type & ReplayType::Event)
            Events::ReplayStarted();
        if (currentReplay.type & ReplayType::Frame)
            Frames::ReplayStarted();
        Camera::ReplayStarted();

        for (auto& pair : customDataCallbacks) {
            std::string const& key = pair.first;
            std::vector<CustomDataCallbackType>& callbacks = pair.second;

            char const* data = nullptr;
            size_t length = 0;
            auto eventReplay = dynamic_cast<EventReplay*>(currentReplay.replay.get());
            if (eventReplay) {
                auto data_it = eventReplay->customDatas.find(key);
                if (data_it != eventReplay->customDatas.end()) {
                    std::vector<char> const& data_vec = data_it->second;
                    data = data_vec.data();
                    length = data_vec.size();
                }
            }
            for (int idx = 0; idx < callbacks.size(); idx++) {
                callbacks[idx](data, length);
            }
        }
    }

    void ReplayStarted(std::string const& path) {
        for (auto& pair : currentReplays) {
            if (pair.first == path)
                ReplayStarted(pair.second);
        }
    }

    void ReplayRestarted(bool full) {
        LOG_INFO("Replay restarted {}", full);
        if (full) {
            paused = false;
            Objects::hasObjects = false;
        }
        currentFrame = 0;
        lerpAmount = 0;
        lastCutTime = -1;
        if (currentReplay.type & ReplayType::Event)
            Events::ReplayStarted();
        if (currentReplay.type & ReplayType::Frame)
            Frames::ReplayStarted();
        Camera::ReplayStarted();
    }

    ON_EVENT(MetaCore::Events::MapRestarted) {
        if (!replaying)
            return;
        ReplayRestarted();
    }

    void ReplayEnded(bool quit) {
        LOG_INFO("Replay ended");
        Camera::ReplayEnded();
        MetaCore::Game::SetScoreSubmission(MOD_ID, true);
        replaying = false;
        if (Camera::rendering && !quit) {
            if (!getConfig().LevelsToSelect.GetValue().empty()) {
                if (getConfig().Restart.GetValue()) {
                    getConfig().RenderLaunch.SetValue(true);
                    RestartGame();
                } else
                    RenderLevelInConfig();
            } else if (getConfig().Ding.GetValue())
                PlayDing();
        }
    }

    ON_EVENT(MetaCore::Events::MapPaused) {
        if (!replaying)
            return;
        Pause::EnsureSetup(Objects::pauseManager);
        paused = true;
        Camera::moving = false;
    }

    ON_EVENT(MetaCore::Events::MapUnpaused) {
        if (!replaying)
            return;
        Pause::OnUnpause();
        paused = false;
    }

    void UpdateTime(float time) {
        if (!Objects::hasObjects)
            Objects::GetObjects();
        if (currentReplay.type == ReplayType::Frame)
            time += 0.01;
        auto& frames = currentReplay.replay->frames;

        while (currentFrame < frameCount && frames[currentFrame].time <= time)
            currentFrame++;
        if (currentFrame > 0)
            currentFrame--;

        if (currentFrame == frameCount - 1)
            lerpAmount = 0;
        else {
            float timeDiff = time - frames[currentFrame].time;
            float frameDur = frames[currentFrame + 1].time - frames[currentFrame].time;
            lerpAmount = timeDiff / frameDur;
        }
        if (currentReplay.type & ReplayType::Event)
            Events::UpdateTime(time);
        if (currentReplay.type & ReplayType::Frame)
            Frames::UpdateTime(time);
        Camera::UpdateTime();
    }

    void SetLastCutTime(float value) {
        lastCutTime = value;
    }

    bool timeForwardPressed = false, timeBackPressed = false;
    bool speedUpPressed = false, slowDownPressed = false;

    void CheckInputs() {
        if (Camera::rendering)
            return;

        int timeState = IsButtonDown(getConfig().TimeButton.GetValue());
        if (timeState == 1 && !timeForwardPressed)
            Pause::SetTime(MetaCore::Stats::GetSongTime() + getConfig().TimeSkip.GetValue());
        else if (timeState == -1 && !timeBackPressed)
            Pause::SetTime(MetaCore::Stats::GetSongTime() - getConfig().TimeSkip.GetValue());
        timeForwardPressed = timeState == 1;
        timeBackPressed = timeState == -1;

        int speedState = IsButtonDown(getConfig().SpeedButton.GetValue());
        if (speedState == 1 && !speedUpPressed)
            Pause::SetSpeed(Objects::scoreController->_audioTimeSyncController->timeScale + 0.1);
        else if (speedState == -1 && !slowDownPressed)
            Pause::SetSpeed(Objects::scoreController->_audioTimeSyncController->timeScale - 0.1);
        speedUpPressed = speedState == 1;
        slowDownPressed = speedState == -1;

        Camera::moving = IsButtonDown(getConfig().MoveButton.GetValue());

        Camera::Move(IsButtonDown(getConfig().TravelButton.GetValue()));
    }

    ON_EVENT(MetaCore::Events::Update) {
        if (!replaying)
            return;
        UpdateTime(MetaCore::Stats::GetSongTime());
        CheckInputs();
    }

    float GetLength() {
        if (GetCurrentInfo().failed)
            return GetCurrentInfo().failTime;
        return MetaCore::Stats::GetSongLength();
    }

    Frame const& GetFrame() {
        return currentReplay.replay->frames[currentFrame];
    }

    Frame const& GetNextFrame() {
        if (currentFrame == frameCount - 1)
            return currentReplay.replay->frames[currentFrame];
        else
            return currentReplay.replay->frames[currentFrame + 1];
    }

    float GetFrameProgress() {
        return lerpAmount;
    }
}
