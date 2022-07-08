#pragma once

#include "Utils.hpp"

#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/ObstacleController.hpp"

struct ScoreFrame;
struct NoteEvent;
struct WallEvent;
struct HeightEvent;
struct Pause;

namespace Manager {
    
    namespace Frames {
        ScoreFrame* GetScoreFrame();
    }
    
    namespace Events {
        void AddNoteController(GlobalNamespace::NoteController* note);
        void AddObstacleController(GlobalNamespace::ObstacleController* wall);
        void ObstacleControllerFinished(GlobalNamespace::ObstacleController* wall);
    }

    void SetLevel(GlobalNamespace::IDifficultyBeatmap* level);
    
    void RefreshLevelReplays();
    
    void ReplayStarted(const std::string& path);
    void ReplayRestarted();
    void ReplayEnded();

    extern bool replaying;
    extern bool paused;
    extern ReplayWrapper currentReplay;

    void UpdateTime(float songTime);
    Frame& GetFrame();
    Frame& GetNextFrame();
    float GetFrameProgress();
}
