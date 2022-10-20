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

enum struct ReplayType {
    Frame,
    Event
};

struct ReplayInfo {
    ReplayModifiers modifiers;
    std::time_t timestamp;
    int score;
    std::string source;
    bool positionsAreLocal;
    float jumpDistance = -1;
    bool hasYOffset = false;
    std::optional<std::string> playerName;

    Quaternion averageOffset; // inverse of the average difference from looking forward

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
    constexpr Transform(const Vector3& pos, const Quaternion& rot) : position(pos), rotation(rot) {}
};

struct Frame {
    float time;
    int fps;
    Transform head;
    Transform leftHand;
    Transform rightHand;

    constexpr Frame() = default;
    constexpr Frame(const Transform& head, const Transform& leftHand, const Transform& rightHand) :
        head(head), leftHand(leftHand), rightHand(rightHand) {}
};

struct Replay {
    ReplayInfo info;
    std::vector<Frame> frames;
    virtual ~Replay() = default;
};

struct ReplayWrapper {
    ReplayType type;
    std::shared_ptr<Replay> replay;

    ReplayWrapper() = default;
    ReplayWrapper(ReplayType type, Replay* replay) : type(type), replay(replay) {}

    bool IsValid() const { return (bool) replay; }
};
