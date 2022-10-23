#include "Main.hpp"
#include "Formats/EventReplay.hpp"
#include "MathUtils.hpp"
#include "Utils.hpp"

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

struct BSORNoteEventInfo {
    int noteID;
    float eventTime;
    float spawnTime;
    NoteEventInfo::Type eventType = NoteEventInfo::Type::GOOD;
};

struct BSORWallEvent {
    int wallID;
    float energy;
    float time;
    float spawnTime;
};

bool IsLikelyValidCutInfo(ReplayNoteCutInfo& info) {
    if(abs(info.saberType) > 1)
        return false;
    if(info.saberSpeed < 1 || info.saberSpeed >= 1000)
        return false;
    if(info.beforeCutRating < 0 || info.afterCutRating < 0)
        return false;
    return true;
}

#define READ_TO(name) input.read(reinterpret_cast<char*>(&name), sizeof(decltype(name)))
#define READ_STRING(name) name = ReadString(input)
#define READ_UTF16(name) name = ReadPotentialUTF16(input)

std::string ReadString(std::ifstream& input) {
    int length;
    READ_TO(length);
    std::string str;
    str.resize(length);
    input.read(str.data(), length);
    return str;
}

// Some strings like name, mapper or song name
// may contain incorrectly encoded UTF16 symbols.
std::string ReadPotentialUTF16(std::ifstream& input) {
    int length;
    READ_TO(length);

    if (length > 0) {
        input.seekg(length, input.cur);
        int nextLength;
        READ_TO(nextLength);
        
        // This code will search for the next valid string length
        while (nextLength < 0 || nextLength > 100) {
            input.seekg(-3, input.cur);

            length++;
            READ_TO(nextLength);
        }
        input.seekg(-length - 4, input.cur);
    }

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
    READ_UTF16(info.playerName);
    READ_STRING(info.platform);

    READ_STRING(info.trackingSytem);
    READ_STRING(info.hmd);
    READ_STRING(info.controller);

    READ_STRING(info.hash);
    READ_UTF16(info.songName);
    READ_UTF16(info.mapper);
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
    if (version > 1) {
        LOG_ERROR("Unsupported version in bsor file {}", path);
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
    replay->info.positionsAreLocal = true;
    replay->info.playerName.emplace(info.playerName);
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
    QuaternionAverage averageCalc(UnityEngine::Quaternion::Euler({0, 0, 0}));
    // here we have yet another lecagy bug where multiplayer replays record all the avatars
    float firstTime = -1000;
    int skip = 0;
    bool checkDone = false;
    for(int i = 0; i < framesCount; i++) {
        auto& frame = replay->frames.emplace_back(Frame());
        READ_TO(frame);
        if(firstTime == -1000 && frame.time != 0)
            firstTime = frame.time;
        else if(firstTime == frame.time) {
            skip++;
            continue;
        }
        averageCalc.AddRotation(frame.head.rotation);
        if(skip > 0) {
            if(!checkDone) {
                replay->frames.erase(replay->frames.begin() + 1, replay->frames.end() - 1);
                checkDone = true;
            }
            input.seekg(sizeof(Frame) * skip, std::ios::cur);
            i += skip;
        }
    }
    replay->info.averageOffset = UnityEngine::Quaternion::Inverse(averageCalc.GetAverage());
    if(info.mode.find("Degree") != std::string::npos) {
        auto euler = replay->info.averageOffset.get_eulerAngles();
        euler.y = 0;
        replay->info.averageOffset = UnityEngine::Quaternion::Euler(euler);
    }
    
    READ_TO(section);
    if (section != 2) {
        LOG_ERROR("Invalid section 2 header in bsor file {}", path);
        return {};
    }
    int notesCount;
    READ_TO(notesCount);
    BSORNoteEventInfo noteInfo;
    for(int i = 0; i < notesCount; i++) {
        auto& note = replay->notes.emplace_back(NoteEvent());
        READ_TO(noteInfo);

        // Mapping extensions replays require map data
        // for parsing because of the lost data. Blame NSGolova
        if (noteInfo.noteID >= 1000000 || noteInfo.noteID <= -1000) {
            replay->needsRecalculation = true;
        }

        note.info.scoringType = noteInfo.noteID / 10000;
        noteInfo.noteID -= note.info.scoringType * 10000;
        note.info.scoringType -= 2;

        note.info.lineIndex = noteInfo.noteID / 1000;
        noteInfo.noteID -= note.info.lineIndex * 1000;

        note.info.lineLayer = noteInfo.noteID / 100;
        noteInfo.noteID -= note.info.lineLayer * 100;

        note.info.colorType = noteInfo.noteID / 10;
        noteInfo.noteID -= note.info.colorType * 10;
        if(note.info.colorType == 3) note.info.colorType = -1;

        note.info.cutDirection = noteInfo.noteID;

        note.time = noteInfo.eventTime;
        note.info.eventType = noteInfo.eventType;
        
        if(note.info.eventType == NoteEventInfo::Type::GOOD || note.info.eventType == NoteEventInfo::Type::BAD) {
            READ_TO(note.noteCutInfo);
        
            // replays on a certain BL version failed to save the NoteCutInfo for chain links correctly
            // so we catch replays with garbage note cut info (to limit failures to maps with the problem) and missing scoring type info
            if(note.info.scoringType == -2 && !IsLikelyValidCutInfo(note.noteCutInfo)) {
                // either fail to load the replay or force set the data (since most fields don't matter for chain links)
                if(false) {
                    LOG_ERROR("BSOR had garbage NoteCutInfo data in bsor file {}", path);
                    return {};
                } else {
                    note.noteCutInfo = {0};
                    if(note.info.eventType == NoteEventInfo::Type::GOOD) {
                        note.noteCutInfo.speedOK = true;
                        note.noteCutInfo.directionOK = true;
                        note.noteCutInfo.saberTypeOK = true;
                        note.noteCutInfo.wasCutTooSoon = false;
                    } else {
                        note.noteCutInfo.speedOK = true;
                        note.noteCutInfo.directionOK = true;
                        note.noteCutInfo.saberTypeOK = false;
                        note.noteCutInfo.wasCutTooSoon = false;
                    }
                }
            }
        }
        replay->events.emplace(note.time, EventRef::Note, replay->notes.size() - 1);
    }
    
    READ_TO(section);
    if (section != 3) {
        LOG_ERROR("Invalid section 3 header in bsor file {}", path);
        return {};
    }
    int wallsCount;
    READ_TO(wallsCount);
    BSORWallEvent wallEvent;
    // oh boy, I get to calculate the end time of wall events based on energy, it's not like anything better could have been done in the recording phase
    float energy = 0.5;
    auto notesIter = replay->notes.begin();
    for(int i = 0; i < wallsCount; i++) {
        auto& wall = replay->walls.emplace_back(WallEvent());
        READ_TO(wallEvent);
        wall.lineIndex = wallEvent.wallID / 100;
        wallEvent.wallID -= wall.lineIndex * 100;
        
        wall.obstacleType = wallEvent.wallID / 10;
        wallEvent.wallID -= wall.obstacleType * 10;
        
        wall.width = wallEvent.wallID;

        wall.time = wallEvent.time;
        if(info.platform == "oculus" || version > 1)
            wall.endTime = wallEvent.energy;
            // replays on yet another BL version just forgot to record wall event end times
            if(wall.endTime < wall.time || wall.endTime > replay->notes.back().time * 100) {
                LOG_ERROR("Replay had broken wall event {}", path);
                return {};
            }
        else {
            // process all note events up to event time
            while(notesIter != replay->notes.end() && notesIter->time < wallEvent.time) {
                energy += EnergyForNote(notesIter->info);
                if(energy > 1)
                    energy = 1;
                notesIter++;
            }
            float diff = energy - wallEvent.energy;
            // only realistic case for this happening (assuming the recorder is correct)
            // is for a wall event's time span to be fully contained inside another wall event
            if(diff < 0)
                continue;
            float seconds = diff / 1.3;
            wall.endTime = wallEvent.time + seconds;
            // now we also correct for any misses that happen during the wall...
            while(notesIter != replay->notes.end() && notesIter->time < wall.endTime) {
                wall.endTime -= EnergyForNote(notesIter->info) / 1.3;
                notesIter++;
            }
            energy = wallEvent.energy;
        }
        replay->events.emplace(wall.time, EventRef::Wall, replay->walls.size() - 1);
    }
    
    READ_TO(section);
    if (section != 4) {
        LOG_ERROR("Invalid section 4 header in bsor file {}", path);
        return {};
    }
    int heightCount;
    READ_TO(heightCount);
    for(int i = 0; i < heightCount; i++) {
        auto& height = replay->heights.emplace_back(HeightEvent());
        READ_TO(height);
        replay->events.emplace(height.time, EventRef::Height, replay->heights.size() - 1);
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
        auto& pause = replay->pauses.emplace_back(PauseEvent());
        READ_TO(pause);
        replay->events.emplace(pause.time, EventRef::Height, replay->pauses.size() - 1);
    }

    return ret;
}

#include "GlobalNamespace/BeatmapData.hpp"

#include "System/Collections/Generic/LinkedList_1.hpp"
#include "System/Collections/Generic/LinkedListNode_1.hpp"

#include <list>

using namespace GlobalNamespace;

void RecalculateNotes(ReplayWrapper& replay, IReadonlyBeatmapData* beatmapData) {
    if(replay.type != ReplayType::Event)
        return;
    auto eventReplay = (EventReplay*) replay.replay.get();
    if(!eventReplay->needsRecalculation)
        return;
    
    std::list<NoteEvent*> notes{};
    for(auto& note : eventReplay->notes)
        notes.emplace_back(&note);

    auto list = beatmapData->get_allBeatmapDataItems();
    for(auto i = list->head; i->next != list->head; i = i->next) {
        auto dataOpt = il2cpp_utils::try_cast<NoteData>(i->item);
        if(!dataOpt.has_value())
            continue;
        auto noteData = dataOpt.value();
        int mapNoteId = BSORNoteID(noteData);
        
        for(auto iter = notes.begin(); iter != notes.end(); iter++) {
            auto& info = (*iter)->info;
            int eventNoteId = BSORNoteID(info);
            if(mapNoteId == eventNoteId || mapNoteId == (eventNoteId + 30000)) {
                info.scoringType = noteData->scoringType.value;
                info.lineIndex = noteData->lineIndex;
                info.lineLayer = noteData->noteLineLayer.value;
                info.colorType = noteData->colorType.value;
                info.cutDirection = noteData->cutDirection.value;
                notes.erase(iter);
                break;
            }
        }
    }
    eventReplay->needsRecalculation = false;
}
