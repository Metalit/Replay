#include "playback.hpp"

#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/SettingsManager.hpp"
#include "GlobalNamespace/VRCenterAdjust.hpp"
#include "System/Action.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "Unity/Mathematics/float3.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "manager.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/stats.hpp"
#include "utils.hpp"

using namespace GlobalNamespace;

struct NoteCompare {
    constexpr bool operator()(NoteController const* const& lhs, NoteController const* const& rhs) const {
        if (lhs->_noteData->_time_k__BackingField == rhs->_noteData->_time_k__BackingField)
            return lhs < rhs;
        return lhs->_noteData->_time_k__BackingField < rhs->_noteData->_time_k__BackingField;
    }
};

namespace Frames {
    static constexpr int frameSearchRadius = 2;
    static Replay::Frames::Data const* frames;
    static decltype(frames->scores)::const_iterator score;
    static float lastCutTime;

    static void UpdateTime(float time) {
        if (!frames)
            return;
        while (score != frames->scores.end() && score->time < time)
            score++;
    }

    static void SeekTo(float currentTime, float time) {
        if (!frames)
            return;
        if (time < currentTime) {
            while (score != frames->scores.begin() && score->time > time)
                score--;
        } else
            UpdateTime(time);
    }

    static bool AllowComboDrop() {
        if (!frames)
            return true;
        int back = std::min(frameSearchRadius, (int) std::distance(score, frames->scores.begin()));
        auto iter = score - back;
        int combo = iter->combo;
        for (int i = 0; i < back + frameSearchRadius; i++) {
            iter++;
            if (iter == frames->scores.end())
                break;
            if (iter->combo < 0)
                i--;
            else if (iter->combo < combo)
                return true;
            else
                combo = iter->combo;
        }
        return false;
    }

    static bool AllowScoreOverride() {
        if (!frames)
            return false;
        if (score->percent >= 0)
            return true;
        // fix scoresaber replays having incorrect max score before cut finishes
        return MetaCore::Stats::GetSongTime() - lastCutTime > 0.4;
    }
}

namespace Events {
    static std::set<NoteController*, NoteCompare> notes;
    static Replay::Events::Data const* events;
    static decltype(events->events)::const_iterator event;
    static float wallEndTime;
    static float wallEnergyLoss;
    static PlayerHeadAndObstacleInteraction* obstacles;

    static bool Matches(NoteData* data, Replay::Events::NoteInfo const& info) {
        return ((int) data->scoringType == info.scoringType || info.scoringType == -2) && (int) data->lineIndex == info.lineIndex &&
               (int) data->noteLineLayer == info.lineLayer && (int) data->colorType == info.colorType &&
               (int) data->cutDirection == info.cutDirection;
    }

    static void RunNoteEvent(Replay::Events::Note const& event, NoteController* controller) {
        static auto sendCut = il2cpp_utils::FindMethodUnsafe(classof(NoteController*), "SendNoteWasCutEvent", 1);

        bool isLeftSaber = event.noteCutInfo.saberType == (int) SaberType::SaberA;
        auto sabers = MetaCore::Internals::saberManager();
        Saber* saber = isLeftSaber ? sabers->_leftSaber : sabers->_rightSaber;

        switch (event.info.eventType) {
            case Replay::Events::NoteInfo::Type::GOOD:
            case Replay::Events::NoteInfo::Type::BAD: {
                auto cutInfo = Utils::GetNoteCutInfo(controller, saber, event.noteCutInfo);
                if (events->cutInfoMissingOKs) {
                    cutInfo.speedOK = cutInfo.saberSpeed > 2;
                    bool isLeftColor = controller->noteData->colorType == ColorType::ColorA;
                    cutInfo.saberTypeOK = isLeftColor == isLeftSaber;
                    cutInfo.timeDeviation = controller->noteData->time - event.time;
                }
                Frames::lastCutTime = event.time;
                il2cpp_utils::RunMethod<void, false>(controller, sendCut, byref(cutInfo));
            }
            case Replay::Events::NoteInfo::Type::BOMB: {
                auto cutInfo = Utils::GetBombCutInfo(controller, saber);
                il2cpp_utils::RunMethod<void, false>(controller, sendCut, byref(cutInfo));
                break;
            }
            case Replay::Events::NoteInfo::Type::MISS:
                controller->SendNoteWasMissedEvent();
                break;
        }
    }

    static void ProcessNoteEvent(Replay::Events::Note const& event) {
        auto& info = event.info;

        for (auto iter = notes.begin(); iter != notes.end(); iter++) {
            auto controller = *iter;
            auto data = controller->noteData;
            if (!Matches(data, info))
                continue;
            RunNoteEvent(event, controller);
            if (info.eventType == Replay::Events::NoteInfo::Type::MISS)
                notes.erase(iter);  // note will despawn and be removed in the other cases
            return;
        }

        int bsorID = (event.info.scoringType + 2) * 10000 + event.info.lineIndex * 1000 + event.info.lineLayer * 100 + event.info.colorType * 10 +
                     event.info.cutDirection;
        logger.error("Could not find note for event! time: {}, bsor id: {}", event.time, bsorID);
    }

    static void ProcessWallEvent(Replay::Events::Wall const& event) {
        obstacles->headDidEnterObstacleEvent->Invoke(nullptr);
        obstacles->headDidEnterObstaclesEvent->Invoke();
        float diffStartTime = std::max(wallEndTime, event.time);
        wallEndTime = std::max(wallEndTime, event.endTime);
        wallEnergyLoss += (wallEndTime - diffStartTime) * 1.3;
    }

    static void UpdateTime(float time) {
        if (!events)
            return;
        while (event != events->events.end() && event->time < time) {
            switch (event->eventType) {
                case Replay::Events::Reference::Note:
                    ProcessNoteEvent(events->notes[event->index]);
                    break;
                case Replay::Events::Reference::Wall:
                    ProcessWallEvent(events->walls[event->index]);
                    break;
                default:
                    break;
            }
            event++;
        }
    }

    static void SeekTo(float currentTime, float time) {
        if (!events)
            return;
        if (time > currentTime) {
            while (event != events->events.end() && event->time < time)
                event++;
        } else {
            while (event != events->events.begin() && event->time > time)
                event--;
        }
    }
}

static Replay::Pose interpolatedPose;
static int index = 0;

static Replay::Transform Lerp(Replay::Transform const& start, Replay::Transform const& end, float t) {
    return {Vector3::Lerp(start.position, end.position, t), Quaternion::Lerp(start.rotation, end.rotation, t)};
}

static void UpdateInterpolatedPose(std::vector<Replay::Pose> const& poses, float time) {
    if (index < poses.size() - 1) {
        auto const& current = poses[index];
        auto const& next = poses[index + 1];

        float progress = time - current.time;
        float poseDuration = next.time - current.time;
        float lerpAmount = progress / poseDuration;

        interpolatedPose.time = time;
        interpolatedPose.fps = current.fps;
        interpolatedPose.head = Lerp(current.head, next.head, lerpAmount);
        interpolatedPose.leftHand = Lerp(current.leftHand, next.leftHand, lerpAmount);
        interpolatedPose.rightHand = Lerp(current.rightHand, next.rightHand, lerpAmount);
    } else
        interpolatedPose = poses[index];
}

void Playback::UpdateTime() {
    if (!Manager::Replaying())
        return;

    float time = MetaCore::Stats::GetSongTime();
    Frames::UpdateTime(time);
    Events::UpdateTime(time);

    auto& poses = Manager::GetCurrentReplay().poses;

    while (index < poses.size() - 1 && poses[index].time < time)
        index++;
    UpdateInterpolatedPose(poses, time);
}

void Playback::SeekTo(float time) {
    if (!Manager::Replaying())
        return;

    auto& poses = Manager::GetCurrentReplay().poses;
    float currentTime = interpolatedPose.time;

    Frames::SeekTo(currentTime, time);
    Events::SeekTo(currentTime, time);

    if (time > currentTime) {
        while (index < poses.size() - 1 && poses[index].time < time)
            index++;
    } else {
        while (index > 0 && poses[index].time > time)
            index--;
    }
    UpdateInterpolatedPose(poses, time);
}

Replay::Pose const& Playback::GetPose() {
    return interpolatedPose;
}

bool Playback::DisableRealEvent(bool bad) {
    if (!Manager::Replaying())
        return false;
    if (Events::events)
        return true;
    if (Frames::frames && bad && !Frames::AllowComboDrop())
        return true;
    return false;
}

void Playback::AddNoteController(NoteController* note) {
    if (!Manager::Rendering())
        return;

    auto data = note->noteData;
    if (data->scoringType > NoteData::ScoringType::NoScore || data->gameplayType == NoteData::GameplayType::Bomb)
        Events::notes.insert(note);
}

void Playback::RemoveNoteController(NoteController* note) {
    if (Manager::Replaying())
        Events::notes.erase(note);
}

static GameplayModifiers* CreateModifiers() {
    auto& info = Manager::GetCurrentInfo();
    auto const& modifiers = info.modifiers;
    auto energyType = modifiers.fourLives ? GameplayModifiers::EnergyType::Battery : GameplayModifiers::EnergyType::Bar;
    bool noFail = modifiers.noFail;
    bool instaFail = modifiers.oneLife;
    bool saberClash = false;
    auto obstacleType = modifiers.noObstacles ? GameplayModifiers::EnabledObstacleType::NoObstacles : GameplayModifiers::EnabledObstacleType::All;
    bool noBombs = modifiers.noBombs;
    bool fastNotes = false;
    bool strictAngles = modifiers.strictAngles;
    bool disappearingArrows = modifiers.disappearingArrows;
    auto songSpeed =
        modifiers.superFastSong
            ? GameplayModifiers::SongSpeed::SuperFast
            : (modifiers.fasterSong ? GameplayModifiers::SongSpeed::Faster
                                    : (modifiers.slowerSong ? GameplayModifiers::SongSpeed::Slower : GameplayModifiers::SongSpeed::Normal));
    bool noArrows = modifiers.noArrows;
    bool ghostNotes = modifiers.ghostNotes;
    bool proMode = modifiers.proMode;
    bool zenMode = false;
    bool smallNotes = modifiers.smallNotes;
    return GameplayModifiers::New_ctor(
        energyType,
        noFail,
        instaFail,
        saberClash,
        obstacleType,
        noBombs,
        fastNotes,
        strictAngles,
        disappearingArrows,
        songSpeed,
        noArrows,
        ghostNotes,
        proMode,
        zenMode,
        smallNotes
    );
}

static PracticeSettings* CreatePracticeSettings() {
    auto& info = Manager::GetCurrentInfo();
    return info.practice ? PracticeSettings::New_ctor(info.startTime, info.speed) : nullptr;
}

void Playback::ProcessStart(GameplayModifiers*& modifiers, PracticeSettings*& practice, PlayerSpecificSettings* player) {
    Events::notes.clear();
    Events::wallEndTime = 0;
    Events::wallEnergyLoss = 0;

    Frames::lastCutTime = -9999;

    if (!Manager::Replaying())
        return;
    auto& replay = Manager::GetCurrentReplay();

    Events::events = replay.events ? &*replay.events : nullptr;
    if (Events::events)
        Events::event = Events::events->events.begin();

    Frames::frames = replay.frames ? &*replay.frames : nullptr;
    if (Frames::frames)
        Frames::score = Frames::frames->scores.begin();

    interpolatedPose = replay.poses.front();
    index = 0;

    modifiers = CreateModifiers();
    practice = CreatePracticeSettings();

    bool leftHanded = player->_leftHanded;
    player->_leftHanded = replay.info.modifiers.leftHanded;

    // setting the local transform doesn't work for some reason, even after the scene change
    auto roomAdjust = UnityEngine::Object::FindObjectOfType<VRCenterAdjust*>(true);
    auto oldPosAdj = roomAdjust->_settingsManager->settings.room.center;
    auto oldRotAdj = roomAdjust->_settingsManager->settings.room.rotation;
    roomAdjust->ResetRoom();

    auto resetChanges = [player, leftHanded, roomAdjust, oldPosAdj, oldRotAdj]() {
        player->_leftHanded = leftHanded;
        roomAdjust->_settingsManager->settings.room.center = oldPosAdj;
        roomAdjust->_settingsManager->settings.room.rotation = oldRotAdj;
    };

    MetaCore::Events::AddCallback(MetaCore::Events::GameplaySceneEnded, resetChanges, true);
}

void Playback::ProcessSaber(Saber* saber) {
    if (!Manager::Replaying())
        return;

    Replay::Transform const& hand = saber->saberType == SaberType::SaberA ? interpolatedPose.leftHand : interpolatedPose.rightHand;

    // controller offset
    saber->transform->SetLocalPositionAndRotation(Vector3::zero(), Quaternion::identity());

    auto saberParent = saber->transform->parent;
    if (Manager::GetCurrentInfo().positionsAreLocal) {
        saberParent->localRotation = hand.rotation;
        saberParent->localPosition = hand.position;
    } else {
        saberParent->rotation = hand.rotation;
        saberParent->position = hand.position;
    }
}

void Playback::ProcessScore(ScoreController* controller) {
    if (!Manager::Replaying())
        return;

    // potentially necessary if no scoring elements were despawned, preventing ProcessMaxScore from being called
    if (Frames::AllowScoreOverride() && controller->_multipliedScore != Frames::score->score) {
        controller->_multipliedScore = Frames::score->score;
        if (Frames::score->percent > 0)
            controller->_immediateMaxPossibleMultipliedScore = Frames::score->score / Frames::score->percent;
        float multiplier = controller->_gameplayModifiersModel->GetTotalMultiplier(controller->_gameplayModifierParams, Frames::score->energy);
        controller->_modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(Frames::score->score, multiplier);
        controller->_immediateMaxPossibleModifiedScore =
            ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(controller->immediateMaxPossibleMultipliedScore, multiplier);
        if (!controller->scoreDidChangeEvent->Equals(nullptr))
            controller->scoreDidChangeEvent->Invoke(Frames::score->score, controller->modifiedScore);
    }
}

void Playback::ProcessMaxScore(ScoreController* controller) {
    if (Manager::Replaying() && Frames::AllowScoreOverride()) {
        controller->_multipliedScore = Frames::score->score;
        if (Frames::score->percent > 0)
            controller->_immediateMaxPossibleMultipliedScore = Frames::score->score / Frames::score->percent;
    }
}

bool Playback::ProcessEnergy(GameEnergyCounter* counter) {
    if (!Manager::Replaying())
        return true;

    bool battery = counter->energyType == GameplayModifiers::EnergyType::Battery;

    if (Events::wallEnergyLoss > 0) {
        float gameEnergyLoss = UnityEngine::Time::get_deltaTime() * 1.3;
        if (battery) {
            counter->ProcessEnergyChange(-1);
            Events::wallEnergyLoss = 0;
        } else if (gameEnergyLoss >= Events::wallEnergyLoss) {
            counter->ProcessEnergyChange(-Events::wallEnergyLoss);
            Events::wallEnergyLoss = 0;
        } else {
            counter->ProcessEnergyChange(-gameEnergyLoss);
            Events::wallEnergyLoss -= gameEnergyLoss;
        }
    }

    if (float energy = Frames::score->energy; energy >= 0) {
        if (battery) {
            counter->energy = energy;
            // process the fail if needed, otherwise just send the callback
            counter->ProcessEnergyChange(energy == 0 ? -1 : 0);
        } else
            counter->ProcessEnergyChange(energy - counter->energy);
    }

    return false;
}
