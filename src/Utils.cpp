#include "Main.hpp"
#include "Utils.hpp"
#include "Config.hpp"
#include "Assets.hpp"

#include "Formats/EventFrame.hpp"

#include "CustomTypes/MovementData.hpp"

#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapDifficultySerializedMethods.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/ScoreModel_NoteScoreDefinition.hpp"
#include "GlobalNamespace/OVRInput_Button.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "System/Threading/Tasks/Task_1.hpp"

#include "custom-types/shared/coroutine.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include <filesystem>
#include <chrono>
#include <sstream>
#include <regex>

using namespace GlobalNamespace;

std::string SanitizedPath(std::string path) {
    static std::string forbiddenChars("\\/:?\"<>|*");
    static auto replace = [](char c) {
        if(forbiddenChars.find(c) != std::string::npos)
            return '_';
        return c;
    };
    std::transform(path.begin(), path.end(), path.begin(), replace);
    return path;
}

std::string GetReqlaysPath() {
    static auto path = getDataDir("Replay") + "replays/";
    return path;
}

std::string GetBSORsPath() {
    static auto path = getDataDir("bl") + "replays/";
    return path;
}

std::string GetSSReplaysPath() {
    static auto path = getDataDir("ScoreSaber") + "replays/";
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
const std::string ssSuffix = ".dat";

void GetReqlays(IDifficultyBeatmap* beatmap, std::vector<std::pair<std::string, ReplayWrapper>>& replays) {
    std::vector<std::string> tests;

    std::string hash = GetHash((IPreviewBeatmapLevel*) beatmap->get_level());
    std::string diff = std::to_string((int) beatmap->get_difficulty());
    std::string mode = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->compoundIdPartName;
    std::string reqlayName = GetReqlaysPath() + hash + diff + mode;
    tests.emplace_back(reqlayName + reqlaySuffix1);
    tests.emplace_back(reqlayName + reqlaySuffix2);
    for(auto& path : tests) {
        if(fileexists(path)) {
            auto replay = ReadReqlay(path);
            if(replay.IsValid()) {
                replays.emplace_back(path, replay);
                LOG_INFO("Read reqlay from {}", path);
            }
        }
    }
}

void GetBSORs(IDifficultyBeatmap* beatmap, std::vector<std::pair<std::string, ReplayWrapper>>& replays) {
    std::string diffName = BeatmapDifficultySerializedMethods::SerializedName(beatmap->get_difficulty());
    if(diffName == "Unknown")
        diffName = "Error";
    std::string characteristic = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;

    std::string bsorHash = regex_replace((std::string)((IPreviewBeatmapLevel*) beatmap->get_level())->get_levelID(), std::basic_regex("custom_level_"), "");
    // sadly, because of beatleader's naming scheme, it's impossible to come up with a reasonably sized set of candidates
    std::string search = fmt::format("{}-{}-{}", diffName, characteristic, bsorHash);
    for(const auto& entry : std::filesystem::directory_iterator(GetBSORsPath())) {
        if(!entry.is_directory()) {
            auto path = entry.path();
            if(path.extension() == bsorSuffix && path.stem().string().find(search) != std::string::npos) {
                auto replay = ReadBSOR(path.string());
                if(replay.IsValid()) {
                    replays.emplace_back(path.string(), replay);
                    LOG_INFO("Read bsor from {}", path.string());
                }
            }
        }
    }
}

void GetSSReplays(IDifficultyBeatmap* beatmap, std::vector<std::pair<std::string, ReplayWrapper>>& replays) {
    auto previewBeatmap = (IPreviewBeatmapLevel*) beatmap->get_level();

    std::string diffName = BeatmapDifficultySerializedMethods::SerializedName(beatmap->get_difficulty());
    std::string characteristic = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    std::string songName = previewBeatmap->get_songName();
    std::string levelHash = previewBeatmap->get_levelID();

    if(levelHash.find("custom_level_") == std::string::npos)
        levelHash = "ost_" + levelHash;
    else
        levelHash = levelHash.substr(13);

    std::string ending = fmt::format("-{}-{}-{}-{}", songName, diffName, characteristic, levelHash);

    for(const auto& entry : std::filesystem::directory_iterator(GetSSReplaysPath())) {
        if(!entry.is_directory()) {
            auto path = entry.path();
            if(path.extension() == ssSuffix && path.stem().string().ends_with(ending)) {
                auto replay = ReadScoresaber(path.string());
                if(replay.IsValid()) {
                    replays.emplace_back(path.string(), replay);
                    LOG_INFO("Read scoresaber replay from {}", path.string());
                }
            }
        }
    }
}

std::vector<std::pair<std::string, ReplayWrapper>> GetReplays(IDifficultyBeatmap* beatmap) {
    std::vector<std::pair<std::string, ReplayWrapper>> replays;

    if(std::filesystem::exists(GetReqlaysPath()))
        GetReqlays(beatmap, replays);

    if(std::filesystem::exists(GetBSORsPath()))
        GetBSORs(beatmap, replays);

    if(std::filesystem::exists(GetSSReplaysPath()))
        GetSSReplays(beatmap, replays);

    return replays;
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

NoteCutInfo GetNoteCutInfo(NoteController* note, Saber* saber, const ReplayNoteCutInfo& info) {
    return NoteCutInfo(note ? note->noteData : nullptr,
        info.speedOK,
        info.directionOK,
        info.saberTypeOK,
        info.wasCutTooSoon,
        info.saberSpeed != -1 ? info.saberSpeed : saber->get_bladeSpeed(),
        info.saberDir,
        info.saberType,
        info.timeDeviation,
        info.cutDirDeviation,
        info.cutPoint,
        info.cutNormal,
        info.cutAngle,
        info.cutDistanceToCenter,
        note ? note->get_worldRotation() : Quaternion::identity(),
        note ? note->get_inverseWorldRotation() : Quaternion::identity(),
        note ? note->noteTransform->get_rotation() : Quaternion::identity(),
        note ? note->noteTransform->get_position() : Vector3::zero(),
        saber ? MakeFakeMovementData((ISaberMovementData*) saber->movementData, info.beforeCutRating, info.afterCutRating) : nullptr
    );
}

NoteCutInfo GetBombCutInfo(NoteController* note, Saber* saber) {
    return NoteCutInfo(note ? note->noteData : nullptr,
        true,
        true,
        false,
        false,
        saber->get_bladeSpeed(),
        Vector3::zero(),
        saber->get_saberType(),
        0,
        0,
        note->get_transform()->get_position(),
        Vector3::zero(),
        0,
        0,
        note ? note->get_worldRotation() : Quaternion::identity(),
        note ? note->get_inverseWorldRotation() : Quaternion::identity(),
        note ? note->noteTransform->get_rotation() : Quaternion::identity(),
        note ? note->noteTransform->get_position() : Vector3::zero(),
        saber ? (ISaberMovementData*) saber->movementData : nullptr
    );
}

float ModifierMultiplier(const ReplayWrapper& replay, bool failed) {
    auto mods = replay.replay->info.modifiers;
    float mult = 1;
    if(mods.disappearingArrows)
        mult += 0.07;
    if(mods.noObstacles)
        mult -= 0.05;
    if(mods.noBombs)
        mult -= 0.1;
    if(mods.noArrows)
        mult -= 0.3;
    if(mods.slowerSong)
        mult -= 0.3;
    if(mods.noFail && failed)
        mult -= 0.5;
    if(mods.ghostNotes)
        mult += 0.11;
    if(mods.fasterSong)
        mult += 0.08;
    if(mods.superFastSong)
        mult += 0.1;
    return mult;
}

float EnergyForNote(const NoteEventInfo& noteEvent) {
    if(noteEvent.eventType == NoteEventInfo::Type::BOMB)
        return -0.15;
    bool goodCut = noteEvent.eventType == NoteEventInfo::Type::GOOD;
    bool miss = noteEvent.eventType == NoteEventInfo::Type::MISS;
    switch(noteEvent.scoringType) {
    case -2:
    case NoteData::ScoringType::Normal:
    case NoteData::ScoringType::BurstSliderHead:
        return goodCut ? 0.01 : (miss ? -0.15 : -0.1);
    case NoteData::ScoringType::BurstSliderElement:
        return goodCut ? 0.002 : (miss ? -0.03 : -0.025);
    default:
        return 0;
    }
}

int ScoreForNote(const NoteEvent& note, bool max) {
    ScoreModel::NoteScoreDefinition* scoreDefinition;
    if(note.info.scoringType == -2)
        scoreDefinition = ScoreModel::GetNoteScoreDefinition(NoteData::ScoringType::Normal);
    else
        scoreDefinition = ScoreModel::GetNoteScoreDefinition(note.info.scoringType);
    if(max)
        return scoreDefinition->get_maxCutScore();
    return scoreDefinition->fixedCutScore +
        int(std::lerp(scoreDefinition->minBeforeCutScore, scoreDefinition->maxBeforeCutScore, std::clamp(note.noteCutInfo.beforeCutRating, 0.0f, 1.0f)) + 0.5) +
        int(std::lerp(scoreDefinition->minAfterCutScore, scoreDefinition->maxAfterCutScore, std::clamp(note.noteCutInfo.afterCutRating, 0.0f, 1.0f)) + 0.5) +
        int(scoreDefinition->maxCenterDistanceCutScore * (1 - std::clamp(note.noteCutInfo.cutDistanceToCenter / 0.3, 0.0, 1.0)) + 0.5);
}

int BSORNoteID(GlobalNamespace::NoteData* note) {
    int colorType = note->colorType.value;
    if (colorType < 0) colorType = 3;

    return (note->scoringType.value + 2) * 10000 +
            note->lineIndex * 1000 +
            note->noteLineLayer.value * 100 +
            colorType * 10 +
            note->cutDirection.value;
}

int BSORNoteID(NoteEventInfo note) {
    int colorType = note.colorType;
    if (colorType < 0) colorType = 3;

    return (note.scoringType + 2) * 10000 +
            note.lineIndex * 1000 +
            note.lineLayer * 100 +
            colorType * 10 +
            note.cutDirection;
}

void UpdateMultiplier(int& multiplier, int& progress, bool good) {
    if(good) {
        progress++;
        if(multiplier < 8 && progress == (multiplier * 2)) {
            progress = 0;
            multiplier *= 2;
        }
    } else {
        if(multiplier > 1)
            multiplier /= 2;
        progress = 0;
    }
}

void AddEnergy(float& energy, float addition, const ReplayModifiers& modifiers) {
    if(modifiers.oneLife) {
        if(addition < 0)
            energy = 0;
    } else if(modifiers.fourLives) {
        if(addition < 0)
            energy -= 0.25;
    } else if(energy > 0)
        energy += addition;
    if(energy > 1)
        energy = 1;
    if(energy < 0)
        energy = 0;
}

MapPreview MapAtTime(const ReplayWrapper& replay, float time) {
    MapPreview ret{};
    float recentNoteTime = -1;
    if(replay.type & ReplayType::Event) {
        auto eventReplay = dynamic_cast<EventReplay*>(replay.replay.get());
        auto& modifiers = eventReplay->info.modifiers;
        int multiplier = 1, multiProg = 0;
        int maxMultiplier = 1, maxMultiProg = 0;
        float lastCalculatedWall = 0, wallEnd = 0;
        int score = 0;
        int maxScore = 0;
        int combo = 0;
        float energy = 0.5;
        if(modifiers.oneLife || modifiers.fourLives)
            energy = 1;
        for(auto& event: eventReplay->events) {
            if(event.time > time)
                break;
            // add wall energy change since last event
            if(lastCalculatedWall != wallEnd) {
                if(event.time < wallEnd) {
                    AddEnergy(energy, 1.3 * (lastCalculatedWall - event.time), modifiers);
                    lastCalculatedWall = event.time;
                } else {
                    AddEnergy(energy, 1.3 * (lastCalculatedWall - wallEnd), modifiers);
                    lastCalculatedWall = wallEnd;
                }
            }
            switch(event.eventType) {
            case EventRef::Note: {
                recentNoteTime = event.time;
                auto& note = eventReplay->notes[event.index];
                if(note.info.eventType != NoteEventInfo::Type::BOMB) {
                    UpdateMultiplier(maxMultiplier, maxMultiProg, true);
                    maxScore += ScoreForNote(note, true) * maxMultiplier;
                }
                if(note.info.eventType == NoteEventInfo::Type::GOOD) {
                    UpdateMultiplier(multiplier, multiProg, true);
                    score += ScoreForNote(note) * multiplier;
                    combo += 1;
                } else {
                    UpdateMultiplier(multiplier, multiProg, false);
                    combo = 0;
                }
                AddEnergy(energy, EnergyForNote(note.info), modifiers);
                break;
            }
            case EventRef::Wall:
                // combo doesn't get set back to 0 if you enter a wall while inside of one
                if(event.time > wallEnd) {
                    UpdateMultiplier(multiplier, multiProg, false);
                    combo = 0;
                }
                // step through wall energy loss instead of doing it all at once
                lastCalculatedWall = event.time;
                wallEnd = std::max(wallEnd, eventReplay->walls[event.index].endTime);
                break;
            default:
                break;
            }
        }
        ret.energy = energy;
        ret.combo = combo;
        ret.score = score;
        ret.maxScore = maxScore;
    }
    if(replay.type & ReplayType::Frame) {
        auto frames = dynamic_cast<FrameReplay*>(replay.replay.get())->scoreFrames;
        auto recentValues = ScoreFrame(0, 0, -1, 0, 1, 0);
        for(auto& frame : frames) {
            if(frame.time > time)
                break;
            if(frame.score >= 0)
                recentValues.score = frame.score;
            if(frame.percent >= 0)
                recentValues.percent = frame.percent;
            if(frame.combo >= 0)
                recentValues.combo = frame.combo;
            if(frame.energy >= 0)
                recentValues.energy = frame.energy;
        }
        ret.energy = recentValues.energy;
        ret.combo = recentValues.combo;
        if(time - recentNoteTime > 0.4)
            ret.score = recentValues.score;
        if(recentValues.percent > 0)
            ret.maxScore = (int) (recentValues.score / recentValues.percent);
    }
    return ret;
}

const std::vector<OVRInput::Button> buttons = {
    OVRInput::Button::None,
    OVRInput::Button::PrimaryHandTrigger,
    OVRInput::Button::PrimaryIndexTrigger,
    OVRInput::Button::One,
    OVRInput::Button::Two,
    OVRInput::Button::PrimaryThumbstickUp,
    OVRInput::Button::PrimaryThumbstickDown,
    OVRInput::Button::PrimaryThumbstickLeft,
    OVRInput::Button::PrimaryThumbstickRight
};
const std::vector<OVRInput::Controller> controllers = {
    OVRInput::Controller::LTouch,
    OVRInput::Controller::RTouch
};

inline bool IsButtonDown(const int& buttonIdx, int controller) {
    if(buttonIdx <= 0) return false;
    auto button = buttons[buttonIdx];
    if(controller == 2)
        return OVRInput::Get(button, controllers[0]) || OVRInput::Get(button, controllers[1]);
    return OVRInput::Get(button, controllers[controller]);
}

bool IsButtonDown(const Button& button) {
    return IsButtonDown(button.Button, button.Controller);
}

int IsButtonDown(const ButtonPair& button) {
    int ret = 0;
    if(IsButtonDown(button.ForwardButton, button.ForwardController))
        ret += 1;
    if(IsButtonDown(button.BackButton, button.BackController))
        ret -= 1;
    return ret;
}

void PlayDing() {
    static const int offset = 44;
    static const int frequency = 44100;
    static const int sampleSize = 2;
    using sampleSizeType = int16_t;
    int length = (Ding_wav::getLength() - offset) / sampleSize;
    length -= frequency * 2.5; // remove some static that shows up at the end for some reason
    auto arr = ArrayW<float>(length);
    for(int i = 0; i < length; i++) {
        arr[i] = *((sampleSizeType*) Ding_wav::getData() + offset + i * sampleSize);
        arr[i] /= std::numeric_limits<sampleSizeType>::max();
    }
    auto clip = UnityEngine::AudioClip::Create("Ding", length, 1, frequency, false);
    clip->SetData(arr, 0);
    auto audioClipGO = UnityEngine::GameObject::New_ctor("DingAudioClip");
    auto audioSource = audioClipGO->AddComponent<UnityEngine::AudioSource*>();
    audioSource->set_playOnAwake(false);
    audioSource->set_clip(clip);
    audioSource->set_volume(5);
    audioSource->Play();
}
