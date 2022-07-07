#include "Main.hpp"
#include "ReplayManager.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "ReplayMenu.hpp"

#include "bs-utils/shared/utils.hpp"

using namespace GlobalNamespace;

IDifficultyBeatmap* beatmap = nullptr;
std::unordered_map<std::string, ReplayWrapper> currentReplays;

int currentFrame = 0;
int frameCount = 0;
float songTime = 0;
float lerpAmount = 0;

namespace Manager {
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
    }

    void ReplayRestarted() {
        paused = false;
        currentFrame = 0;
        songTime = 0;
    }

    void ReplayEnded() {
        bs_utils::Submission::enable(modInfo);
        replaying = false;
    }

    bool replaying = false;
    bool paused = false;
    ReplayWrapper currentReplay;
    
    void UpdateTime(float time) {
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

    namespace Frames {
        ScoreFrame* GetScoreFrame() {
            return &((FrameReplay*) currentReplay.replay.get())->scoreFrames[currentFrame];
        }
    }
    
    namespace Events {

    }
}
