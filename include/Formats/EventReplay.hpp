#pragma once

#include "FrameReplay.hpp"

struct ReplayNoteCutInfo {
    bool speedOK;
    bool directionOK;
    bool saberTypeOK;
    bool wasCutTooSoon;
    float saberSpeed;
    Vector3 saberDir;
    int saberType;
    float timeDeviation;
    float cutDirDeviation;
    Vector3 cutPoint;
    Vector3 cutNormal;
    float cutDistanceToCenter;
    float cutAngle;
    float beforeCutRating;
    float afterCutRating;
};

struct NoteEventInfo {
    enum struct Type {
        GOOD = 0,
        BAD = 1,
        MISS = 2,
        BOMB = 3
    };

    int noteID;
    float eventTime;
    float spawnTime;
    Type eventType = Type::GOOD;
};

struct NoteEvent {
    NoteEventInfo info;
    ReplayNoteCutInfo noteCutInfo;
};

struct WallEvent {
    int wallID;
    float energy;
    float time;
    float spawnTime;
};

struct HeightEvent {
    float height;
    float time;
};

struct Pause {
    long duration;
    float time;
};

struct Frame {
    float time;
    int fps;
    Transform head;
    Transform leftHand;
    Transform rightHand;

    constexpr Frame() = default;
    constexpr Frame(float time, int fps, const Transform& head, const Transform& leftHand, const Transform& rightHand) : 
        time(time), fps(fps), head(head), leftHand(leftHand), rightHand(rightHand) {}
};

struct EventReplay {
    ReplayInfo info;
    std::vector<Frame> frames;
    std::vector<NoteEvent> notes;
    std::vector<WallEvent> walls;
    std::vector<HeightEvent> heights;
    std::vector<Pause> pauses;
};

EventReplay ReadBSOR(const std::string& path);
