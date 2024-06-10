#pragma once

#include "GlobalNamespace/BeatmapKey.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/ObstacleController.hpp"
#include "Utils.hpp"

struct ScoreFrame;
struct NoteEvent;
struct WallEvent;
struct HeightEvent;
struct PauseEvent;

namespace GlobalNamespace {
    class Saber;
    class PlayerHeadAndObstacleInteraction;
    class PlayerTransforms;
    class PauseMenuManager;
    class ScoreController;
    class ComboController;
    class GameEnergyCounter;
    class GameEnergyUIPanel;
    class NoteCutSoundEffectManager;
    class AudioManagerSO;
    class BeatmapObjectManager;
    class BeatmapCallbacksController;
}

namespace Manager {

    namespace Objects {
        extern GlobalNamespace::Saber *leftSaber, *rightSaber;
        extern GlobalNamespace::PlayerHeadAndObstacleInteraction* obstacleChecker;
        extern GlobalNamespace::PlayerTransforms* playerTransforms;
        extern GlobalNamespace::PauseMenuManager* pauseManager;
        extern GlobalNamespace::ScoreController* scoreController;
        extern GlobalNamespace::ComboController* comboController;
        extern GlobalNamespace::GameEnergyCounter* gameEnergyCounter;
        extern GlobalNamespace::GameEnergyUIPanel* energyBar;
        extern GlobalNamespace::NoteCutSoundEffectManager* noteSoundManager;
        extern GlobalNamespace::AudioManagerSO* audioManager;
        extern GlobalNamespace::BeatmapObjectManager* beatmapObjectManager;
        extern GlobalNamespace::BeatmapCallbacksController* callbackController;
    }

    namespace Camera {
        extern bool rendering;
        extern bool muxingFinished;
        extern bool moving;

        Vector3 GetHeadPosition();
        Quaternion GetHeadRotation();
        bool GetAudioMode();
        int GetMode();
        void SetMode(int mode);
    }

    namespace Frames {
        ScoreFrame* GetScoreFrame();
        bool AllowComboDrop();
        bool AllowScoreOverride();
    }

    namespace Events {
        extern float wallEnergyLoss;
        void AddNoteController(GlobalNamespace::NoteController* note);
        void RemoveNoteController(GlobalNamespace::NoteController* note);
    }

    void SetLevel(DifficultyBeatmap level);

    void SetReplays(std::vector<std::pair<std::string, ReplayWrapper>> replays, bool external = false);
    void RefreshLevelReplays();
    bool AreReplaysLocal();

    void ReplayStarted(ReplayWrapper& wrapper);
    void ReplayStarted(std::string const& path);
    void ReplayRestarted(bool full = true);
    void ReplayEnded(bool quit);
    void ReplayPaused();
    void ReplayUnpaused();

    extern bool replaying;
    extern bool paused;
    extern ReplayWrapper currentReplay;
    extern DifficultyBeatmap beatmap;

    ReplayInfo const& GetCurrentInfo();

    void UpdateTime(float songTime, float songLength = -1);
    void SetLastCutTime(float lastCutTime);
    void CheckInputs();
    float GetSongTime();
    float GetLength();
    Frame const& GetFrame();
    Frame const& GetNextFrame();
    float GetFrameProgress();
}
