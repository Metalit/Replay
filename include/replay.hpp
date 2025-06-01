#pragma once

#include "main.hpp"

namespace Replay {
    namespace Frames {
        struct Score {
            float time;
            int score;
            float percent;
            int combo;
            float energy;
            float offset;

            constexpr Score() = default;
            constexpr Score(float time, int score, float percent, int combo, float energy, float offset) :
                time(time),
                score(score),
                percent(percent),
                combo(combo),
                energy(energy),
                offset(offset) {}
        };

        struct Data {
            std::vector<Score> scores;
        };
    }

    namespace Events {
        struct CutInfo {
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

        struct NoteInfo {
            enum struct Type { GOOD = 0, BAD = 1, MISS = 2, BOMB = 3 };

            short scoringType;
            short lineIndex;
            short lineLayer;
            short colorType;
            short cutDirection;
            Type eventType = Type::GOOD;

            bool HasCut() const { return eventType == Type::GOOD || eventType == Type::BAD; }
        };

        struct Note {
            NoteInfo info;
            CutInfo noteCutInfo;
            float time;
        };

        struct Wall {
            short lineIndex;
            short obstacleType;
            short width;
            float time;
            float endTime;
        };

        struct Height {
            float height;
            float time;
        };

        struct Pause {
            long duration;
            float time;
        };

        struct Reference {
            enum Type { Note, Wall, Height, Pause };
            float time;
            Type eventType;
            int index;

            constexpr Reference(float time, Type eventType, int index) : time(time), eventType(eventType), index(index) {}

            struct Comparer {
                constexpr bool operator()(Reference const& lhs, Reference const& rhs) const {
                    if (lhs.time == rhs.time)
                        return lhs.eventType < rhs.eventType || (lhs.eventType == rhs.eventType && lhs.index < rhs.index);
                    return lhs.time < rhs.time;
                }
            };
        };

        struct Data {
            std::vector<Note> notes;
            std::vector<Wall> walls;
            std::vector<Height> heights;
            std::vector<Pause> pauses;
            std::set<Reference, Reference::Comparer> events;
            bool needsRecalculation;
            bool cutInfoMissingOKs;
        };
    }

    struct Modifiers {
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

    struct Info {
        Modifiers modifiers;
        long timestamp;
        int score;
        std::string source;
        bool positionsAreLocal;
        float jumpDistance = -1;
        bool hasYOffset = false;
        std::optional<std::string> playerName;
        bool playerOk = false;

        Quaternion averageOffset;  // inverse of the average difference from looking forward

        bool practice = false;
        float startTime = 0;
        float speed = 0;
        bool failed = false;
        float failTime = 0;
        bool reached0Energy = false;  // failed, but with no fail enabled
        float reached0Time = 0;
    };

    struct Transform {
        Vector3 position;
        Quaternion rotation;

        constexpr Transform() = default;
        constexpr Transform(Vector3 const& pos, Quaternion const& rot) : position(pos), rotation(rot) {}
    };

    struct Pose {
        float time;
        int fps;
        Transform head;
        Transform leftHand;
        Transform rightHand;

        constexpr Pose() = default;
        constexpr Pose(Transform const& head, Transform const& leftHand, Transform const& rightHand) :
            head(head),
            leftHand(leftHand),
            rightHand(rightHand) {}
        constexpr Pose(float time, int fps, Transform const& head, Transform const& leftHand, Transform const& rightHand) :
            time(time),
            fps(fps),
            head(head),
            leftHand(leftHand),
            rightHand(rightHand) {}
    };

    struct Offsets {
        Transform leftSaber;
        Transform rightSaber;
    };

    struct Data {
        Info info;
        std::vector<Pose> poses;
        std::optional<Frames::Data> frames;
        std::optional<Events::Data> events;
        std::optional<Offsets> offsets;
        std::map<std::string, std::vector<char>> customData;
    };
}
