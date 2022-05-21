#include "Main.hpp"
#include "Formats/EventReplay.hpp"

#include <fstream>
#include <sstream>

// loading code for beatleader's replay format: https://github.com/BeatLeader/BS-Open-Replay

struct BSORInfo {
    std::string version;
    std::string gameVersion;
    std::string timestamp;

    std::string playerID;
    std::string playerName;
    std::string platform;

    std::string trackingSytem;
    std::string hmd;
    std::string controller;

    std::string hash;
    std::string songName;
    std::string mapper;
    std::string difficulty;

    int score;
    std::string mode;
    std::string environment;
    std::string modifiers;
    float jumpDistance = 0;
    bool leftHanded = false;
    float height = 0;

    float startTime = 0;
    float failTime = 0;
    float speed = 0;
};

#define READ_TO(name) input.read(reinterpret_cast<char*>(&name), sizeof(decltype(name)))
#define READ_STRING(name) name = ReadString(input)

std::string ReadString(std::ifstream& input) {
    int length;
    READ_TO(length);
    std::string str;
    str.resize(length);
    input.read(str.data(), length);
    return str;
}

ReplayModifiers ParseModifierString(const std::string& modifiers) {
    ReplayModifiers ret;
    ret.disappearingArrows = modifiers.find("DA") != std::string::npos;
    ret.fasterSong = modifiers.find("FS") != std::string::npos;
    ret.slowerSong = modifiers.find("SS") != std::string::npos;
    ret.superFastSong = modifiers.find("SF") != std::string::npos;
    ret.strictAngles = modifiers.find("SA") != std::string::npos;
    ret.proMode = modifiers.find("PM") != std::string::npos;
    ret.smallNotes = modifiers.find("SC") != std::string::npos;
    ret.ghostNotes = modifiers.find("GN") != std::string::npos;
    ret.noArrows = modifiers.find("NA") != std::string::npos;
    ret.noBombs = modifiers.find("NB") != std::string::npos;
    ret.noFail = modifiers.find("NF") != std::string::npos;
    ret.noObstacles = modifiers.find("NO") != std::string::npos;
    return ret;
}

BSORInfo ReadInfo(std::ifstream& input) {
    BSORInfo info;
    READ_STRING(info.version);
    READ_STRING(info.gameVersion);
    READ_STRING(info.timestamp);
    
    READ_STRING(info.playerID);
    READ_STRING(info.playerName);
    READ_STRING(info.platform);

    READ_STRING(info.trackingSytem);
    READ_STRING(info.hmd);
    READ_STRING(info.controller);

    READ_STRING(info.hash);
    READ_STRING(info.songName);
    READ_STRING(info.mapper);
    READ_STRING(info.difficulty);

    READ_TO(info.score);
    READ_STRING(info.mode);
    READ_STRING(info.environment);
    READ_STRING(info.modifiers);
    READ_TO(info.jumpDistance);
    READ_TO(info.leftHanded);
    READ_TO(info.height);

    READ_TO(info.startTime);
    READ_TO(info.failTime);
    READ_TO(info.speed);
    return info;
}

ReplayWrapper ReadBSOR(const std::string& path) {
    std::ifstream input(path, std::ios::binary);

    if(!input.is_open()) {
        LOG_ERROR("Failure opening file {}", path);
        return {};
    }

    LOG_INFO("Reading bsor file");

    int header;
    char version;
    char section;
    READ_TO(header);
    READ_TO(version);
    READ_TO(section);
    if (header != 0x442d3d69) {
        LOG_ERROR("Invalid header bytes in bsor file {}", path);
        return {};
    }
    if (version != 1) {
        LOG_ERROR("Invalid version in bsor file {}", path);
        return {};
    }
    if (section != 0) {
        LOG_ERROR("Invalid beginning section in bsor file {}", path);
        return {};
    }

    auto replay = new EventReplay();
    ReplayWrapper ret(ReplayType::Event, replay);
    
    auto info = ReadInfo(input);
    replay->info.modifiers = ParseModifierString(info.modifiers);
    replay->info.modifiers.leftHanded = info.leftHanded;
    replay->info.timestamp = std::stol(info.timestamp);
    replay->info.score = info.score;
    replay->info.source = "Beatleader";
    // infer reached 0 energy because no fail is only listed if it did
    replay->info.reached0Energy = replay->info.modifiers.noFail;
    replay->info.jumpDistance = info.jumpDistance;
    // infer practice because these values are only non 0 (defaults) when it is
    replay->info.practice = info.speed > 0.001 && info.startTime > 0.001;
    replay->info.startTime = info.startTime;
    replay->info.speed = info.speed;
    // infer whether or not the player failed
    replay->info.failed = info.failTime > 0.001;
    replay->info.failTime = info.failTime;

    READ_TO(section);
    if (section != 1) {
        LOG_ERROR("Invalid section 1 header in bsor file {}", path);
        return {};
    }
    int framesCount;
    READ_TO(framesCount);
    for(int i = 0; i < framesCount; i++) {
        auto frame = replay->frames.emplace_back(Frame());
        READ_TO(frame);
    }
    
    READ_TO(section);
    if (section != 2) {
        LOG_ERROR("Invalid section 2 header in bsor file {}", path);
        return {};
    }
    int notesCount;
    READ_TO(notesCount);
    for(int i = 0; i < notesCount; i++) {
        auto note = replay->notes.emplace_back(NoteEvent());
        READ_TO(note.info);
        if(note.info.eventType == NoteEventInfo::Type::GOOD || note.info.eventType == NoteEventInfo::Type::BAD)
            READ_TO(note.noteCutInfo);
    }
    
    READ_TO(section);
    if (section != 3) {
        LOG_ERROR("Invalid section 3 header in bsor file {}", path);
        return {};
    }
    int wallsCount;
    READ_TO(wallsCount);
    for(int i = 0; i < wallsCount; i++) {
        auto wall = replay->walls.emplace_back(WallEvent());
        READ_TO(wall);
    }
    
    READ_TO(section);
    if (section != 4) {
        LOG_ERROR("Invalid section 4 header in bsor file {}", path);
        return {};
    }
    int heightCount;
    READ_TO(heightCount);
    for(int i = 0; i < heightCount; i++) {
        auto height = replay->heights.emplace_back(HeightEvent());
        READ_TO(height);
    }
    replay->info.hasYOffset = true;
    
    READ_TO(section);
    if (section != 5) {
        LOG_ERROR("Invalid section 5 header in bsor file {}", path);
        return {};
    }
    int pauseCount;
    READ_TO(pauseCount);
    for(int i = 0; i < pauseCount; i++) {
        auto pause = replay->pauses.emplace_back(Pause());
        READ_TO(pause);
    }

    return ret;
}
