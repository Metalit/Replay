#pragma once

#include "Replay.hpp"

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

    short lineIndex;
    short lineLayer;
    short colorType;
    short cutDirection;
    float eventTime;
    Type eventType = Type::GOOD;
};

struct NoteEvent {
    NoteEventInfo info;
    ReplayNoteCutInfo noteCutInfo;
};

struct WallEvent {
    short lineIndex;
    short obstacleType;
    short width;
    float energy;
    float time;
};

struct HeightEvent {
    float height;
    float time;
};

struct Pause {
    long duration;
    float time;
};

struct EventReplay : public Replay {
    std::vector<NoteEvent> notes;
    std::vector<WallEvent> walls;
    std::vector<HeightEvent> heights;
    std::vector<Pause> pauses;
};

ReplayWrapper ReadBSOR(const std::string& path);
