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

    namespace Camera {
        enum struct Mode {
            HEADSET,
            SMOOTH,
            THIRDPERSON
        };

        extern Mode mode;

        const Vector3& GetHeadPosition();
        const Quaternion& GetHeadRotation();
    }
    
    namespace Frames {
        ScoreFrame* GetScoreFrame();
        bool AllowComboDrop();
    }
    
    namespace Events {
        void AddNoteController(GlobalNamespace::NoteController* note);
        float& GetWallEnergyLoss();
    }

    void SetLevel(GlobalNamespace::IDifficultyBeatmap* level);
    
    void RefreshLevelReplays();
    
    void ReplayStarted(ReplayWrapper& wrapper);
    void ReplayStarted(const std::string& path);
    void ReplayRestarted();
    void ReplayEnded();

    extern bool replaying;
    extern bool paused;
    extern ReplayWrapper currentReplay;

    void UpdateTime(float songTime);
    float GetSongTime();
    Frame& GetFrame();
    Frame& GetNextFrame();
    float GetFrameProgress();
}
