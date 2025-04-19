#pragma once

#include <map>

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
    enum struct Type { GOOD = 0, BAD = 1, MISS = 2, BOMB = 3 };

    short scoringType;
    short lineIndex;
    short lineLayer;
    short colorType;
    short cutDirection;
    Type eventType = Type::GOOD;
};

struct NoteEvent {
    NoteEventInfo info;
    ReplayNoteCutInfo noteCutInfo;
    float time;
};

struct WallEvent {
    short lineIndex;
    short obstacleType;
    short width;
    float time;
    float endTime;
};

struct HeightEvent {
    float height;
    float time;
};

struct PauseEvent {
    long duration;
    float time;
};

struct EventRef {
    enum Type { Note, Wall, Height, Pause };
    float time;
    Type eventType;
    int index;
    EventRef(float time, Type eventType, int index) : time(time), eventType(eventType), index(index) {}
};

struct EventCompare {
    constexpr bool operator()(EventRef const& lhs, EventRef const& rhs) const {
        if (lhs.time == rhs.time)
            return lhs.eventType < rhs.eventType || (lhs.eventType == rhs.eventType && lhs.index < rhs.index);
        return lhs.time < rhs.time;
    }
};

struct ReplayOffset {
    Vector3 leftSaberPos;
    Quaternion leftSaberRot;
    Vector3 rightSaberPos;
    Quaternion rightSaberRot;
};
static_assert(sizeof(ReplayOffset) == sizeof(float) * (3 + 4 + 3 + 4));

struct EventReplay : public virtual Replay {
    std::vector<NoteEvent> notes;
    std::vector<WallEvent> walls;
    std::vector<HeightEvent> heights;
    std::vector<PauseEvent> pauses;
    std::set<EventRef, EventCompare> events;
    bool needsRecalculation;
    bool cutInfoMissingOKs;
    std::optional<ReplayOffset> offsets;
    std::map<std::string, std::vector<char> > customDatas;
};

ReplayWrapper ReadBSOR(std::string const& path);

namespace GlobalNamespace {
    class IReadonlyBeatmapData;
}
void RecalculateNotes(ReplayWrapper& replay, GlobalNamespace::IReadonlyBeatmapData* beatmapData);
