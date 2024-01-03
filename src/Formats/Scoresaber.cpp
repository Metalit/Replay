#include "Main.hpp"
#include "Formats/EventFrame.hpp"
#include "MathUtils.hpp"
#include "lzma/lzma.hpp"

#include <fstream>
#include <sstream>
#include <map>

struct SSPointers {
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

enum SSNoteEventType {
    None,
    GoodCut,
    BadCut,
    Miss,
    Bomb
};

struct SSNoteID {
    float Time;
    int LineLayer;
    int LineIndex;
    int ColorType;
    int CutDirection;
};

// scoresaber serializes field by field instead of whole structs at a time
struct __attribute__ ((packed)) SSNoteEvent {
    SSNoteID NoteID;
    SSNoteEventType EventType;
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

struct SSScoreEvent {
    int Score;
    float Time;
};

struct SSComboEvent {
    int Combo;
    float Time;
};

struct SSEnergyEvent {
    float Energy;
    float Time;
};

// struct SSMultiplierEvent {
//     int Multiplier;
//     float NextMultiplierProgress;
//     float Time;
// };

struct VRPoseGroup {
    Transform Head;
    Transform Left;
    Transform Right;
    int FPS;
    float Time;
};

struct SSMetadata {
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

#define READ_TO(name) input.read(reinterpret_cast<char*>(&name), sizeof(decltype(name)))
#define READ_STRING(name) name = ReadString(input)

std::string ReadString(std::stringstream& input) {
    int length;
    READ_TO(length);
    std::string str;
    str.resize(length);
    input.read(str.data(), length);
    return str;
}

SSMetadata ReadMetadata(std::stringstream& input) {
    SSMetadata ret;
    READ_STRING(ret.Version);
    READ_STRING(ret.LevelID);
    READ_TO(ret.Difficulty);
    READ_STRING(ret.Characteristic);
    READ_STRING(ret.Environment);
    int modifiersLength;
    READ_TO(modifiersLength);
    for(int i = 0; i < modifiersLength; i++)
        ret.Modifiers.emplace_back(ReadString(input));
    READ_TO(ret.NoteSpawnOffset);
    READ_TO(ret.LeftHanded);
    READ_TO(ret.InitialHeight);
    READ_TO(ret.RoomRotation);
    READ_TO(ret.RoomCenter);
    READ_TO(ret.FailTime);
    return ret;
}

ReplayModifiers ParseModifiers(const std::vector<std::string>& modifiers) {
    ReplayModifiers ret = {0};
    for(auto& s : modifiers) {
        if(s == "BE") ret.fourLives = true;
        else if(s == "NF") ret.noFail = true;
        else if(s == "IF") ret.oneLife = true;
        else if(s == "NO") ret.noObstacles = true;
        else if(s == "NB") ret.noBombs = true;
        else if(s == "SA") ret.strictAngles = true;
        else if(s == "DA") ret.disappearingArrows = true;
        else if(s == "GN") ret.ghostNotes = true;
        else if(s == "SS") ret.slowerSong = true;
        else if(s == "FS") ret.fasterSong = true;
        else if(s == "SF") ret.superFastSong = true;
        else if(s == "SC") ret.smallNotes = true;
        else if(s == "PM") ret.proMode = true;
        else if(s == "NA") ret.noArrows = true;
    }
    return ret;
}

bool DecompressReplay(const std::vector<char>& replay, std::vector<char>& decompressed) {
    if(replay[0] == (char)93 && replay[1] == 0 && replay[2] == 0 && replay[3] == (char)128) {
        LOG_ERROR("Scoresaber replay had legacy magic bytes");
        return false;
    }
    std::vector<char> compressedReplayBytes(replay.begin() + 28, replay.end());

    return LZMA::lzmaDecompress(compressedReplayBytes, decompressed);
}

ReplayWrapper ReadScoresaber(const std::string& path) {
    std::ifstream inputCompressed(path, std::ios::binary);

    if(!inputCompressed.is_open()) {
        LOG_ERROR("Failure opening file {}", path);
        return {};
    }
    std::vector<char> compressed(std::istreambuf_iterator<char>{inputCompressed}, std::istreambuf_iterator<char>{});
    std::vector<char> decompressed = {};
    if(!DecompressReplay(compressed, decompressed)) {
        LOG_ERROR("Failure decompressing file {}", path);
        return {};
    }
    std::stringstream input;
    input.write(decompressed.data(), decompressed.size());

    auto replay = new EventFrame();
    ReplayWrapper ret(ReplayType::Event | ReplayType::Frame, replay);
    auto& info = replay->info;

    SSPointers beginnings;
    READ_TO(beginnings);

    input.seekg(beginnings.metadata);
    auto meta = ReadMetadata(input);

    info.modifiers = ParseModifiers(meta.Modifiers);
    info.modifiers.leftHanded = meta.LeftHanded;
    info.failed = meta.FailTime > 0.001;
    info.failTime = meta.FailTime;
    info.modifiers.noFail = info.modifiers.noFail && info.failed;
    info.reached0Energy = info.modifiers.noFail;
    info.jumpDistance = meta.NoteSpawnOffset;

    bool rotation = meta.Characteristic.find("Degree") != std::string::npos;
    QuaternionAverage averageCalc(Quaternion::identity(), rotation);
    input.seekg(beginnings.poseKeyframes);
    int count;
    READ_TO(count);
    VRPoseGroup posFrame;
    for(int i = 0; i < count; i++) {
        READ_TO(posFrame);
        replay->frames.emplace_back(posFrame.Time, posFrame.FPS, posFrame.Head, posFrame.Left, posFrame.Right);
        averageCalc.AddRotation(posFrame.Head.rotation);
    }
    info.averageOffset = UnityEngine::Quaternion::Inverse(averageCalc.GetAverage());

    input.seekg(beginnings.heightKeyframes);
    READ_TO(count);
    for(int i = 0; i < count; i++) {
        auto& height = replay->heights.emplace_back(HeightEvent());
        READ_TO(height);
        replay->events.emplace(height.time, EventRef::Height, replay->heights.size() - 1);
    }

    input.seekg(beginnings.noteKeyframes);
    READ_TO(count);
    SSNoteEvent ssNote;
    for(int i = 0; i < count; i++) {
        auto& note = replay->notes.emplace_back(NoteEvent());
        READ_TO(ssNote);

        note.time = ssNote.Time;
        note.info.scoringType = -2; // not present but v3 replays don't exist anyway
        note.info.lineIndex = ssNote.NoteID.LineIndex;
        note.info.lineLayer = ssNote.NoteID.LineLayer;
        note.info.colorType = ssNote.NoteID.ColorType;
        note.info.cutDirection = ssNote.NoteID.CutDirection;
        note.info.eventType = (NoteEventInfo::Type) (((int) ssNote.EventType) - 1);

        if(note.info.eventType == NoteEventInfo::Type::GOOD || note.info.eventType == NoteEventInfo::Type::BAD) {
            note.noteCutInfo.directionOK = ssNote.DirectionOK;
            note.noteCutInfo.wasCutTooSoon = false; // they do this in their replayer
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
        replay->events.emplace(note.time, EventRef::Note, replay->notes.size() - 1);
    }

    std::map<float, ScoreFrame> framesMap = {};

    input.seekg(beginnings.scoreKeyframes);
    READ_TO(count);
    SSScoreEvent ssScore;
    for(int i = 0; i < count; i++) {
        READ_TO(ssScore);
        auto existing = framesMap.find(ssScore.Time);
        if(existing == framesMap.end())
            framesMap.emplace(ssScore.Time, ScoreFrame{ssScore.Time, ssScore.Score, -1, -1, -1, 0});
        else
            existing->second.score = ssScore.Score;
    }
    info.score = ssScore.Score;
    input.seekg(beginnings.comboKeyframes);
    READ_TO(count);
    SSComboEvent ssCombo;
    for(int i = 0; i < count; i++) {
        READ_TO(ssCombo);
        auto existing = framesMap.find(ssCombo.Time);
        if(existing == framesMap.end())
            framesMap.emplace(ssCombo.Time, ScoreFrame{ssCombo.Time, -1, -1, ssCombo.Combo, -1, 0});
        else
            existing->second.combo = ssCombo.Combo;
    }
    // input.seekg(beginnings.multiplierKeyframes);
    // READ_TO(count);
    // for(int i = 0; i < count; i++) {

    // }
    input.seekg(beginnings.energyKeyframes);
    READ_TO(count);
    SSEnergyEvent ssEnergy;
    for(int i = 0; i < count; i++) {
        READ_TO(ssEnergy);
        auto existing = framesMap.find(ssEnergy.Time);
        if(existing == framesMap.end())
            framesMap.emplace(ssEnergy.Time, ScoreFrame{ssEnergy.Time, -1, -1, -1, ssEnergy.Energy, 0});
        else
            existing->second.energy = ssEnergy.Energy;
    }
    for(auto& [_, frame] : framesMap)
        replay->scoreFrames.emplace_back(std::move(frame));

    auto modified = std::filesystem::last_write_time(path);
    info.timestamp = std::chrono::duration_cast<std::chrono::seconds>(modified.time_since_epoch()).count();
    info.source = "ScoreSaber";
    info.positionsAreLocal = false;
    replay->cutInfoMissingOKs = true;
    // get player name somehow, player id seems to be in file name
    return ret;
}
