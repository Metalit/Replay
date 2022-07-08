#include "Main.hpp"
#include "ReplayManager.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "ReplayMenu.hpp"

#include "bs-utils/shared/utils.hpp"

#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/ObstacleData.hpp"
#include "GlobalNamespace/Saber.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Resources.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"

using namespace GlobalNamespace;

IDifficultyBeatmap* beatmap = nullptr;
std::unordered_map<std::string, ReplayWrapper> currentReplays;

// TODO: move this and event resolution code
#include "CustomTypes/MovementData.hpp"

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

namespace Manager {

    int currentFrame = 0;
    int frameCount = 0;
    float songTime = 0;
    float lerpAmount = 0;

    Saber *leftSaber, *rightSaber;
    PlayerHeadAndObstacleInteraction* obstacleChecker;

    void GetObjects() {
        auto sabers = UnityEngine::Resources::FindObjectsOfTypeAll<Saber*>();
        leftSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberA; });
        rightSaber = sabers.First([](Saber* s) { return s->get_saberType() == SaberType::SaberB; });
        obstacleChecker = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerHeadAndObstacleInteraction*>().First();
    }

    namespace Frames {
        ScoreFrame* GetScoreFrame() {
            return &((FrameReplay*) currentReplay.replay.get())->scoreFrames[currentFrame];
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
        
        void UpdateTime() {
            while(noteEvent != ((EventReplay*) currentReplay.replay.get())->notes.end() && noteEvent->time < songTime) {
                auto& info = noteEvent->info;
                for(auto iter = notes.begin(); iter != notes.end(); iter++) {
                    auto noteData = (*iter)->noteData;
                    if((noteData->scoringType == info.scoringType || info.scoringType == -2)
                            && noteData->lineIndex == info.lineIndex
                            && noteData->noteLineLayer == info.lineLayer
                            && noteData->colorType == info.colorType
                            && noteData->cutDirection == info.cutDirection) {
                        auto saber = noteEvent->noteCutInfo.saberType == SaberType::SaberA ? leftSaber : rightSaber;
                        if(info.eventType == NoteEventInfo::Type::GOOD || info.eventType == NoteEventInfo::Type::BAD) {
                            auto cutInfo = GetNoteCutInfo(*iter, saber, noteEvent->noteCutInfo);
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
                noteEvent++;
            }
            while(wallEvent != ((EventReplay*) currentReplay.replay.get())->walls.end() && wallEvent->time < songTime) {
                for(auto iter = walls.begin(); iter != walls.end(); iter++) {
                    auto wallData = (*iter)->obstacleData;
                    if(wallData->lineIndex == wallEvent->lineIndex
                            && wallData->type == wallEvent->obstacleType
                            && wallData->width == wallEvent->width) {
                        obstacleChecker->intersectingObstacles->AddIfNotPresent(*iter);
                        currentWalls.insert(*iter);
                        walls.erase(iter);
                        break;
                    }
                }
                wallEvent++;
            }
        }
    }
    
    bool replaying = false;
    bool paused = false;
    ReplayWrapper currentReplay;

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

    void ReplayStarted(const std::string& path) {
        auto replay = currentReplays.find(path);
        if(replay != currentReplays.end()) {
            currentReplay = replay->second;
            frameCount = currentReplay.replay->frames.size();
        } else
            return;
        bs_utils::Submission::disable(modInfo);
        replaying = true;
        paused = false;
        currentFrame = 0;
        songTime = 0;
        if(currentReplay.type == ReplayType::Event)
            Events::ReplayStarted();
    }

    void ReplayRestarted() {
        paused = false;
        currentFrame = 0;
        songTime = 0;
        if(currentReplay.type == ReplayType::Event)
            Events::ReplayStarted();
    }

    void ReplayEnded() {
        bs_utils::Submission::enable(modInfo);
        replaying = false;
    }
    
    void UpdateTime(float time) {
        if(songTime == 0)
            GetObjects();
        songTime = time;
        auto& frames = currentReplay.replay->frames;

        while(frames[currentFrame].time < songTime && currentFrame < frameCount)
            currentFrame++;
        
        if(currentFrame == frameCount - 1)
            lerpAmount = 0;
        else {
            if(currentReplay.type == ReplayType::Frame)
                currentFrame++;
            float timeDiff = songTime - frames[currentFrame].time;
            float frameDur = frames[currentFrame + 1].time - frames[currentFrame].time;
            lerpAmount = timeDiff / frameDur;
        }
        if(currentReplay.type == ReplayType::Event)
            Events::UpdateTime();
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
