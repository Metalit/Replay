#include "parsing.hpp"

#include <regex>

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/BeatmapDifficultySerializedMethods.hpp"
#include "System/Collections/Generic/LinkedListNode_1.hpp"
#include "System/Collections/Generic/LinkedList_1.hpp"
#include "config.hpp"
#include "metacore/shared/songs.hpp"
#include "utils.hpp"

template <class T>
static std::string ReadStringGeneric(T& input) {
    int length;
    READ_TO(length);
    std::string str;
    str.resize(length);
    input.read(str.data(), length);
    return str;
}

std::string Parsing::ReadString(std::stringstream& input) {
    return ReadStringGeneric(input);
}

std::string Parsing::ReadString(std::ifstream& input) {
    return ReadStringGeneric(input);
}

// Some strings like name, mapper or song name may contain incorrectly encoded UTF16 symbols
// Contributed by NSGolova
std::string Parsing::ReadStringUTF16(std::ifstream& input) {
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

static std::string const ReqlaySuffix1 = ".reqlay";
static std::string const ReqlaySuffix2 = ".questReplayFileForQuestDontTryOnPcAlsoPinkEraAndLillieAreCuteBtwWilliamGay";
static std::string const BSORSuffix = ".bsor";
static std::string const SSSuffix = ".dat";

static std::string GetReqlaysPath() {
    static auto path = getDataDir("Replay") + "replays/";
    return path;
}

static std::string GetBSORsPath() {
    static auto path = getDataDir("bl") + "replays/";
    return path;
}

static std::string GetSSReplaysPath() {
    static auto path = getDataDir("ScoreSaber") + "replays/";
    return path;
}

static void GetReqlays(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, Replay::Data>>& replays) {
    std::vector<std::string> tests;

    std::string hash = MetaCore::Songs::GetHash(beatmap);
    std::string diff = std::to_string((int) beatmap.difficulty);
    std::string mode = beatmap.beatmapCharacteristic->compoundIdPartName;
    std::string reqlayName = GetReqlaysPath() + hash + diff + mode;
    tests.emplace_back(reqlayName + ReqlaySuffix1);
    tests.emplace_back(reqlayName + ReqlaySuffix2);
    logger.debug("searching for reqlays with name {}", reqlayName);
    for (auto& path : tests) {
        try {
            if (fileexists(path)) {
                replays.emplace_back(path, Parsing::ReadReqlay(path));
                logger.info("Read reqlay from {}", path);
            }
        } catch (std::exception const& e) {
            logger.error("Error reading reqlay from {}: {}", path, e.what());
        }
    }
}

static void GetBSORs(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, Replay::Data>>& replays) {
    std::string diffName = GlobalNamespace::BeatmapDifficultySerializedMethods::SerializedName(beatmap.difficulty);
    if (diffName == "Unknown")
        diffName = "Error";
    std::string characteristic = beatmap.beatmapCharacteristic->serializedName;

    std::string hash = beatmap.levelId;
    if (hash.starts_with("custom_level_"))
        hash = hash.substr(13);

    // sadly, because of beatleader's naming scheme, it's impossible to come up with a reasonably sized set of candidates
    std::string search = fmt::format("{}-{}-{}", diffName, characteristic, hash);
    logger.debug("searching for bl replays with string {}", search);

    for (auto const& entry : std::filesystem::directory_iterator(GetBSORsPath())) {
        auto path = entry.path();
        try {
            if (!entry.is_directory() && path.extension() == BSORSuffix && path.stem().string().find(search) != std::string::npos) {
                replays.emplace_back(path.string(), Parsing::ReadBSOR(path.string()));
                logger.info("Read bsor from {}", path.string());
            }
        } catch (std::exception const& e) {
            logger.error("Error reading bsor from {}: {}", path.string(), e.what());
        }
    }
}

static void GetSSReplays(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, Replay::Data>>& replays) {
    std::string diffName = GlobalNamespace::BeatmapDifficultySerializedMethods::SerializedName(beatmap.difficulty);
    std::string characteristic = beatmap.beatmapCharacteristic->serializedName;
    std::string levelHash = beatmap.levelId;
    std::string songName = MetaCore::Songs::FindLevel(beatmap)->songName;

    if (levelHash.find("custom_level_") == std::string::npos)
        levelHash = "ost_" + levelHash;
    else
        levelHash = levelHash.substr(13);

    std::string ending = fmt::format("-{}-{}-{}-{}", songName, diffName, characteristic, levelHash);
    logger.debug("searching for ss replays with string {}", ending);

    for (auto const& entry : std::filesystem::directory_iterator(GetSSReplaysPath())) {
        auto path = entry.path();
        try {
            if (!entry.is_directory() && path.extension() == SSSuffix && path.stem().string().ends_with(ending)) {
                replays.emplace_back(path.string(), Parsing::ReadScoresaber(path.string()));
                logger.info("Read scoresaber replay from {}", path.string());
            }
        } catch (std::exception const& e) {
            logger.error("Error reading scoresaber replay from {}: {}", path.string(), e.what());
        }
    }
}

std::vector<std::pair<std::string, Replay::Data>> Parsing::GetReplays(GlobalNamespace::BeatmapKey beatmap) {
    if (!beatmap.IsValid())
        return {};
    logger.debug("search replays {}", beatmap.SerializedName());

    std::vector<std::pair<std::string, Replay::Data>> replays;

    if (std::filesystem::exists(GetReqlaysPath()))
        GetReqlays(beatmap, replays);

    if (std::filesystem::exists(GetBSORsPath()))
        GetBSORs(beatmap, replays);

    if (std::filesystem::exists(GetSSReplaysPath()))
        GetSSReplays(beatmap, replays);

    return replays;
}

static int combo;
static int leftCombo;
static int rightCombo;
static int maxCombo;
static int maxLeftCombo;
static int maxRightCombo;
static int multiplier;
static int multiplierProgress;

static void ResetTrackers() {
    combo = 0;
    leftCombo = 0;
    rightCombo = 0;
    maxCombo = 0;
    maxLeftCombo = 0;
    maxRightCombo = 0;
    multiplier = 1;
    multiplierProgress = 0;
}

static void GoodEvent(bool left, bool right) {
    combo++;
    if (left)
        leftCombo++;
    if (right)
        rightCombo++;
    maxCombo = std::max(combo, maxCombo);
    maxLeftCombo = std::max(leftCombo, maxLeftCombo);
    maxRightCombo = std::max(rightCombo, maxRightCombo);
    if (multiplier == 8 || ++multiplierProgress < multiplier * 2)
        return;
    multiplier *= 2;
    multiplierProgress = 0;
}

static void BadEvent(bool left, bool right) {
    combo = 0;
    if (left)
        leftCombo = 0;
    if (right)
        rightCombo = 0;
    multiplierProgress = 0;
    if (multiplier != 1)
        multiplier /= 2;
}

void Parsing::PreProcess(Replay::Data& replay) {
    if (replay.frames) {
        auto& frames = *replay.frames;
        ResetTrackers();

        Replay::Frames::Score previous;
        for (auto& score : frames.scores) {
            if (score.score < 0)
                score.score = previous.score;
            if (score.percent < 0)
                score.percent = previous.percent;
            if (score.combo < 0)
                score.combo = previous.combo;
            if (score.energy < 0)
                score.energy = previous.energy;
            if (score.offset < 0)
                score.offset = previous.offset;
            if (score.multiplier < 0)
                score.multiplier = previous.multiplier;
            if (score.multiplierProgress < 0)
                score.multiplierProgress = previous.multiplierProgress;
            score.maxCombo = std::max(score.combo, score.maxCombo);
            previous = score;

            // do our best guess of multipliers if missing, no hope for per hand combo though
            if (score.multiplier < 0) {
                if (score.combo < previous.combo)
                    BadEvent(false, false);
                else if (score.combo > previous.combo)
                    GoodEvent(false, false);
                score.multiplier = multiplier;
                score.multiplierProgress = multiplierProgress;
            }
        }
    }

    if (replay.events) {
        auto& events = *replay.events;
        ResetTrackers();

        int lives = 0;
        if (replay.info.modifiers.oneLife)
            lives = 1;
        else if (replay.info.modifiers.fourLives)
            lives = 4;

        float wallSegmentStart = 0;
        float wallSegmentEnd = 0;
        float energy = lives > 0 ? 1 : 0.5;

        for (auto event = events.events.begin(); event != events.events.end(); event++) {
            bool note = event->eventType == Replay::Events::Reference::Note;
            bool wall = event->eventType == Replay::Events::Reference::Wall;

            auto noteInfo = note ? &events.notes[event->index].info : nullptr;

            bool mistake = wall || note && noteInfo->eventType != Replay::Events::NoteInfo::Type::GOOD;
            bool left = note && noteInfo->colorType == 0;
            bool right = note && noteInfo->colorType == 1;

            if (wall || note)
                mistake ? BadEvent(left, right) : GoodEvent(left, right);

            if (lives == 0) {
                // split up the energy loss from walls if there are events in between
                if (wallSegmentEnd > 0) {
                    if (event->time > wallSegmentEnd) {
                        energy -= (wallSegmentEnd - wallSegmentStart) * 1.3;
                        wallSegmentEnd = 0;
                    } else {
                        energy -= (event->time - wallSegmentStart) * 1.3;
                        wallSegmentStart = event->time;
                    }
                }

                if (wall) {
                    wallSegmentStart = event->time;
                    wallSegmentEnd = events.walls[event->index].endTime;
                } else if (note && energy > 0)
                    energy += Utils::EnergyForNote(*noteInfo);
            } else if (mistake)
                energy -= 1 / (float) lives;

            if (energy > 1)
                energy = 1;
            else if (energy < 0)
                energy = 0;

            // casts are ok because these aren't used when sorting the set
            const_cast<int&>(event->combo) = combo;
            const_cast<int&>(event->leftCombo) = leftCombo;
            const_cast<int&>(event->rightCombo) = rightCombo;
            const_cast<int&>(event->maxCombo) = maxCombo;
            const_cast<int&>(event->maxLeftCombo) = maxLeftCombo;
            const_cast<int&>(event->maxRightCombo) = maxRightCombo;
            const_cast<float&>(event->energy) = energy;
            const_cast<int&>(event->multiplier) = multiplier;
            const_cast<int&>(event->multiplierProgress) = multiplierProgress;
        }
    }
}

void Parsing::RecalculateNotes(Replay::Data& replay, GlobalNamespace::IReadonlyBeatmapData* beatmapData) {
    if (!replay.events)
        return;

    auto& events = *replay.events;
    if (!events.needsRecalculation)
        return;
    events.needsRecalculation = false;

    logger.debug("recalculating replay notes with beatmap data");

    std::list<Replay::Events::Note*> notes;
    for (auto& note : events.notes)
        notes.emplace_back(&note);

    // for each note in the beatmap data, try to find the first note in the replay with a matching id,
    // then set its info and remove it from the pool (since multiple notes may have the same id)
    auto list = beatmapData->allBeatmapDataItems;
    for (auto i = list->head; i->next != list->head; i = i->next) {
        auto cast = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(i->item);
        if (!cast)
            continue;
        auto noteData = *cast;
        int mapNoteId = Utils::BSORNoteID(noteData);

        for (auto iter = notes.begin(); iter != notes.end(); iter++) {
            auto& info = (*iter)->info;
            int eventNoteId = Utils::BSORNoteID(info);
            if (mapNoteId == eventNoteId || mapNoteId == (eventNoteId + 30000)) {
                info.scoringType = (int) noteData->scoringType;
                info.lineIndex = noteData->lineIndex;
                info.lineLayer = (int) noteData->noteLineLayer;
                info.colorType = (int) noteData->colorType;
                info.cutDirection = (int) noteData->cutDirection;
                notes.erase(iter);
                break;
            }
        }
    }
}
