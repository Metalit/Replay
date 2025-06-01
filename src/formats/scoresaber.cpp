#include "lzma/lzma.hpp"
#include "math.hpp"
#include "metacore/shared/unity.hpp"
#include "parsing.hpp"

namespace SS {
    struct Pointers {
        int metadata;
        int poseKeyframes;
        int heightKeyframes;
        int noteKeyframes;
        int scoreKeyframes;
        int comboKeyframes;
        int multiplierKeyframes;
        int energyKeyframes;
        int fpsKeyframes;
    };

    enum NoteEventType { None, GoodCut, BadCut, Miss, Bomb };

    struct NoteID {
        float Time;
        int LineLayer;
        int LineIndex;
        int ColorType;
        int CutDirection;
    };

    // scoresaber serializes field by field instead of whole structs at a time
    struct __attribute__((packed)) NoteEvent {
        NoteEventType EventType;
        Vector3 CutPoint;
        Vector3 CutNormal;
        Vector3 SaberDirection;
        int SaberType;
        bool DirectionOK;
        float SaberSpeed;
        float CutAngle;
        float CutDistanceToCenter;
        float CutDirectionDeviation;
        float BeforeCutRating;
        float AfterCutRating;
        float Time;
        float UnityTimescale;
        float TimeSyncTimescale;
    };

    struct ScoreEvent {
        int Score;
        float Time;
    };

    struct ComboEvent {
        int Combo;
        float Time;
    };

    struct EnergyEvent {
        float Energy;
        float Time;
    };

    // struct MultiplierEvent {
    //     int Multiplier;
    //     float NextMultiplierProgress;
    //     float Time;
    // };

    struct VRPoseGroup {
        Replay::Transform Head;
        Replay::Transform Left;
        Replay::Transform Right;
        int FPS;
        float Time;
    };

    struct Metadata {
        std::string Version;
        std::string LevelID;
        int Difficulty;
        std::string Characteristic;
        std::string Environment;
        std::vector<std::string> Modifiers;
        float NoteSpawnOffset;
        bool LeftHanded;
        float InitialHeight;
        float RoomRotation;
        Vector3 RoomCenter;
        float FailTime;
    };

    namespace V3 {
        struct NoteID : SS::NoteID {
            int GameplayType;
            int ScoringType;
            float CutDirectionAngleOffset;
        };

        struct __attribute__((packed)) NoteEvent : SS::NoteEvent {
            float TimeDeviation;
            Quaternion WorldRotation;
            Quaternion InverseWorldRotation;
            Quaternion NoteRotation;
            Vector3 NotePosition;
        };

        struct ScoreEvent : SS::ScoreEvent {
            int MaxScore;
        };
    }
}

static std::string ReadString(std::stringstream& input) {
    int length;
    READ_TO(length);
    std::string str;
    str.resize(length);
    input.read(str.data(), length);
    return str;
}

static SS::Metadata ReadMetadata(std::stringstream& input) {
    SS::Metadata ret;
    READ_STRING(ret.Version);
    READ_STRING(ret.LevelID);
    READ_TO(ret.Difficulty);
    READ_STRING(ret.Characteristic);
    READ_STRING(ret.Environment);
    int modifiersLength;
    READ_TO(modifiersLength);
    for (int i = 0; i < modifiersLength; i++)
        ret.Modifiers.emplace_back(ReadString(input));
    READ_TO(ret.NoteSpawnOffset);
    READ_TO(ret.LeftHanded);
    READ_TO(ret.InitialHeight);
    READ_TO(ret.RoomRotation);
    READ_TO(ret.RoomCenter);
    READ_TO(ret.FailTime);
    return ret;
}

static Replay::Modifiers ParseModifiers(std::vector<std::string> const& modifiers) {
    Replay::Modifiers ret = {0};
    for (auto& s : modifiers) {
        if (s == "BE")
            ret.fourLives = true;
        else if (s == "NF")
            ret.noFail = true;
        else if (s == "IF")
            ret.oneLife = true;
        else if (s == "NO")
            ret.noObstacles = true;
        else if (s == "NB")
            ret.noBombs = true;
        else if (s == "SA")
            ret.strictAngles = true;
        else if (s == "DA")
            ret.disappearingArrows = true;
        else if (s == "GN")
            ret.ghostNotes = true;
        else if (s == "")
            ret.slowerSong = true;
        else if (s == "FS")
            ret.fasterSong = true;
        else if (s == "SF")
            ret.superFastSong = true;
        else if (s == "SC")
            ret.smallNotes = true;
        else if (s == "PM")
            ret.proMode = true;
        else if (s == "NA")
            ret.noArrows = true;
    }
    return ret;
}

static void DecompressReplay(std::vector<char> const& replay, std::vector<char>& decompressed) {
    if (replay[0] == (char) 93 && replay[1] == 0 && replay[2] == 0 && replay[3] == (char) 128)
        throw Parsing::Exception("Legacy ScoreSaber magic bytes");

    std::vector<char> compressedReplayBytes(replay.begin() + 28, replay.end());

    if (!LZMA::lzmaDecompress(compressedReplayBytes, decompressed))
        throw Parsing::Exception("Error decompressing replay");
}

static SS::Metadata ParseMetadata(std::stringstream& input, Replay::Data& replay) {
    auto& info = replay.info;
    auto meta = ReadMetadata(input);

    info.modifiers = ParseModifiers(meta.Modifiers);
    info.modifiers.leftHanded = meta.LeftHanded;
    info.failed = meta.FailTime > 0.001;
    info.failTime = meta.FailTime;
    info.modifiers.noFail = info.modifiers.noFail && info.failed;
    info.reached0Energy = info.modifiers.noFail;
    info.jumpDistance = meta.NoteSpawnOffset;

    return meta;
}

static void ParsePoses(std::stringstream& input, Replay::Data& replay, bool hasRotations) {
    MetaCore::Engine::QuaternionAverage averageCalc(Quaternion::identity(), hasRotations);

    int count;
    READ_TO(count);

    SS::VRPoseGroup pose;
    for (int i = 0; i < count; i++) {
        READ_TO(pose);
        replay.poses.emplace_back(pose.Time, pose.FPS, pose.Head, pose.Left, pose.Right);
        averageCalc.AddRotation(pose.Head.rotation);
    }

    replay.info.averageOffset = UnityEngine::Quaternion::Inverse(averageCalc.GetAverage());
}

static void ParseHeights(std::stringstream& input, Replay::Data& replay) {
    int count;
    READ_TO(count);

    auto& heights = replay.events->heights;
    auto& events = replay.events->events;

    for (int i = 0; i < count; i++) {
        auto& height = heights.emplace_back();
        READ_TO(height);
        events.emplace(height.time, Replay::Events::Reference::Height, heights.size() - 1);
    }
}

template <int V>
static void ParseNote(std::stringstream& input, Replay::Events::Note& note) {
    std::conditional_t<V == 2, SS::NoteID, SS::V3::NoteID> ssNoteID;
    std::conditional_t<V == 2, SS::NoteEvent, SS::V3::NoteEvent> ssNote;
    READ_TO(ssNoteID);
    READ_TO(ssNote);

    note.time = ssNote.Time;
    note.info.scoringType = -2;
    note.info.lineIndex = ssNoteID.LineIndex;
    note.info.lineLayer = ssNoteID.LineLayer;
    note.info.colorType = ssNoteID.ColorType;
    note.info.cutDirection = ssNoteID.CutDirection;
    note.info.eventType = (Replay::Events::NoteInfo::Type)(((int) ssNote.EventType) - 1);

    if (note.info.HasCut()) {
        note.noteCutInfo.directionOK = ssNote.DirectionOK;
        note.noteCutInfo.wasCutTooSoon = false;  // they do this in their replayer
        note.noteCutInfo.saberSpeed = ssNote.SaberSpeed;
        note.noteCutInfo.saberDir = ssNote.SaberDirection;
        note.noteCutInfo.saberType = ssNote.SaberType;
        note.noteCutInfo.cutDirDeviation = ssNote.CutDirectionDeviation;
        note.noteCutInfo.cutPoint = ssNote.CutPoint;
        note.noteCutInfo.cutNormal = ssNote.CutNormal;
        note.noteCutInfo.cutDistanceToCenter = ssNote.CutDistanceToCenter;
        note.noteCutInfo.cutAngle = ssNote.CutAngle;
        note.noteCutInfo.beforeCutRating = ssNote.BeforeCutRating;
        note.noteCutInfo.afterCutRating = ssNote.AfterCutRating;
    }

    if constexpr (V == 3)
        note.info.scoringType = ssNoteID.ScoringType;
}

template <int V>
static void ParseNotes(std::stringstream& input, Replay::Data& replay) {
    int count;
    READ_TO(count);

    auto& notes = replay.events->notes;
    auto& events = replay.events->events;

    for (int i = 0; i < count; i++) {
        auto& note = notes.emplace_back();
        ParseNote<V>(input, note);
        events.emplace(note.time, Replay::Events::Reference::Note, notes.size() - 1);
    }
}

template <int V>
static int ParseScores(std::stringstream& input, std::map<float, Replay::Frames::Score>& frames) {
    int count;
    READ_TO(count);

    std::conditional_t<V == 2, SS::ScoreEvent, SS::V3::ScoreEvent> ssScore;

    for (int i = 0; i < count; i++) {
        READ_TO(ssScore);

        float percent = -1;
        if constexpr (V == 3)
            percent = ssScore.Score / (float) ssScore.MaxScore;

        auto existing = frames.find(ssScore.Time);
        if (existing != frames.end()) {
            existing->second.score = ssScore.Score;
            existing->second.percent = percent;
        } else
            frames.emplace(ssScore.Time, Replay::Frames::Score{ssScore.Time, ssScore.Score, percent, -1, -1, 0});
    }

    return ssScore.Score;
}

static void ParseCombo(std::stringstream& input, std::map<float, Replay::Frames::Score>& frames) {
    int count;
    READ_TO(count);

    SS::ComboEvent ssCombo;

    for (int i = 0; i < count; i++) {
        READ_TO(ssCombo);

        auto existing = frames.find(ssCombo.Time);
        if (existing == frames.end())
            frames.emplace(ssCombo.Time, Replay::Frames::Score{ssCombo.Time, -1, -1, ssCombo.Combo, -1, 0});
        else
            existing->second.combo = ssCombo.Combo;
    }
}

static void ParseEnergy(std::stringstream& input, std::map<float, Replay::Frames::Score>& frames) {
    int count;
    READ_TO(count);

    SS::EnergyEvent ssEnergy;

    for (int i = 0; i < count; i++) {
        READ_TO(ssEnergy);

        auto existing = frames.find(ssEnergy.Time);
        if (existing == frames.end())
            frames.emplace(ssEnergy.Time, Replay::Frames::Score{ssEnergy.Time, -1, -1, -1, ssEnergy.Energy, 0});
        else
            existing->second.energy = ssEnergy.Energy;
    }
}

Replay::Data Parsing::ReadScoresaber(std::string const& path) {
    std::ifstream inputCompressed(path, std::ios::binary);

    if (!inputCompressed.is_open())
        throw Exception("Failure opening file");

    std::vector<char> compressed(std::istreambuf_iterator<char>{inputCompressed}, std::istreambuf_iterator<char>{});
    std::vector<char> decompressed = {};
    DecompressReplay(compressed, decompressed);

    std::stringstream input;
    input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
    input.write(decompressed.data(), decompressed.size());

    Replay::Data replay;
    auto& info = replay.info;

    replay.events.emplace();
    replay.frames.emplace();

    SS::Pointers pointers;
    READ_TO(pointers);

    input.seekg(pointers.metadata);
    auto meta = ParseMetadata(input, replay);

    bool v3 = false;
    if (meta.Version == "3.0.0")
        v3 = true;
    else if (meta.Version != "2.0.0")
        throw Exception(fmt::format("Unsupported version {}", meta.Version));

    input.seekg(pointers.poseKeyframes);
    ParsePoses(input, replay, meta.Characteristic.find("Degree") != std::string::npos);

    input.seekg(pointers.heightKeyframes);
    ParseHeights(input, replay);

    input.seekg(pointers.noteKeyframes);
    if (v3)
        ParseNotes<3>(input, replay);
    else
        ParseNotes<2>(input, replay);

    // use map to sort and merge frames as possible
    std::map<float, Replay::Frames::Score> frames = {};

    input.seekg(pointers.scoreKeyframes);
    if (v3)
        info.score = ParseScores<3>(input, frames);
    else
        info.score = ParseScores<2>(input, frames);

    input.seekg(pointers.comboKeyframes);
    ParseCombo(input, frames);

    // multipliers unused

    input.seekg(pointers.energyKeyframes);
    ParseEnergy(input, frames);

    for (auto& [_, frame] : frames)
        replay.frames->scores.emplace_back(std::move(frame));

    auto modified = std::filesystem::last_write_time(path);
    info.timestamp = std::chrono::duration_cast<std::chrono::seconds>(modified.time_since_epoch()).count();
    info.source = "ScoreSaber";
    info.positionsAreLocal = false;
    replay.events->cutInfoMissingOKs = true;
    return replay;
}
