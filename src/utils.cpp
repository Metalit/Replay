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
#include "metacore/shared/songs.hpp"

using namespace GlobalNamespace;

std::string Utils::GetDifficultyName(BeatmapDifficulty difficulty) {
    return BeatmapDifficultyMethods::Name(difficulty);
}

std::string Utils::GetDifficultyName(int difficulty) {
    return GetDifficultyName((BeatmapDifficulty) difficulty);
}

BeatmapCharacteristicSO* Utils::GetCharacteristic(std::string serializedName) {
    auto chars = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapCharacteristicCollectionSO*>()->First();
    for (auto characteristic : chars->_beatmapCharacteristics) {
        if (characteristic->serializedName == serializedName)
            return characteristic;
    }
    return nullptr;
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

float Utils::ModifierMultiplier(Replay::Data const& replay, bool failed) {
    auto& mods = replay.info.modifiers;
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

float Utils::EnergyForNote(Replay::Events::NoteInfo const& noteEvent) {
    if (noteEvent.eventType == Replay::Events::NoteInfo::Type::BOMB)
        return -0.15;
    bool goodCut = noteEvent.eventType == Replay::Events::NoteInfo::Type::GOOD;
    bool miss = noteEvent.eventType == Replay::Events::NoteInfo::Type::MISS;
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

int Utils::ScoreForNote(Replay::Events::Note const& note, bool max) {
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
