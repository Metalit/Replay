#include "conditional-dependencies/shared/main.hpp"
#include "math.hpp"
#include "metacore/shared/unity.hpp"
#include "parsing.hpp"
#include "utils.hpp"

// loading code for beatleader's replay format: https://github.com/BeatLeader/BS-Open-Replay

namespace BSOR {
    struct Info {
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

    struct NoteEventInfo {
        int noteID;
        float eventTime;
        float spawnTime;
        Replay::Events::NoteInfo::Type eventType = Replay::Events::NoteInfo::Type::GOOD;
    };

    struct WallEvent {
        int wallID;
        float energy;
        float time;
        float spawnTime;
    };
}

static bool IsLikelyValidCutInfo(Replay::Events::CutInfo& info) {
    if (abs(info.saberType) > 1)
        return false;
    if (info.saberSpeed < -1 || info.saberSpeed >= 1000)
        return false;
    if (info.beforeCutRating < 0 || info.afterCutRating < 0)
        return false;
    return true;
}

std::set<std::string> GetFilenameFlags(std::string filename) {
    int pos = filename.size();
    int i = 0;
    std::set<std::string> flags = {};
    while (true) {
        // split filename on "-" characters
        int start = filename.rfind("-", pos - 1);
        // skip last section from end (player id)
        if (start == std::string::npos)
            break;
        // skip first four sections from end (name, diff, mode, hash)
        // if the name has a dash in it then rip I guess
        if (i > 3)
            flags.emplace(filename.substr(start + 1, pos - start - 1));
        i++;
        pos = start;
    }
    return flags;
}

static Replay::Modifiers ParseModifierString(std::string const& modifiers) {
    Replay::Modifiers ret;
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

static BSOR::Info ReadInfo(std::ifstream& input) {
    BSOR::Info info;
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

static bool ParseMetadata(std::ifstream& input) {
    int header;
    READ_TO(header);
    if (header != 0x442d3d69)
        throw Parsing::Exception("Invalid header bytes");

    int8_t version;
    READ_TO(version);
    if (version > 1)
        throw Parsing::Exception("Unsupported version");

    int8_t section;
    READ_TO(section);
    if (section != 0)
        throw Parsing::Exception("Invalid beginning section");

    return true;
}

static BSOR::Info ParseInfo(std::ifstream& input, Replay::Data& replay, bool practice, bool failed) {
    auto info = ReadInfo(input);
    replay.info.modifiers = ParseModifierString(info.modifiers);
    replay.info.modifiers.leftHanded = info.leftHanded;
    replay.info.timestamp = std::stol(info.timestamp);
    replay.info.score = info.score;
    replay.info.source = "BeatLeader";
    replay.info.positionsAreLocal = true;
    replay.info.playerName.emplace(info.playerName);

    static auto getPlayerId = CondDeps::Find<std::optional<std::string>>("bl", "LoggedInPlayerId");
    static auto getPlayerQuestId = CondDeps::Find<std::optional<std::string>>("bl", "LoggedInPlayerQuestId");

    logger.debug("found beatleader functions {} {}", getPlayerId.has_value(), getPlayerQuestId.has_value());

    logger.debug(
        "player id {}, bl ids {} {}",
        info.playerID,
        getPlayerId ? getPlayerId.value()().value_or("no player") : "no bl",
        getPlayerQuestId ? getPlayerQuestId.value()().value_or("no player") : "no bl"
    );

    replay.info.playerOk = false;
    if (getPlayerId && getPlayerId.value()() == info.playerID)
        replay.info.playerOk = true;
    else if (getPlayerQuestId && getPlayerQuestId.value()() == info.playerID)
        replay.info.playerOk = true;

    logger.debug("player logged in {}", replay.info.playerOk);

    // infer reached 0 energy because no fail is only listed if it did
    replay.info.reached0Energy = replay.info.modifiers.noFail;
    replay.info.jumpDistance = info.jumpDistance;
    replay.info.practice = practice;
    if (practice) {
        replay.info.startTime = info.startTime;
        replay.info.speed = info.speed;
    }
    replay.info.failed = failed;
    if (failed)
        replay.info.failTime = info.failTime;

    return info;
}

static void ParsePoses(std::ifstream& input, Replay::Data& replay, bool hasRotation) {
    int count;
    READ_TO(count);

    MetaCore::Engine::QuaternionAverage averageCalc(Quaternion::identity(), hasRotation);

    // here we have yet another lecagy bug where multiplayer replays record all the avatars
    // we can check for it by checking if multiple frames are recorded at the same time,
    // and fix it by skipping more than one frame each increment, since the duplicates are consistent and ordered
    float firstTime = -1000;
    int duplicates = 0;
    bool finishedDuplicatesCheck = false;

    for (int i = 0; i < count; i++) {
        auto& frame = replay.poses.emplace_back();
        READ_TO(frame);

        if (firstTime == -1000 && frame.time != 0)
            firstTime = frame.time;
        else if (firstTime == frame.time) {
            duplicates++;
            continue;
        }

        if (duplicates > 0) {
            if (!finishedDuplicatesCheck) {
                replay.poses.erase(replay.poses.begin() + 1, replay.poses.end() - 1);
                finishedDuplicatesCheck = true;
            }
            // make sure we don't overseek
            if (i + duplicates >= count)
                duplicates = count - i - 1;
            input.seekg(sizeof(Replay::Pose) * duplicates, std::ios::cur);
            i += duplicates;
        }

        averageCalc.AddRotation(frame.head.rotation);
    }

    replay.info.averageOffset = Quaternion::Inverse(averageCalc.GetAverage());
}

static void ParseNotes(std::ifstream& input, Replay::Data& replay) {
    int count;
    READ_TO(count);

    auto& notes = replay.events->notes;
    auto& events = replay.events->events;

    BSOR::NoteEventInfo noteInfo;

    for (int i = 0; i < count; i++) {
        auto& note = notes.emplace_back();
        READ_TO(noteInfo);

        // Mapping extensions replays require map data for parsing because of the lost data. Blame NSGolova
        if (noteInfo.noteID >= 1000000 || noteInfo.noteID <= -1000)
            replay.events->needsRecalculation = true;

        note.info.scoringType = noteInfo.noteID / 10000;
        noteInfo.noteID -= note.info.scoringType * 10000;
        note.info.scoringType -= 2;

        note.info.lineIndex = noteInfo.noteID / 1000;
        noteInfo.noteID -= note.info.lineIndex * 1000;

        note.info.lineLayer = noteInfo.noteID / 100;
        noteInfo.noteID -= note.info.lineLayer * 100;

        note.info.colorType = noteInfo.noteID / 10;
        noteInfo.noteID -= note.info.colorType * 10;
        if (note.info.colorType == 3)
            note.info.colorType = -1;

        note.info.cutDirection = noteInfo.noteID;

        note.time = noteInfo.eventTime;
        note.info.eventType = noteInfo.eventType;

        if (note.info.HasCut()) {
            READ_TO(note.noteCutInfo);

            // encoding bug in the beatleader qmod made this value be messed with
            // here we set it to -1, which will be replaced with the approximation from the replayed movements
            if (note.noteCutInfo.saberSpeed < 0 || note.noteCutInfo.saberSpeed > 100)
                note.noteCutInfo.saberSpeed = -1;

            // replays on a certain BL version failed to save the NoteCutInfo for chain links correctly
            // so we catch replays with garbage note cut info (to limit failures to maps with the problem) and missing scoring type info
            if (note.info.scoringType == -2 && !IsLikelyValidCutInfo(note.noteCutInfo)) {
                // either fail to load the replay or force set the data (since most fields don't matter for chain links)
                note.noteCutInfo = {0};
                if (note.info.eventType == Replay::Events::NoteInfo::Type::GOOD) {
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
        events.emplace(note.time, Replay::Events::Reference::Note, notes.size() - 1);
    }
}

static void ParseWalls(std::ifstream& input, Replay::Data& replay, BSOR::Info const& info) {
    int count;
    READ_TO(count);

    auto& notes = replay.events->notes;
    auto& walls = replay.events->walls;
    auto& events = replay.events->events;

    BSOR::WallEvent wallEvent;

    // oh boy, I get to calculate the end time of wall events based on energy, it's not like anything better could have been done in the recording phase
    float energy = 0.5;
    // note that beatleader does not record overlapping wall events
    float latestWallTime = -1;
    auto note = notes.begin();

    for (int i = 0; i < count; i++) {
        auto& wall = walls.emplace_back();
        READ_TO(wallEvent);
        wall.lineIndex = wallEvent.wallID / 100;
        wallEvent.wallID -= wall.lineIndex * 100;

        wall.obstacleType = wallEvent.wallID / 10;
        wallEvent.wallID -= wall.obstacleType * 10;

        wall.width = wallEvent.wallID;

        wall.time = wallEvent.time;

        events.emplace(wall.time, Replay::Events::Reference::Wall, walls.size() - 1);

        // we don't care about energy or end time in this case
        // theoretically we should use end time to keep playerHeadIsInObstacle accurate, but since the PC version
        // records end energy instead of end time, it's not possible to know the end time on those replays in this case
        if (replay.info.modifiers.fourLives || replay.info.modifiers.oneLife)
            continue;

        // "oculus" means standalone - PC is either "steam" or "oculuspc"
        if (info.platform == "oculus") {
            wall.endTime = wallEvent.energy;
            // should be in order, but doesn't hurt to check
            if (wall.time > latestWallTime)
                latestWallTime = wall.time;
        } else {
            // process all note events up to event time
            while (note != notes.end() && note->time < wallEvent.time) {
                energy += Utils::EnergyForNote(note->info, replay.events->hasOldScoringTypes);
                if (energy > 1)
                    energy = 1;
                note++;
            }
            float diff = energy - wallEvent.energy;
            // this should never happen
            if (diff < 0)
                diff = 0;
            float seconds = diff / 1.3;
            wall.endTime = wallEvent.time + seconds;
            // now we also correct for any cuts that happen during the wall...
            while (note != notes.end() && note->time < wall.endTime) {
                wall.endTime += Utils::EnergyForNote(note->info, replay.events->hasOldScoringTypes) / 1.3;
                note++;
            }
            energy = wallEvent.energy;
        }
    }

    // replays on yet another BL version just forgot to record wall event end times
    if (latestWallTime > -1) {
        for (auto& wall : walls) {
            if (wall.endTime < wall.time || wall.endTime > latestWallTime * 1000)
                throw Parsing::Exception("Broken wall event");
        }
    }
}

static void ParseHeights(std::ifstream& input, Replay::Data& replay) {
    int count;
    READ_TO(count);

    auto& heights = replay.events->heights;
    auto& events = replay.events->events;

    for (int i = 0; i < count; i++) {
        auto& height = heights.emplace_back();
        READ_TO(height);
        events.emplace(height.time, Replay::Events::Reference::Height, heights.size() - 1);
    }

    replay.info.hasYOffset = true;
}

static void ParsePauses(std::ifstream& input, Replay::Data& replay) {
    int count;
    READ_TO(count);

    auto& pauses = replay.events->pauses;
    auto& events = replay.events->events;

    for (int i = 0; i < count; i++) {
        auto& pause = pauses.emplace_back();
        READ_TO(pause);
        events.emplace(pause.time, Replay::Events::Reference::Pause, pauses.size() - 1);
    }
}

static void ParseOffsets(std::ifstream& input, Replay::Data& replay) {
    READ_TO(replay.offsets.emplace());
}

static void ParseCustomData(std::ifstream& input, Replay::Data& replay) {
    int count;
    READ_TO(count);

    for (int i = 0; i < count; i++) {
        std::string key;
        int length;

        READ_STRING(key);
        READ_TO(length);
        if (length < 0)
            length = 0;

        std::vector<char> content;
        content.resize(length);
        input.read(content.data(), length);

        replay.customData.emplace(key, std::move(content));
    }
}

static void ParseOptionalSections(std::ifstream& input, Replay::Data& replay) {
    while (true) {
        int8_t section;
        try {
            READ_TO(section);
        } catch (...) {
            input.clear();
            return;
        }
        if (section == 6)
            ParseOffsets(input, replay);
        else if (section == 7)
            ParseCustomData(input, replay);
    }
}

std::shared_ptr<Replay::Data> Parsing::ReadBSOR(std::string const& path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);

    if (!input.is_open())
        throw Exception("Failure opening file");

    if (!ParseMetadata(input))
        return {};

    int8_t section;
    auto replay = std::make_shared<Replay::Data>();
    replay->events.emplace();

    auto flags = GetFilenameFlags(std::filesystem::path(path).filename());
    auto info = ParseInfo(input, *replay, flags.contains("practice"), flags.contains("fail"));

    replay->events->hasOldScoringTypes = Utils::LowerVersion(info.gameVersion, "1.40");

    READ_TO(section);
    if (section != 1)
        throw Exception("Invalid section 1 header");
    ParsePoses(input, *replay, info.mode.find("Degree") != std::string::npos);

    READ_TO(section);
    if (section != 2)
        throw Exception("Invalid section 2 header");
    ParseNotes(input, *replay);

    READ_TO(section);
    if (section != 3)
        throw Exception("Invalid section 3 header");
    ParseWalls(input, *replay, info);

    READ_TO(section);
    if (section != 4)
        throw Exception("Invalid section 4 header");
    ParseHeights(input, *replay);

    READ_TO(section);
    if (section != 5)
        throw Exception("Invalid section 5 header");
    ParsePauses(input, *replay);

    ParseOptionalSections(input, *replay);

    // need to do after parsing poses
    replay->info.quit = flags.contains("quit");
    // set so we know that having quit is possible, since older replays won't have the file name
    replay->info.quitTime = replay->poses.back().time;

    replay->info.hash = GetFullHash(input);

    PreProcess(*replay);
    return replay;
}
