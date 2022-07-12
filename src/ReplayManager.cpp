#include "Main.hpp"
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
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Time.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"

using namespace GlobalNamespace;

std::unordered_map<std::string, ReplayWrapper> currentReplays;

namespace Manager {

    int currentFrame = 0;
    int frameCount = 0;
    float songTime = -1;
    float lerpAmount = 0;

    Saber *leftSaber, *rightSaber;
    PlayerHeadAndObstacleInteraction* obstacleChecker;

    void GetObjects() {
        auto sabers = UnityEngine::Resources::FindObjectsOfTypeAll<Saber*>();
        leftSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberA; });
        rightSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberB; });
        obstacleChecker = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerHeadAndObstacleInteraction*>().First();
    }

    namespace Camera {
        Mode mode = Mode::HEADSET;
        float zOffset = -0.5;
        float smoothing = 1;

        Vector3 smoothPosition;
        Quaternion smoothRotation;

        const Vector3& GetHeadPosition() {
            float deltaTime = UnityEngine::Time::get_deltaTime();
            smoothPosition = EaseLerp(smoothPosition, GetFrame().head.position + UnityEngine::Vector3{0, 0, zOffset}, UnityEngine::Time::get_time(), deltaTime * 2 / smoothing);
            return smoothPosition;
        }
        const Quaternion& GetHeadRotation() {
            float deltaTime = UnityEngine::Time::get_deltaTime();
            smoothRotation = Slerp(smoothRotation, GetFrame().head.rotation, deltaTime * 2 / smoothing);
            return smoothRotation;
        }

        void ReplayStarted() {
            smoothPosition = GetFrame().head.position + UnityEngine::Vector3{0, 0, zOffset};
            smoothRotation = GetFrame().head.rotation;
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
        std::vector<NoteController*> notes;
        std::vector<NoteEvent>::iterator noteEvent;
        std::vector<ObstacleController*> walls;
        std::vector<WallEvent>::iterator wallEvent;
        std::set<ObstacleController*> currentWalls;

        void ReplayStarted() {
            notes.clear();
            noteEvent = ((EventReplay*) currentReplay.replay.get())->notes.begin();
            walls.clear();
            currentWalls.clear();
            wallEvent = ((EventReplay*) currentReplay.replay.get())->walls.begin();
        }

        void AddNoteController(NoteController* note) {
            notes.push_back(note);
        }
        void AddObstacleController(ObstacleController* wall) {
            walls.push_back(wall);
        }
        void ObstacleControllerFinished(ObstacleController* wall) {
            auto iter = currentWalls.find(wall);
            if(iter != currentWalls.end()) {
                currentWalls.erase(iter);
                obstacleChecker->intersectingObstacles->Remove(wall);
            }
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
            for(auto iter = notes.begin(); iter != notes.end(); iter++) {
                auto noteData = (*iter)->noteData;
                if((noteData->scoringType == info.scoringType || info.scoringType == -2)
                        && noteData->lineIndex == info.lineIndex
                        && noteData->noteLineLayer == info.lineLayer
                        && noteData->colorType == info.colorType
                        && noteData->cutDirection == info.cutDirection) {
                    auto saber = event.noteCutInfo.saberType == SaberType::SaberA ? leftSaber : rightSaber;
                    if(info.eventType == NoteEventInfo::Type::GOOD || info.eventType == NoteEventInfo::Type::BAD) {
                        auto cutInfo = GetNoteCutInfo(*iter, saber, event.noteCutInfo);
                        il2cpp_utils::RunMethodUnsafe(*iter, "SendNoteWasCutEvent", byref(cutInfo));
                    } else if(info.eventType == NoteEventInfo::Type::MISS) {
                        (*iter)->SendNoteWasMissedEvent();
                    } else if(info.eventType == NoteEventInfo::Type::BOMB) {
                        il2cpp_utils::RunMethodUnsafe(*iter, "HandleWasCutBySaber", saber,
                            (*iter)->get_transform()->get_position(), Quaternion::identity(), Vector3::up());
                    }
                    notes.erase(iter);
                    break;
                }
            }
        }

        void ProcessWallEvent(const WallEvent& event) {
            for(auto iter = walls.begin(); iter != walls.end(); iter++) {
                auto wallData = (*iter)->obstacleData;
                if(wallData->lineIndex == event.lineIndex
                        && wallData->type == event.obstacleType
                        && wallData->width == event.width) {
                    obstacleChecker->intersectingObstacles->AddIfNotPresent(*iter);
                    currentWalls.insert(*iter);
                    walls.erase(iter);
                    break;
                }
            }
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

    void RefreshLevelReplays() {
        currentReplays = GetReplays(beatmap);
        if(currentReplays.size() > 0) {
            Menu::SetButtonEnabled(true);
            std::vector<std::string> paths;
            std::vector<ReplayInfo*> infos;
            for(auto& pair : currentReplays) {
                paths.emplace_back(pair.first);
                infos.emplace_back(&pair.second.replay->info);
            }
            Menu::SetReplays(infos, paths);
        } else
            Menu::SetButtonEnabled(false);
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

    void ReplayEnded() {
        bs_utils::Submission::enable(modInfo);
        replaying = false;
    }
    
    void UpdateTime(float time) {
        if(songTime < 0)
            GetObjects();
        songTime = time;
        auto& frames = currentReplay.replay->frames;

        while(frames[currentFrame].time <= songTime && currentFrame < frameCount)
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
}
