#pragma once

#include "Utils.hpp"

#include "GlobalNamespace/IDifficultyBeatmap.hpp"

struct ScoreFrame;
struct NoteEvent;
struct WallEvent;
struct HeightEvent;
struct Pause;

namespace Manager {
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
    
    namespace Frames {
        ScoreFrame* GetScoreFrame();
    }
    
    namespace Events {
        
    }
}
