#include "parsing.hpp"

#include <regex>

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/BeatmapDifficultySerializedMethods.hpp"
#include "System/Collections/Generic/LinkedListNode_1.hpp"
#include "System/Collections/Generic/LinkedList_1.hpp"
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

static std::string const reqlaySuffix1 = ".reqlay";
static std::string const reqlaySuffix2 = ".questReplayFileForQuestDontTryOnPcAlsoPinkEraAndLillieAreCuteBtwWilliamGay";
static std::string const bsorSuffix = ".bsor";
static std::string const ssSuffix = ".dat";

static void GetReqlays(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, Replay::Replay>>& replays) {
    std::vector<std::string> tests;

    std::string hash = MetaCore::Songs::GetHash(beatmap);
    std::string diff = std::to_string((int) beatmap.difficulty);
    std::string mode = beatmap.beatmapCharacteristic->compoundIdPartName;
    std::string reqlayName = GetReqlaysPath() + hash + diff + mode;
    tests.emplace_back(reqlayName + reqlaySuffix1);
    tests.emplace_back(reqlayName + reqlaySuffix2);
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

static void GetBSORs(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, Replay::Replay>>& replays) {
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
            if (!entry.is_directory() && path.extension() == bsorSuffix && path.stem().string().find(search) != std::string::npos) {
                replays.emplace_back(path.string(), Parsing::ReadBSOR(path.string()));
                logger.info("Read bsor from {}", path.string());
            }
        } catch (std::exception const& e) {
            logger.error("Error reading bsor from {}: {}", path.string(), e.what());
        }
    }
}

static void GetSSReplays(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, Replay::Replay>>& replays) {
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
            if (!entry.is_directory() && path.extension() == ssSuffix && path.stem().string().ends_with(ending)) {
                replays.emplace_back(path.string(), Parsing::ReadScoresaber(path.string()));
                logger.info("Read scoresaber replay from {}", path.string());
            }
        } catch (std::exception const& e) {
            logger.error("Error reading scoresaber replay from {}: {}", path.string(), e.what());
        }
    }
}

std::vector<std::pair<std::string, Replay::Replay>> Parsing::GetReplays(GlobalNamespace::BeatmapKey beatmap) {
    if (!beatmap.IsValid())
        return {};
    logger.debug("search replays {}", beatmap.SerializedName());

    std::vector<std::pair<std::string, Replay::Replay>> replays;

    if (std::filesystem::exists(GetReqlaysPath()))
        GetReqlays(beatmap, replays);

    if (std::filesystem::exists(GetBSORsPath()))
        GetBSORs(beatmap, replays);

    if (std::filesystem::exists(GetSSReplaysPath()))
        GetSSReplays(beatmap, replays);

    return replays;
}

void Parsing::RecalculateNotes(Replay::Replay& replay, GlobalNamespace::IReadonlyBeatmapData* beatmapData) {
    if (!replay.events)
        return;

    auto& events = *replay.events;
    if (!events.needsRecalculation)
        return;
    events.needsRecalculation = false;

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
        int mapNoteId = BSORNoteID(noteData);

        for (auto iter = notes.begin(); iter != notes.end(); iter++) {
            auto& info = (*iter)->info;
            int eventNoteId = BSORNoteID(info);
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
