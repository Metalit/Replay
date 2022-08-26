#include "Main.hpp"
#include "Config.hpp"
#include "ReplayManager.hpp"
#include "MathUtils.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "ReplayMenu.hpp"
#include "CustomTypes/MovementData.hpp"

#include "bs-utils/shared/utils.hpp"

#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/ObstacleData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "GlobalNamespace/MainSettingsModelSO.hpp"
#include "GlobalNamespace/IntSO.hpp"
#include "GlobalNamespace/BoolSO.hpp"
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

std::unordered_map<std::string, ReplayWrapper> currentReplays;

namespace Manager {

    int currentFrame = 0;
    int frameCount = 0;
    float songTime = -1;
    float lerpAmount = 0;
    bool inTransition = false;

    Saber *leftSaber, *rightSaber;
    PlayerHeadAndObstacleInteraction* obstacleChecker;
    PlayerTransforms* playerTransforms;

    void GetObjects() {
        auto sabers = UnityEngine::Resources::FindObjectsOfTypeAll<Saber*>();
        leftSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberA; });
        rightSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberB; });
        obstacleChecker = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerHeadAndObstacleInteraction*>().First();
        playerTransforms = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerTransforms*>().First();
    }

    namespace Camera {
        Mode mode = Mode::HEADSET;
        bool rendering = false;

        Vector3 smoothPosition;
        Quaternion smoothRotation;

        void UpdateTime() {
            float deltaTime = UnityEngine::Time::get_deltaTime();
            smoothPosition = EaseLerp(smoothPosition, GetFrame().head.position, UnityEngine::Time::get_time(), deltaTime * 2 / getConfig().Smoothing.GetValue());
            smoothRotation = Slerp(smoothRotation, GetFrame().head.rotation, deltaTime * 2 / getConfig().Smoothing.GetValue());
        }

        Vector3 GetHeadPosition() {
            return GetPosOffset(smoothPosition, true);
        }
        Quaternion GetHeadRotation() {
            return GetRotOffset(smoothRotation, true);
        }

        Mode GetMode() {
            if(mode == Mode::HEADSET && rendering)
                return Mode::SMOOTH;
            return mode;
        }

        void SetGraphicsSettings() {
            auto settings = UnityEngine::Resources::FindObjectsOfTypeAll<MainSettingsModelSO*>().First();
            settings->maxShockwaveParticles->set_value(getConfig().Shockwaves.GetValue() ? 1 : 0);
            settings->screenDisplacementEffectsEnabled->set_value(getConfig().Walls.GetValue());
        }
        void UnsetGraphicsSettings() {
            auto settings = UnityEngine::Resources::FindObjectsOfTypeAll<MainSettingsModelSO*>().First();
            settings->maxShockwaveParticles->set_value(0);
            settings->screenDisplacementEffectsEnabled->set_value(false);
        }

        void ReplayStarted() {
            if(rendering)
                SetGraphicsSettings();
            smoothRotation = GetFrame().head.rotation;
            // undo rotation by average rotation offset
            if(getConfig().Correction.GetValue())
                smoothRotation = Sombrero::QuaternionMultiply(smoothRotation, currentReplay.replay->info.averageOffset);
            // add position offset, potentially relative to rotation
            auto offset = getConfig().Offset.GetValue();
            if(getConfig().Relative.GetValue())
                offset = Sombrero::QuaternionMultiply(smoothRotation, offset);
            smoothPosition = GetFrame().head.position + offset;
        }

        void ReplayEnded() {
            if(rendering)
                UnsetGraphicsSettings();
        }
    }

    namespace Frames {
        ScoreFrame* GetScoreFrame() {
            return &((FrameReplay*) currentReplay.replay.get())->scoreFrames[currentFrame];
        }

        bool AllowComboDrop() {
            static int searchRange = 3;
            int combo = 1;
            auto& frames = ((FrameReplay*) currentReplay.replay.get())->scoreFrames;
            for(int i = std::max(0, currentFrame - searchRange); i < std::min(frameCount, currentFrame + searchRange); i++) {
                if(frames[i].combo < combo)
                    return true;
                combo = frames[i].combo;
            }
            return false;
        }
    }
    
    namespace Events {
        std::set<NoteController*, NoteCompare> notes;
        std::vector<NoteEvent>::iterator noteEvent;
        float wallEndTime = 0;
        float wallEnergyLoss = 0;
        std::vector<WallEvent>::iterator wallEvent;

        void ReplayStarted() {
            notes.clear();
            noteEvent = ((EventReplay*) currentReplay.replay.get())->notes.begin();
            wallEndTime = 0;
            wallEnergyLoss = 0;
            wallEvent = ((EventReplay*) currentReplay.replay.get())->walls.begin();
        }

        void AddNoteController(NoteController* note) {
            if(note->noteData->scoringType > NoteData::ScoringType::NoScore)
                notes.insert(note);
        }
        void RemoveNoteController(NoteController* note) {
            auto iter = notes.find(note);
            if(iter != notes.end())
                notes.erase(iter);
        }
        float& GetWallEnergyLoss() {
            return wallEnergyLoss;
        }

        NoteCutInfo GetNoteCutInfo(NoteController* note, Saber* saber, ReplayNoteCutInfo info) {
            return NoteCutInfo(note->noteData,
                info.speedOK,
                info.directionOK,
                info.saberTypeOK,
                info.wasCutTooSoon,
                info.saberSpeed,
                info.saberDir,
                info.saberType,
                info.timeDeviation,
                info.cutDirDeviation,
                info.cutPoint,
                info.cutNormal,
                info.cutAngle,
                info.cutDistanceToCenter,
                note->get_worldRotation(),
                note->get_inverseWorldRotation(),
                note->noteTransform->get_rotation(),
                note->noteTransform->get_position(),
                MakeFakeMovementData((ISaberMovementData*) saber->movementData, info.beforeCutRating, info.afterCutRating)
            );
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
                    auto saber = event.noteCutInfo.saberType == SaberType::SaberA ? leftSaber : rightSaber;
                    if(info.eventType == NoteEventInfo::Type::GOOD || info.eventType == NoteEventInfo::Type::BAD) {
                        auto cutInfo = GetNoteCutInfo(controller, saber, event.noteCutInfo);
                        il2cpp_utils::RunMethodUnsafe(controller, "SendNoteWasCutEvent", byref(cutInfo));
                    } else if(info.eventType == NoteEventInfo::Type::MISS) {
                        controller->SendNoteWasMissedEvent();
                        notes.erase(iter); // note will despawn and be removed in the other cases
                    } else if(info.eventType == NoteEventInfo::Type::BOMB) {
                        il2cpp_utils::RunMethodUnsafe(*iter, "HandleWasCutBySaber", saber,controller->get_transform()->get_position(), Quaternion::identity(), Vector3::up());
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
            obstacleChecker->headDidEnterObstacleEvent->Invoke(nullptr);
            obstacleChecker->headDidEnterObstaclesEvent->Invoke();
            float diffStartTime = std::max(wallEndTime, event.time);
            wallEndTime = std::max(wallEndTime, event.endTime);
            wallEnergyLoss += (wallEndTime - diffStartTime) * 1.3;
        }
        
        void UpdateTime() {
            while(noteEvent != ((EventReplay*) currentReplay.replay.get())->notes.end() && noteEvent->time < songTime) {
                ProcessNoteEvent(*noteEvent);
                noteEvent++;
            }
            while(wallEvent != ((EventReplay*) currentReplay.replay.get())->walls.end() && wallEvent->time < songTime) {
                ProcessWallEvent(*wallEvent);
                wallEvent++;
            }
        }
    }
    
    bool replaying = false;
    bool paused = false;
    ReplayWrapper currentReplay;
    IDifficultyBeatmap* beatmap = nullptr;

    void SetLevel(IDifficultyBeatmap* level) {
        beatmap = level;
        RefreshLevelReplays();
    }

    void SetReplays(std::unordered_map<std::string, ReplayWrapper> replays, bool external) {
        currentReplays = replays;
        if(currentReplays.size() > 0) {
            if(!external)
                Menu::SetButtonEnabled(true);
            std::vector<std::string> paths;
            std::vector<ReplayInfo*> infos;
            for(auto& pair : currentReplays) {
                paths.emplace_back(pair.first);
                infos.emplace_back(&pair.second.replay->info);
            }
            Menu::SetReplays(infos, paths, external);
        } else if(!external) {
            Menu::SetButtonEnabled(false);
            Menu::DismissMenu();
        }
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
        if(currentReplay.type == ReplayType::Event)
            Events::ReplayStarted();
        Camera::ReplayStarted();
    }

    void ReplayStarted(const std::string& path) {
        auto replay = currentReplays.find(path);
        if(replay != currentReplays.end())
            ReplayStarted(replay->second);
        else
            return;
    }

    void ReplayRestarted() {
        paused = false;
        currentFrame = 0;
        songTime = -1;
        if(currentReplay.type == ReplayType::Event)
            Events::ReplayStarted();
    }

    void EndSceneChangeStarted() {
        inTransition = true;
    }

    void ReplayEnded() {
        Camera::ReplayEnded();
        bs_utils::Submission::enable(modInfo);
        replaying = false;
        inTransition = false;
    }
    
    void UpdateTime(float time) {
        if(songTime < 0)
            GetObjects();
        songTime = time;
        auto& frames = currentReplay.replay->frames;

        while(frames[currentFrame].time <= songTime && currentFrame < frameCount - 1)
            currentFrame++;
        
        if(currentFrame == frameCount - 1)
            lerpAmount = 0;
        else {
            float timeDiff = songTime - frames[currentFrame].time;
            float frameDur = frames[currentFrame + 1].time - frames[currentFrame].time;
            lerpAmount = timeDiff / frameDur;
        }
        if(currentReplay.type == ReplayType::Event)
            Events::UpdateTime();
        Camera::UpdateTime();
    }

    float GetSongTime() {
        return songTime;
    }

    Frame& GetFrame() {
        return currentReplay.replay->frames[currentFrame];
    }

    Frame& GetNextFrame() {
        if(currentFrame == frameCount - 1)
            return currentReplay.replay->frames[currentFrame];
        else
            return currentReplay.replay->frames[currentFrame + 1];
    }

    float GetFrameProgress() {
        return lerpAmount;
    }

    Vector3 GetPosOffset(Vector3 pos, bool head) {
        if(head) {
            auto offset = getConfig().Offset.GetValue();
            if(getConfig().Relative.GetValue())
                offset = Sombrero::QuaternionMultiply(Camera::GetHeadRotation(), offset);
            pos += offset;
        }
        if(!inTransition && playerTransforms->useOriginParentTransformForPseudoLocalCalculations && currentReplay.type == ReplayType::Event)
            return playerTransforms->originParentTransform->get_position() + pos;
        return pos;
    }
    Quaternion GetRotOffset(Quaternion rot, bool head) {
        if(head) {
            if(getConfig().Correction.GetValue())
                rot = Sombrero::QuaternionMultiply(rot, currentReplay.replay->info.averageOffset);
        }
        if(!inTransition && playerTransforms->useOriginParentTransformForPseudoLocalCalculations && currentReplay.type == ReplayType::Event)
            return Sombrero::QuaternionMultiply(playerTransforms->originParentTransform->get_rotation(), rot);
        return rot;
    }
}
