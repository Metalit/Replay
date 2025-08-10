#include "utils.hpp"

#include "BGLib/Polyglot/Localization.hpp"
#include "CustomTypes/MovementData.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"
#include "GlobalNamespace/BeatmapDataLoader.hpp"
#include "GlobalNamespace/BeatmapDifficultyMethods.hpp"
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
#include "metacore/shared/game.hpp"
#include "metacore/shared/maps.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/strings.hpp"
#include "web-utils/shared/WebUtils.hpp"

using namespace GlobalNamespace;

static MetaCore::CacheMap<std::string, std::optional<std::string>, 50> ssPlayerNames;

bool Utils::LowerVersion(std::string version, std::string compare) {
    while (true) {
        auto versionPos = version.find(".");
        auto comparePos = compare.find(".");
        try {
            int versionInt = std::stoi(version.substr(0, versionPos));
            int compareInt = std::stoi(compare.substr(0, comparePos));
            if (versionInt < compareInt)
                return true;
            else if (versionInt > compareInt)
                return false;
        } catch (std::exception const& e) {
            return false;
        }
        if (versionPos == std::string::npos || comparePos == std::string::npos)
            return versionPos == std::string::npos && comparePos != std::string::npos;
        version = version.substr(versionPos + 1);
        compare = compare.substr(comparePos + 1);
    }
}

void Utils::GetSSPlayerName(std::string id, std::function<void(std::optional<std::string>)> callback) {
    if (ssPlayerNames.contains(id)) {
        auto name = ssPlayerNames[id];
        logger.debug("using cached player name {} for {}", name ? "\"" + *name + "\"" : "nullopt", id);
        callback(name);
        return;
    }
    // this function hopefully won't be called quickly enough to make a ton of requests for the same id
    logger.info("Requesting scoresaber name for {}", id);
    std::string url = fmt::format("https://scoresaber.com/api/player/{}/basic", id);
    WebUtils::GetAsync<WebUtils::JsonResponse>({url}, [id, callback](WebUtils::JsonResponse response) {
        std::optional<std::string> name;
        if (response.DataParsedSuccessful()) {
            try {
                name = response.GetParsedData()["name"].GetString();
                logger.debug("got player name \"{}\" for {}", *name, id);
            } catch (std::exception const& e) {
                logger.error("Failed to get name from response for scoresaber player id {}", id);
            }
        } else
            logger.error("Web request for scoresaber player id {} failed: http {} curl {}", id, response.httpCode, response.curlStatus);
        BSML::MainThreadScheduler::Schedule([id, name, callback]() {
            ssPlayerNames.push(id, name);
            callback(name);
        });
    });
}

std::string Utils::GetDifficultyName(BeatmapDifficulty difficulty) {
    return BeatmapDifficultyMethods::Name(difficulty);
}

std::string Utils::GetDifficultyName(int difficulty) {
    return GetDifficultyName((BeatmapDifficulty) difficulty);
}

BeatmapCharacteristicSO* Utils::GetCharacteristic(std::string serializedName) {
    return MetaCore::Game::GetCharacteristics()->GetBeatmapCharacteristicBySerializedName(serializedName);
}

std::string Utils::GetCharacteristicName(BeatmapCharacteristicSO* characteristic) {
    auto ret = BGLib::Polyglot::Localization::Get(characteristic->characteristicNameLocalizationKey);
    if (!ret)
        ret = characteristic->characteristicNameLocalizationKey;
    return ret;
}

std::string Utils::GetCharacteristicName(std::string serializedName) {
    return GetCharacteristicName(GetCharacteristic(serializedName));
}

std::string Utils::GetMapString() {
    std::string songName = MetaCore::Songs::GetSelectedLevel()->songName;
    std::string songAuthor = MetaCore::Songs::GetSelectedLevel()->songAuthorName;
    std::string characteristic = GetCharacteristicName(MetaCore::Songs::GetSelectedKey().beatmapCharacteristic);
    std::string difficulty = GetDifficultyName(MetaCore::Songs::GetSelectedKey().difficulty);
    return fmt::format("{} - {} ({} {})", songAuthor, songName, characteristic, difficulty);
}

std::string Utils::GetStatusString(Replay::Info const& info, bool color, float songLength) {
    std::string hex = "#2adb44";
    float time = -1;
    std::string status = songLength >= 0 ? "Passed" : "Pass";
    if (info.failed) {
        hex = "#cc1818";
        time = info.failTime;
        status = songLength >= 0 ? "Failed at" : "Fail";
    } else if (info.quit) {
        hex = "#cc7818";
        time = info.quitTime;
        status = songLength >= 0 ? "Quit at" : "Quit";
    } else if (info.practice) {
        hex = "#66ebff";
        time = info.startTime;
        status = songLength >= 0 ? "Practice from" : "Practice";
    }
    bool useTime = songLength >= 0 && time >= 0;
    std::string time1 = useTime ? " " + MetaCore::Strings::SecondsToString(time) : "";
    std::string time2 = useTime ? " / " + MetaCore::Strings::SecondsToString(songLength) : "";
    if (!color)
        return fmt::format("{}{}{}", status, time1, time2);
    return fmt::format("<color={}>{}{}</color>{}", hex, status, time1, time2);
}

std::string Utils::GetModifierString(Replay::Modifiers const& modifiers, bool includeNoFail) {
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

NoteCutInfo Utils::GetNoteCutInfo(NoteController* note, Saber* saber, Replay::Events::CutInfo const& info) {
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
        saber ? Replay::MovementData::Create((ISaberMovementData*) saber->_movementData, info.beforeCutRating, info.afterCutRating) : nullptr
    );
}

NoteCutInfo Utils::GetBombCutInfo(NoteController* note, Saber* saber) {
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

enum OldScoringType { Ignore = -1, NoScore, Normal, ArcHead, ArcTail, ChainHead, ChainLink };

bool Utils::ScoringTypeMatches(int replayType, GlobalNamespace::NoteData::ScoringType noteType, bool oldScoringType) {
    if (replayType == -2)
        return true;
    if (!oldScoringType)
        return replayType == (int) noteType;
    switch (replayType) {
        case OldScoringType::ArcHead:
            return noteType == NoteData::ScoringType::ArcHead || noteType == NoteData::ScoringType::ArcHeadArcTail ||
                   noteType == NoteData::ScoringType::ChainLinkArcHead;
        case OldScoringType::ArcTail:
            return noteType == NoteData::ScoringType::ArcTail || noteType == NoteData::ScoringType::ArcHeadArcTail ||
                   noteType == NoteData::ScoringType::ChainHeadArcTail;
        case OldScoringType::ChainHead:
            return noteType == NoteData::ScoringType::ChainHead || noteType == NoteData::ScoringType::ChainHeadArcTail;
        case OldScoringType::ChainLink:
            return noteType == NoteData::ScoringType::ChainLink || noteType == NoteData::ScoringType::ChainLinkArcHead;
        default:
            return replayType == (int) noteType;
    }
}

float Utils::EnergyForNote(Replay::Events::NoteInfo const& note, bool oldScoringType) {
    if (note.eventType == Replay::Events::NoteInfo::Type::BOMB)
        return -0.15;
    bool goodCut = note.eventType == Replay::Events::NoteInfo::Type::GOOD;
    bool miss = note.eventType == Replay::Events::NoteInfo::Type::MISS;
    if (oldScoringType) {
        switch (note.scoringType) {
            case -2:
            case OldScoringType::Normal:
            case OldScoringType::ChainHead:
                return goodCut ? 0.01 : (miss ? -0.15 : -0.1);
            case OldScoringType::ChainLink:
                return goodCut ? 0.002 : (miss ? -0.03 : -0.025);
            default:
                return 0;
        }
    } else {
        switch ((NoteData::ScoringType) note.scoringType) {
            case NoteData::ScoringType::Normal:
            case NoteData::ScoringType::ChainHead:
            case NoteData::ScoringType::ChainHeadArcTail:
            // thankfully, I don't think chain links can't get marked as any of these arc types
            case NoteData::ScoringType::ArcHead:
            case NoteData::ScoringType::ArcTail:
            case NoteData::ScoringType::ArcHeadArcTail:
                return goodCut ? 0.01 : (miss ? -0.15 : -0.1);
            case NoteData::ScoringType::ChainLink:
            case NoteData::ScoringType::ChainLinkArcHead:
                return goodCut ? 0.002 : (miss ? -0.03 : -0.025);
            default:
                return 0;
        }
    }
}

float Utils::AccuracyForDistance(float distance) {
    return 1 - std::clamp(distance / (float) 0.3, (float) 0, (float) 1);
}

static ScoreModel::NoteScoreDefinition* GetScoreDefinition(int scoringType, bool oldScoringType) {
    if (!oldScoringType)
        return ScoreModel::GetNoteScoreDefinition(scoringType);
    switch (scoringType) {
        case -2:
            return ScoreModel::GetNoteScoreDefinition(NoteData::ScoringType::Normal);
        case OldScoringType::ChainHead:
            return ScoreModel::GetNoteScoreDefinition(NoteData::ScoringType::ChainHead);
        case OldScoringType::ChainLink:
            return ScoreModel::GetNoteScoreDefinition(NoteData::ScoringType::ChainLink);
        default:
            return ScoreModel::GetNoteScoreDefinition(scoringType);
    }
}

std::array<int, 4> Utils::ScoreForNote(Replay::Events::Note const& note, bool oldScoringType, bool max) {
    bool goodCut = note.info.eventType == Replay::Events::NoteInfo::Type::GOOD;
    if (!goodCut && !max)
        return {0};

    auto scoreDefinition = GetScoreDefinition(note.info.scoringType, oldScoringType);

    if (max && scoreDefinition)
        return {
            scoreDefinition->maxBeforeCutScore,
            scoreDefinition->maxAfterCutScore,
            scoreDefinition->maxCenterDistanceCutScore,
            scoreDefinition->maxCutScore
        };

    if (!scoreDefinition || !goodCut)
        return {0};

    float before = std::clamp(note.noteCutInfo.beforeCutRating, (float) 0, (float) 1);
    float after = std::clamp(note.noteCutInfo.afterCutRating, (float) 0, (float) 1);
    float acc = AccuracyForDistance(note.noteCutInfo.cutDistanceToCenter);

    int multBefore = std::round(std::lerp(scoreDefinition->minBeforeCutScore, scoreDefinition->maxBeforeCutScore, before));
    int multAfter = std::round(std::lerp(scoreDefinition->minAfterCutScore, scoreDefinition->maxAfterCutScore, after));
    int multAcc = std::round(scoreDefinition->maxCenterDistanceCutScore * acc);

    return {multBefore, multAfter, multAcc, scoreDefinition->fixedCutScore + multBefore + multAfter + multAcc};
}

int Utils::BSORNoteID(GlobalNamespace::NoteData* note) {
    int colorType = (int) note->colorType;
    if (colorType < 0)
        colorType = 3;

    return ((int) note->scoringType + 2) * 10000 + note->lineIndex * 1000 + (int) note->noteLineLayer * 100 + colorType * 10 +
           (int) note->cutDirection;
}

int Utils::BSORNoteID(Replay::Events::NoteInfo const& note) {
    int colorType = note.colorType;
    if (colorType < 0)
        colorType = 3;

    return (note.scoringType + 2) * 10000 + note.lineIndex * 1000 + note.lineLayer * 100 + colorType * 10 + note.cutDirection;
}

static std::vector<OVRInput::Button> const Buttons = {
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
static std::vector<OVRInput::Controller> const Controllers = {OVRInput::Controller::LTouch, OVRInput::Controller::RTouch};

static bool RawButtonDown(int const& buttonIdx, int controller) {
    if (buttonIdx <= 0)
        return false;
    auto button = Buttons[buttonIdx];
    if (controller == 2)
        return OVRInput::Get(button, Controllers[0]) || OVRInput::Get(button, Controllers[1]);
    return OVRInput::Get(button, Controllers[controller]);
}

bool Utils::IsButtonDown(Button const& button) {
    return RawButtonDown(button.Button, button.Controller);
}

int Utils::IsButtonDown(ButtonPair const& button) {
    int ret = 0;
    if (RawButtonDown(button.ForwardButton, button.ForwardController))
        ret += 1;
    if (RawButtonDown(button.BackButton, button.BackController))
        ret -= 1;
    return ret;
}

void Utils::PlayDing() {
    static constexpr int Offset = 44;
    static constexpr int Frequency = 44100;
    static constexpr int SampleSize = 2;

    int length = (IncludedAssets::Ding_wav.size() - Offset) / SampleSize;
    length -= Frequency * 2.5;  // remove some static that shows up at the end for some reason
    auto arr = ArrayW<float>(length);
    for (int i = 0; i < length; i++) {
        arr[i] = *((short*) IncludedAssets::Ding_wav.data() + Offset + i * SampleSize);
        arr[i] /= std::numeric_limits<short>::max();
    }
    auto clip = UnityEngine::AudioClip::Create("Ding", length, 1, Frequency, false);
    clip->SetData(arr, 0);
    auto audioClipGO = UnityEngine::GameObject::New_ctor("DingAudioClip");
    auto audioSource = audioClipGO->AddComponent<UnityEngine::AudioSource*>();
    audioSource->playOnAwake = false;
    audioSource->clip = clip;
    audioSource->volume = 5;
    audioSource->Play();
}
