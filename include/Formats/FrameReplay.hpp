#pragma once

#include "Replay.hpp"

struct ScoreFrame {
    float time;
    int score;
    float percent;
    int combo;
    float energy;
    float offset;

    constexpr ScoreFrame() = default;
    constexpr ScoreFrame(float time, int score, float percent, int combo, float energy, float offset) : 
        time(time), score(score), percent(percent), combo(combo), energy(energy), offset(offset) {}
};

struct FrameReplay : public Replay {
    std::vector<ScoreFrame> scoreFrames;
};

ReplayWrapper ReadReqlay(const std::string& path);
