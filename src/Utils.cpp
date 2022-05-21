#include "Main.hpp"
#include "Utils.hpp"

#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "System/Threading/Tasks/Task_1.hpp"

#include "custom-types/shared/coroutine.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include <filesystem>
#include <chrono>
#include <sstream>

using namespace GlobalNamespace;

std::string GetReqlaysPath() {
    static auto path = getDataDir("Replay") + "replays/";
    return path;
}

std::string GetBSORsPath() {
    static auto path = getDataDir("bl") + "replays/";
    return path;
}

std::string GetHash(IPreviewBeatmapLevel* level) {
    std::string id = level->get_levelID();
    // should be in all songloader levels
    auto prefixIndex = id.find("custom_level_");
    if(prefixIndex != std::string::npos)
        id.erase(0, 13);
    std::transform(id.begin(), id.end(), id.begin(), tolower);
    return id;
}

const std::string reqlaySuffix1 = ".reqlay";
const std::string reqlaySuffix2 = ".questReplayFileForQuestDontTryOnPcAlsoPinkEraAndLillieAreCuteBtwWilliamGay";
const std::string bsorSuffix = ".bsor";

std::unordered_map<std::string, ReplayWrapper> GetReplays(IDifficultyBeatmap* beatmap) {
    std::unordered_map<std::string, ReplayWrapper> replays;

    std::vector<std::string> tests;

    std::string hash = GetHash((IPreviewBeatmapLevel*) beatmap->get_level());
    std::string diff = std::to_string((int) beatmap->get_difficulty());
    std::string mode = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->compoundIdPartName;
    std::string reqlay1 = GetReqlaysPath() + hash + diff + mode;
    std::string reqlay2 = GetReqlaysPath() + "custom_level_" + hash + diff + mode;
    tests.emplace_back(reqlay1 + reqlaySuffix1);
    tests.emplace_back(reqlay1 + reqlaySuffix2);
    tests.emplace_back(reqlay2 + reqlaySuffix1);
    tests.emplace_back(reqlay2 + reqlaySuffix2);
    for(auto& path : tests) {
        if(fileexists(path))
            replays.insert({path, ReadReqlay(path)});
    }

    std::string bsorDiffName;
    switch ((int) beatmap->get_difficulty()) {
    case 0:
        bsorDiffName = "Easy";
        break;
    case 1:
        bsorDiffName = "Normal";
        break;
    case 2:
        bsorDiffName = "Hard";
        break;
    case 3:
        bsorDiffName = "Expert";
        break;
    case 4:
        bsorDiffName = "ExpertPlus";
        break;
    default:
        bsorDiffName = "Error";
        break;
    }
    std::string characteristic = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    
    std::transform(hash.begin(), hash.end(), hash.begin(), toupper);
    // sadly, because of beatleader's naming scheme, it's impossible to come up with a reasonably sized set of candidates
    std::string search = fmt::format("{}-{}-{}", bsorDiffName, characteristic, hash);
    for(const auto& entry : std::filesystem::directory_iterator(GetBSORsPath())) {
        if(!entry.is_directory()) {
            auto path = entry.path();
            if(path.extension() == bsorSuffix && path.stem().string().find(search) != std::string::npos)
                replays.insert({path.string(), ReadBSOR(path.string())});
        }
    }
    
    return replays;
}

UnityEngine::Sprite* GetReplayIcon() {
    static std::string base64 = R"(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)";
    return QuestUI::BeatSaberUI::Base64ToSprite(base64);
}

std::string GetStringForTimeSinceNow(std::time_t start) {
    auto startTimePoint = std::chrono::system_clock::from_time_t(start);
    auto duration = std::chrono::system_clock::now() - startTimePoint;

    int seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    int minutes = seconds / 60;
    int hours = minutes / 60;
    int days = hours / 24;
    int weeks = days / 7;
    int months = weeks / 4;
    int years = weeks / 52;

    std::string unit;
    int value;

    if(years != 0) {
        unit = "year";
        value = years;
    } else if(months != 0) {
        unit = "month";
        value = months;
    } else if(weeks != 0) {
        unit = "week";
        value = weeks;
    } else if(days != 0) {
        unit = "day";
        value = days;
    } else if(hours != 0) {
        unit = "hour";
        value = hours;
    } else if(minutes != 0) {
        unit = "minute";
        value = minutes;
    } else {
        unit = "second";
        value = seconds;
    }

    if(value != 1) {
        unit = unit + "s";
    }

    return std::to_string(value) + " " + unit + " ago";
}

std::string SecondsToString(int value) {
    int minutes = value / 60;
    int seconds = value - minutes * 60;

    std::string minutesString = std::to_string(minutes);
    std::string secondsString = std::to_string(seconds);
    if(seconds < 10) {
        secondsString = "0" + secondsString;
    }
    
    return minutesString + ":" + secondsString;
}

std::string GetModifierString(const ReplayModifiers& modifiers, bool includeNoFail) {
    std::stringstream s;
    if(modifiers.disappearingArrows) s << "DA, ";
    if(modifiers.fasterSong) s << "FS, ";
    if(modifiers.slowerSong) s << "SS, ";
    if(modifiers.superFastSong) s << "SF, ";
    if(modifiers.strictAngles) s << "SA, ";
    if(modifiers.proMode) s << "PM, ";
    if(modifiers.smallNotes) s << "SC, ";
    if(modifiers.ghostNotes) s << "GN, ";
    if(modifiers.noArrows) s << "NA, ";
    if(modifiers.noBombs) s << "NB, ";
    if(modifiers.noFail && includeNoFail) s << "NF, ";
    if(modifiers.noObstacles) s << "NO, ";
    if(modifiers.leftHanded) s << "LH, ";
    auto str = s.str();
    if(str.length() == 0)
        return "None";
    str.erase(str.end() - 2);
    return str;
}

custom_types::Helpers::Coroutine GetBeatmapDataCoro(IDifficultyBeatmap* beatmap, std::function<void(IReadonlyBeatmapData*)> callback) {
    auto envInfo = ((IPreviewBeatmapLevel*) beatmap->get_level())->get_environmentInfo();

    auto result = beatmap->GetBeatmapDataAsync(envInfo, nullptr);

    while(!result->get_IsCompleted())
        co_yield nullptr;

    callback(result->get_ResultOnSuccess());
    co_return;
}

void GetBeatmapData(IDifficultyBeatmap* beatmap, std::function<void(IReadonlyBeatmapData*)> callback) {
    SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(GetBeatmapDataCoro(beatmap, callback)));
}
