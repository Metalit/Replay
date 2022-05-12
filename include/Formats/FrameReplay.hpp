#pragma once

#include "sombrero/shared/FastVector3.hpp"
#include "sombrero/shared/FastQuaternion.hpp"

using Vector3 = Sombrero::FastVector3;
using Quaternion = Sombrero::FastQuaternion;

struct ReplayModifiers {
    bool disappearingArrows;
    bool noObstacles;
    bool noBombs;
    bool noArrows;
    bool slowerSong;
    bool noFail;
    bool oneLife;
    bool fourLives;
    bool ghostNotes;
    bool fasterSong;
    bool leftHanded;
    bool strictAngles;
    bool proMode;
    bool smallNotes;
    bool superFastSong;
};

struct ReplayInfo {
    ReplayModifiers modifiers;
    float jumpDistance = -1;

    bool practice = false;
    float startTime = 0;
    float speed = 0;
    bool failed = false;
    float failTime = 0;
    bool reached0Energy = false; // failed, but with no fail enabled
    float reached0Time = 0;
};

struct Transform {
    Vector3 position;
    Quaternion rotation;

    constexpr Transform() = default;
    constexpr Transform(const Vector3& position, const Quaternion& rotation) : position(position), rotation(rotation) {}
};

struct ScoreFrame {
    Transform head;
    Transform leftHand;
    Transform rightHand;
    
    float time;
    int score;
    float percent;
    int combo;
    float energy;

    constexpr ScoreFrame() = default;
    constexpr ScoreFrame(float time, int score, float percent, int combo, float energy, const Transform& head, const Transform& leftHand, const Transform& rightHand) : 
        time(time), score(score), percent(percent), combo(combo), energy(energy), head(head), leftHand(leftHand), rightHand(rightHand) {}
};

struct FrameReplay {
    ReplayInfo info;
    std::vector<ScoreFrame> frames;
};

FrameReplay ReadReqlay(const std::string& path);
