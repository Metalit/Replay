#pragma once

#include "Utils.hpp"

#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/ObstacleController.hpp"

struct ScoreFrame;
struct NoteEvent;
struct WallEvent;
struct HeightEvent;
struct PauseEvent;

namespace Manager {

    namespace Camera {
        enum struct Mode {
            HEADSET,
            SMOOTH,
            THIRDPERSON
        };

        extern Mode mode;
        extern bool rendering;

        Vector3 GetHeadPosition();
        Quaternion GetHeadRotation();
        Mode GetMode();
    }
    
    namespace Frames {
        ScoreFrame* GetScoreFrame();
        bool AllowComboDrop();
    }
    
    namespace Events {
        void AddNoteController(GlobalNamespace::NoteController* note);
        void RemoveNoteController(GlobalNamespace::NoteController* note);
        float& GetWallEnergyLoss();
    }

    void SetLevel(GlobalNamespace::IDifficultyBeatmap* level);
    
    void SetReplays(std::unordered_map<std::string, ReplayWrapper> replays, bool external = false);
    void RefreshLevelReplays();
    
    void ReplayStarted(ReplayWrapper& wrapper);
    void ReplayStarted(const std::string& path);
    void ReplayRestarted(bool full = true);
    void EndSceneChangeStarted();
    void ReplayEnded();
    void ReplayPaused();
    void ReplayUnpaused();

    extern bool replaying;
    extern bool paused;
    extern ReplayWrapper currentReplay;
    extern GlobalNamespace::IDifficultyBeatmap* beatmap;

    void UpdateTime(float songTime);
    float GetSongTime();
    Frame& GetFrame();
    Frame& GetNextFrame();
    float GetFrameProgress();

    Vector3 GetPosOffset(Vector3 pos, bool head = false);
    Quaternion GetRotOffset(Quaternion rot, bool head = false);
}
