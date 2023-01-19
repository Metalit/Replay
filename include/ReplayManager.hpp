#pragma once

#include "Utils.hpp"

#include "GlobalNamespace/IDifficultyBeatmap.hpp"
#include "GlobalNamespace/NoteController.hpp"
#include "GlobalNamespace/ObstacleController.hpp"
// #include "GlobalNamespace/BloomPrePassGraphicsSettingsPresetsSO_Preset.hpp"
// #include "GlobalNamespace/BloomPrePassEffectContainerSO.hpp"
#include "GlobalNamespace/MirrorRendererGraphicsSettingsPresets_Preset.hpp"
#include "GlobalNamespace/MirrorRendererSO.hpp"

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
        extern bool moving;

        // extern GlobalNamespace::BloomPrePassGraphicsSettingsPresetsSO* bloomPresets;
        // extern GlobalNamespace::BloomPrePassEffectContainerSO* bloomContainer;
        extern GlobalNamespace::MirrorRendererGraphicsSettingsPresets* mirrorPresets;
        extern GlobalNamespace::MirrorRendererSO* mirrorRenderer;

        Vector3 GetHeadPosition();
        Quaternion GetHeadRotation();
        bool GetAudioMode();
        int GetMode();
        void SetMode(int mode);
    }

    namespace Frames {
        ScoreFrame* GetScoreFrame();
        bool AllowComboDrop();
    }

    namespace Events {
        extern float wallEnergyLoss;
        void AddNoteController(GlobalNamespace::NoteController* note);
        void RemoveNoteController(GlobalNamespace::NoteController* note);
    }

    void SetLevel(GlobalNamespace::IDifficultyBeatmap* level);

    void SetReplays(std::vector<std::pair<std::string, ReplayWrapper>> replays, bool external = false);
    void RefreshLevelReplays();
    bool AreReplaysLocal();

    void ReplayStarted(ReplayWrapper& wrapper);
    void ReplayStarted(const std::string& path);
    void ReplayRestarted(bool full = true);
    void ReplayEnded(bool quit);
    void ReplayPaused();
    void ReplayUnpaused();

    extern bool replaying;
    extern bool paused;
    extern ReplayWrapper currentReplay;
    extern GlobalNamespace::IDifficultyBeatmap* beatmap;

    const ReplayInfo& GetCurrentInfo();

    void UpdateTime(float songTime);
    void CheckInputs();
    float GetSongTime();
    const Frame& GetFrame();
    const Frame& GetNextFrame();
    float GetFrameProgress();
}
