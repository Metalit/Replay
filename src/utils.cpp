#include "utils.hpp"

#include <regex>

#include "BGLib/Polyglot/Localization.hpp"
#include "CustomTypes/MovementData.hpp"
#include "Formats/EventFrame.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"
#include "GlobalNamespace/BeatmapDataLoader.hpp"
#include "GlobalNamespace/BeatmapDifficultyMethods.hpp"
#include "GlobalNamespace/BeatmapDifficultySerializedMethods.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/FadeInOutController.hpp"
#include "GlobalNamespace/FloatSO.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Resources.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "config.hpp"
#include "main.hpp"
#include "metacore/shared/songs.hpp"

using namespace GlobalNamespace;

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

std::string GetDifficultyName(BeatmapDifficulty difficulty) {
    return BeatmapDifficultyMethods::Name(difficulty);
}

std::string GetDifficultyName(int difficulty) {
    return GetDifficultyName((BeatmapDifficulty) difficulty);
}

BeatmapCharacteristicSO* GetCharacteristic(std::string serializedName) {
    auto chars = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapCharacteristicCollectionSO*>()->First();
    for (auto characteristic : chars->_beatmapCharacteristics) {
        if (characteristic->serializedName == serializedName)
            return characteristic;
    }
    return nullptr;
}

std::string GetCharacteristicName(BeatmapCharacteristicSO* characteristic) {
    auto ret = BGLib::Polyglot::Localization::Get(characteristic->characteristicNameLocalizationKey);
    if (!ret)
        ret = characteristic->characteristicNameLocalizationKey;
    return ret;
}

std::string GetCharacteristicName(std::string serializedName) {
    return GetCharacteristicName(GetCharacteristic(serializedName));
}

std::string GetMapString() {
    std::string songName = MetaCore::Songs::GetSelectedLevel()->songName;
    std::string songAuthor = MetaCore::Songs::GetSelectedLevel()->songAuthorName;
    std::string characteristic = GetCharacteristicName(MetaCore::Songs::GetSelectedKey().beatmapCharacteristic);
    std::string difficulty = GetDifficultyName(MetaCore::Songs::GetSelectedKey().difficulty);
    return fmt::format("{} - {} ({} {})", songAuthor, songName, characteristic, difficulty);
}

static std::string const reqlaySuffix1 = ".reqlay";
static std::string const reqlaySuffix2 = ".questReplayFileForQuestDontTryOnPcAlsoPinkEraAndLillieAreCuteBtwWilliamGay";
static std::string const bsorSuffix = ".bsor";
static std::string const ssSuffix = ".dat";

void GetReqlays(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, ReplayWrapper>>& replays) {
    std::vector<std::string> tests;

    std::string hash = MetaCore::Songs::GetHash(beatmap);
    std::string diff = std::to_string((int) beatmap.difficulty);
    std::string mode = beatmap.beatmapCharacteristic->compoundIdPartName;
    std::string reqlayName = GetReqlaysPath() + hash + diff + mode;
    tests.emplace_back(reqlayName + reqlaySuffix1);
    tests.emplace_back(reqlayName + reqlaySuffix2);
    LOG_DEBUG("searching for reqlays with name {}", reqlayName);
    for (auto& path : tests) {
        if (fileexists(path)) {
            auto replay = ReadReqlay(path);
            if (replay.IsValid()) {
                replays.emplace_back(path, replay);
                LOG_INFO("Read reqlay from {}", path);
            }
        }
    }
}

void GetBSORs(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, ReplayWrapper>>& replays) {
    std::string diffName = BeatmapDifficultySerializedMethods::SerializedName(beatmap.difficulty);
    if (diffName == "Unknown")
        diffName = "Error";
    std::string characteristic = beatmap.beatmapCharacteristic->serializedName;

    std::string bsorHash = regex_replace((std::string) beatmap.levelId, std::basic_regex("custom_level_"), "");
    // sadly, because of beatleader's naming scheme, it's impossible to come up with a reasonably sized set of candidates
    std::string search = fmt::format("{}-{}-{}", diffName, characteristic, bsorHash);
    LOG_DEBUG("searching for bl replays with string {}", search);

    for (auto const& entry : std::filesystem::directory_iterator(GetBSORsPath())) {
        if (!entry.is_directory()) {
            auto path = entry.path();
            if (path.extension() == bsorSuffix && path.stem().string().find(search) != std::string::npos) {
                auto replay = ReadBSOR(path.string());
                if (replay.IsValid()) {
                    replays.emplace_back(path.string(), replay);
                    LOG_INFO("Read bsor from {}", path.string());
                }
            }
        }
    }
}

void GetSSReplays(GlobalNamespace::BeatmapKey beatmap, std::vector<std::pair<std::string, ReplayWrapper>>& replays) {
    std::string diffName = BeatmapDifficultySerializedMethods::SerializedName(beatmap.difficulty);
    std::string characteristic = beatmap.beatmapCharacteristic->serializedName;
    std::string levelHash = beatmap.levelId;
    std::string songName = MetaCore::Songs::FindLevel(beatmap)->songName;

    if (levelHash.find("custom_level_") == std::string::npos)
        levelHash = "ost_" + levelHash;
    else
        levelHash = levelHash.substr(13);

    std::string ending = fmt::format("-{}-{}-{}-{}", songName, diffName, characteristic, levelHash);
    LOG_DEBUG("searching for ss replays with string {}", ending);

    for (auto const& entry : std::filesystem::directory_iterator(GetSSReplaysPath())) {
        if (!entry.is_directory()) {
            auto path = entry.path();
            if (path.extension() == ssSuffix && path.stem().string().ends_with(ending)) {
                auto replay = ReadScoresaber(path.string());
                if (replay.IsValid()) {
                    replays.emplace_back(path.string(), replay);
                    LOG_INFO("Read scoresaber replay from {}", path.string());
                }
            }
        }
    }
}

std::vector<std::pair<std::string, ReplayWrapper>> GetReplays(GlobalNamespace::BeatmapKey beatmap) {
    LOG_DEBUG("search replays {} {}", beatmap.levelId, beatmap.IsValid());
    if (!beatmap.IsValid())
        return {};

    std::vector<std::pair<std::string, ReplayWrapper>> replays;

    if (std::filesystem::exists(GetReqlaysPath()))
        GetReqlays(beatmap, replays);

    if (std::filesystem::exists(GetBSORsPath()))
        GetBSORs(beatmap, replays);

    if (std::filesystem::exists(GetSSReplaysPath()))
        GetSSReplays(beatmap, replays);

    return replays;
}

std::string GetModifierString(ReplayModifiers const& modifiers, bool includeNoFail) {
    std::stringstream s;
    if (modifiers.disappearingArrows)
        s << "DA, ";
    if (modifiers.fasterSong)
        s << "FS, ";
    if (modifiers.slowerSong)
        s << "SS, ";
    if (modifiers.superFastSong)
        s << "SF, ";
    if (modifiers.strictAngles)
        s << "SA, ";
    if (modifiers.proMode)
        s << "PM, ";
    if (modifiers.smallNotes)
        s << "SC, ";
    if (modifiers.ghostNotes)
        s << "GN, ";
    if (modifiers.noArrows)
        s << "NA, ";
    if (modifiers.noBombs)
        s << "NB, ";
    if (modifiers.noFail && includeNoFail)
        s << "NF, ";
    if (modifiers.noObstacles)
        s << "NO, ";
    if (modifiers.leftHanded)
        s << "LH, ";
    auto str = s.str();
    if (str.length() == 0)
        return "None";
    str.erase(str.end() - 2);
    return str;
}

NoteCutInfo GetNoteCutInfo(NoteController* note, Saber* saber, ReplayNoteCutInfo const& info) {
    return NoteCutInfo(
        note ? note->noteData : nullptr,
        info.speedOK,
        info.directionOK,
        info.saberTypeOK,
        info.wasCutTooSoon,
        info.saberSpeed != -1 || saber == nullptr ? info.saberSpeed : saber->bladeSpeed,
        info.saberDir,
        info.saberType,
        info.timeDeviation,
        info.cutDirDeviation,
        info.cutPoint,
        info.cutNormal,
        info.cutAngle,
        info.cutDistanceToCenter,
        note ? note->worldRotation : Quaternion::identity(),
        note ? note->inverseWorldRotation : Quaternion::identity(),
        note ? note->noteTransform->rotation : Quaternion::identity(),
        note ? note->noteTransform->position : Vector3::zero(),
        saber ? MakeFakeMovementData((ISaberMovementData*) saber->_movementData, info.beforeCutRating, info.afterCutRating) : nullptr
    );
}

NoteCutInfo GetBombCutInfo(NoteController* note, Saber* saber) {
    return NoteCutInfo(
        note ? note->noteData : nullptr,
        true,
        true,
        false,
        false,
        saber->bladeSpeed,
        Vector3::zero(),
        saber->saberType,
        0,
        0,
        note->transform->position,
        Vector3::zero(),
        0,
        0,
        note ? note->worldRotation : Quaternion::identity(),
        note ? note->inverseWorldRotation : Quaternion::identity(),
        note ? note->noteTransform->rotation : Quaternion::identity(),
        note ? note->noteTransform->position : Vector3::zero(),
        saber ? (ISaberMovementData*) saber->_movementData : nullptr
    );
}

float ModifierMultiplier(ReplayWrapper const& replay, bool failed) {
    auto mods = replay.replay->info.modifiers;
    float mult = 1;
    if (mods.disappearingArrows)
        mult += 0.07;
    if (mods.noObstacles)
        mult -= 0.05;
    if (mods.noBombs)
        mult -= 0.1;
    if (mods.noArrows)
        mult -= 0.3;
    if (mods.slowerSong)
        mult -= 0.3;
    if (mods.noFail && failed)
        mult -= 0.5;
    if (mods.ghostNotes)
        mult += 0.11;
    if (mods.fasterSong)
        mult += 0.08;
    if (mods.superFastSong)
        mult += 0.1;
    return mult;
}

float EnergyForNote(NoteEventInfo const& noteEvent) {
    if (noteEvent.eventType == NoteEventInfo::Type::BOMB)
        return -0.15;
    bool goodCut = noteEvent.eventType == NoteEventInfo::Type::GOOD;
    bool miss = noteEvent.eventType == NoteEventInfo::Type::MISS;
    switch (noteEvent.scoringType) {
        case -2:
        case (int) NoteData::ScoringType::Normal:
        case (int) NoteData::ScoringType::ChainHead:
        case (int) NoteData::ScoringType::ChainHeadArcTail:
            return goodCut ? 0.01 : (miss ? -0.15 : -0.1);
        case (int) NoteData::ScoringType::ChainLink:
        case (int) NoteData::ScoringType::ChainLinkArcHead:
            return goodCut ? 0.002 : (miss ? -0.03 : -0.025);
        default:
            return 0;
    }
}

int ScoreForNote(NoteEvent const& note, bool max) {
    ScoreModel::NoteScoreDefinition* scoreDefinition;
    if (note.info.scoringType == -2)
        scoreDefinition = ScoreModel::GetNoteScoreDefinition(NoteData::ScoringType::Normal);
    else
        scoreDefinition = ScoreModel::GetNoteScoreDefinition(note.info.scoringType);
    if (max)
        return scoreDefinition->maxCutScore;
    return scoreDefinition->fixedCutScore +
           int(std::lerp(
                   scoreDefinition->minBeforeCutScore, scoreDefinition->maxBeforeCutScore, std::clamp(note.noteCutInfo.beforeCutRating, 0.0f, 1.0f)
               ) +
               0.5) +
           int(std::lerp(
                   scoreDefinition->minAfterCutScore, scoreDefinition->maxAfterCutScore, std::clamp(note.noteCutInfo.afterCutRating, 0.0f, 1.0f)
               ) +
               0.5) +
           int(scoreDefinition->maxCenterDistanceCutScore * (1 - std::clamp(note.noteCutInfo.cutDistanceToCenter / 0.3, 0.0, 1.0)) + 0.5);
}

int BSORNoteID(GlobalNamespace::NoteData* note) {
    int colorType = (int) note->colorType;
    if (colorType < 0)
        colorType = 3;

    return ((int) note->scoringType + 2) * 10000 + note->lineIndex * 1000 + (int) note->noteLineLayer * 100 + colorType * 10 +
           (int) note->cutDirection;
}

int BSORNoteID(NoteEventInfo note) {
    int colorType = note.colorType;
    if (colorType < 0)
        colorType = 3;

    return (note.scoringType + 2) * 10000 + note.lineIndex * 1000 + note.lineLayer * 100 + colorType * 10 + note.cutDirection;
}

void UpdateMultiplier(int& multiplier, int& progress, bool good) {
    if (good) {
        progress++;
        if (multiplier < 8 && progress == (multiplier * 2)) {
            progress = 0;
            multiplier *= 2;
        }
    } else {
        if (multiplier > 1)
            multiplier /= 2;
        progress = 0;
    }
}

void AddEnergy(float& energy, float addition, ReplayModifiers const& modifiers) {
    if (modifiers.oneLife) {
        if (addition < 0)
            energy = 0;
    } else if (modifiers.fourLives) {
        if (addition < 0)
            energy -= 0.25;
    } else if (energy > 0)
        energy += addition;
    if (energy > 1)
        energy = 1;
    if (energy < 0)
        energy = 0;
}

MapPreview MapAtTime(ReplayWrapper const& replay, float time) {
    MapPreview ret{};
    float recentNoteTime = -1;
    if (replay.type & ReplayType::Event) {
        auto eventReplay = dynamic_cast<EventReplay*>(replay.replay.get());
        auto& modifiers = eventReplay->info.modifiers;
        int multiplier = 1, multiProg = 0;
        int maxMultiplier = 1, maxMultiProg = 0;
        float lastCalculatedWall = 0, wallEnd = 0;
        int score = 0;
        int maxScore = 0;
        int combo = 0;
        float energy = 0.5;
        if (modifiers.oneLife || modifiers.fourLives)
            energy = 1;
        for (auto& event : eventReplay->events) {
            if (event.time > time)
                break;
            // add wall energy change since last event
            if (lastCalculatedWall != wallEnd) {
                if (event.time < wallEnd) {
                    AddEnergy(energy, 1.3 * (lastCalculatedWall - event.time), modifiers);
                    lastCalculatedWall = event.time;
                } else {
                    AddEnergy(energy, 1.3 * (lastCalculatedWall - wallEnd), modifiers);
                    lastCalculatedWall = wallEnd;
                }
            }
            switch (event.eventType) {
                case EventRef::Note: {
                    recentNoteTime = event.time;
                    auto& note = eventReplay->notes[event.index];
                    if (note.info.eventType != NoteEventInfo::Type::BOMB) {
                        UpdateMultiplier(maxMultiplier, maxMultiProg, true);
                        maxScore += ScoreForNote(note, true) * maxMultiplier;
                    }
                    if (note.info.eventType == NoteEventInfo::Type::GOOD) {
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
                    if (event.time > wallEnd) {
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
    if (replay.type & ReplayType::Frame) {
        auto frames = dynamic_cast<FrameReplay*>(replay.replay.get())->scoreFrames;
        auto recentValues = ScoreFrame(0, 0, -1, 0, 1, 0);
        for (auto& frame : frames) {
            if (frame.time > time)
                break;
            if (frame.score >= 0)
                recentValues.score = frame.score;
            if (frame.percent >= 0)
                recentValues.percent = frame.percent;
            if (frame.combo >= 0)
                recentValues.combo = frame.combo;
            if (frame.energy >= 0)
                recentValues.energy = frame.energy;
        }
        ret.energy = recentValues.energy;
        ret.combo = recentValues.combo;
        if (time - recentNoteTime > 0.4)
            ret.score = recentValues.score;
        if (recentValues.percent > 0)
            ret.maxScore = (int) (recentValues.score / recentValues.percent);
    }
    return ret;
}

std::vector<OVRInput::Button> const buttons = {
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
std::vector<OVRInput::Controller> const controllers = {OVRInput::Controller::LTouch, OVRInput::Controller::RTouch};

inline bool IsButtonDown(int const& buttonIdx, int controller) {
    if (buttonIdx <= 0)
        return false;
    auto button = buttons[buttonIdx];
    if (controller == 2)
        return OVRInput::Get(button, controllers[0]) || OVRInput::Get(button, controllers[1]);
    return OVRInput::Get(button, controllers[controller]);
}

bool IsButtonDown(Button const& button) {
    return IsButtonDown(button.Button, button.Controller);
}

int IsButtonDown(ButtonPair const& button) {
    int ret = 0;
    if (IsButtonDown(button.ForwardButton, button.ForwardController))
        ret += 1;
    if (IsButtonDown(button.BackButton, button.BackController))
        ret -= 1;
    return ret;
}

void PlayDing() {
    static int const offset = 44;
    static int const frequency = 44100;
    static int const sampleSize = 2;
    using SampleType = int16_t;
    int length = (IncludedAssets::Ding_wav.size() - offset) / sampleSize;
    length -= frequency * 2.5;  // remove some static that shows up at the end for some reason
    auto arr = ArrayW<float>(length);
    for (int i = 0; i < length; i++) {
        arr[i] = *((SampleType*) IncludedAssets::Ding_wav.data() + offset + i * sampleSize);
        arr[i] /= std::numeric_limits<SampleType>::max();
    }
    auto clip = UnityEngine::AudioClip::Create("Ding", length, 1, frequency, false);
    clip->SetData(arr, 0);
    auto audioClipGO = UnityEngine::GameObject::New_ctor("DingAudioClip");
    auto audioSource = audioClipGO->AddComponent<UnityEngine::AudioSource*>();
    audioSource->playOnAwake = false;
    audioSource->clip = clip;
    audioSource->volume = 5;
    audioSource->Play();
}
