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

struct NoteComparer {
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

    static void SeekTo(float time) {
        if (!frames)
            return;
        score = std::lower_bound(frames->scores.begin(), frames->scores.end(), time, Replay::TimeSearcher<Replay::Frames::Score>());
    }

    static bool AllowComboDrop() {
        if (!frames || Manager::Paused())
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
        if (!frames || Manager::Paused())
            return false;
        if (score->percent >= 0)
            return true;
        // fix scoresaber replays having incorrect max score before cut finishes (from the original play)
        return MetaCore::Stats::GetSongTime() - lastCutTime > 0.4;
    }

    static void ProcessEnergy(GameEnergyCounter* counter) {
        if (!frames || Manager::Paused())
            return;
        if (score->energy >= 0) {
            if (counter->energyType == GameplayModifiers::EnergyType::Battery) {
                counter->energy = score->energy;
                // process the fail if needed, otherwise just send the callback
                counter->ProcessEnergyChange(score->energy == 0 ? -1 : 0);
            } else
                counter->ProcessEnergyChange(score->energy - counter->energy);
        }
    }
}

namespace Events {
    static std::set<NoteController*, NoteComparer> notes;
    static Replay::Events::Data const* events;
    static decltype(events->events)::const_iterator event;
    static float wallEndTime;
    static float wallEnergyLoss;

    static bool Matches(NoteData* data, Replay::Events::NoteInfo const& info) {
        return Utils::ScoringTypeMatches(info.scoringType, data->scoringType, events->hasOldScoringTypes) &&
               (int) data->lineIndex == info.lineIndex && (int) data->noteLineLayer == info.lineLayer && (int) data->colorType == info.colorType &&
               (int) data->cutDirection == info.cutDirection;
    }

    static void RunNoteEvent(Replay::Events::Note const& noteEvent, NoteController* controller) {
        static auto sendCut = il2cpp_utils::FindMethodUnsafe(classof(NoteController*), "SendNoteWasCutEvent", 1);

        bool isLeftSaber = noteEvent.noteCutInfo.saberType == (int) SaberType::SaberA;
        auto sabers = MetaCore::Internals::saberManager;
        auto saber = isLeftSaber ? sabers->_leftSaber : sabers->_rightSaber;

        switch (noteEvent.info.eventType) {
            case Replay::Events::NoteInfo::Type::GOOD:
            case Replay::Events::NoteInfo::Type::BAD: {
                auto cutInfo = Utils::GetNoteCutInfo(controller, saber, noteEvent.noteCutInfo);
                if (events->cutInfoMissingOKs) {
                    cutInfo.speedOK = cutInfo.saberSpeed > 2;
                    bool isLeftColor = controller->noteData->colorType == ColorType::ColorA;
                    cutInfo.saberTypeOK = isLeftColor == isLeftSaber;
                    cutInfo.timeDeviation = controller->noteData->time - noteEvent.time;
                }
                Frames::lastCutTime = noteEvent.time;
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

    static void ProcessNoteEvent(Replay::Events::Note const& noteEvent) {
        auto& info = noteEvent.info;

        for (auto iter = notes.begin(); iter != notes.end(); iter++) {
            auto controller = *iter;
            auto data = controller->noteData;
            if (!Matches(data, info))
                continue;
            RunNoteEvent(noteEvent, controller);
            if (info.eventType == Replay::Events::NoteInfo::Type::MISS)
                notes.erase(iter);  // note will despawn and be removed in the other cases
            return;
        }

        int bsorID = (noteEvent.info.scoringType + 2) * 10000 + noteEvent.info.lineIndex * 1000 + noteEvent.info.lineLayer * 100 +
                     noteEvent.info.colorType * 10 + noteEvent.info.cutDirection;
        logger.error("Could not find note for event! time: {}, bsor id: {}", noteEvent.time, bsorID);
    }

    static void ProcessWallEvent(Replay::Events::Wall const& wallEvent) {
        auto obstacles = MetaCore::Internals::gameEnergyCounter->_playerHeadAndObstacleInteraction;
        obstacles->headDidEnterObstacleEvent->Invoke(nullptr);
        obstacles->headDidEnterObstaclesEvent->Invoke();
        float diffStartTime = std::max(wallEndTime, wallEvent.time);
        wallEndTime = std::max(wallEndTime, wallEvent.endTime);
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

    static void SeekTo(float time) {
        if (!events)
            return;
        event = events->events.lower_bound(time);
    }

    static void ProcessEnergy(GameEnergyCounter* counter) {
        if (!events)
            return;
        if (wallEnergyLoss != 0) {
            bool battery = counter->energyType == GameplayModifiers::EnergyType::Battery;
            float gameEnergyLoss = UnityEngine::Time::get_deltaTime() * 1.3;

            if (battery) {
                counter->ProcessEnergyChange(-1);
                wallEnergyLoss = 0;
            } else if (gameEnergyLoss >= wallEnergyLoss) {
                counter->ProcessEnergyChange(-wallEnergyLoss);
                wallEnergyLoss = 0;
            } else {
                counter->ProcessEnergyChange(-gameEnergyLoss);
                wallEnergyLoss -= gameEnergyLoss;
            }
        }
        if (counter->_nextFrameEnergyChange != 0) {
            counter->ProcessEnergyChange(counter->_nextFrameEnergyChange);
            counter->_nextFrameEnergyChange = 0;
        }
    }
}

static Replay::Pose interpolatedPose;
static int index = 0;

static Replay::Transform Lerp(Replay::Transform const& start, Replay::Transform const& end, float t) {
    return {Vector3::Lerp(start.position, end.position, t), Quaternion::Lerp(start.rotation, end.rotation, t)};
}

static Replay::Pose GetInterpolatedPose(std::vector<Replay::Pose> const& poses, float time) {
    if (index == 0)
        return poses.front();
    if (index >= poses.size())
        return poses.back();

    int prevIndex = index - 1;
    while (prevIndex > 0 && poses[prevIndex].time > time)
        prevIndex--;
    auto const& prev = poses[prevIndex];
    auto const& next = poses[index];

    float poseDuration = next.time - prev.time;
    if (poseDuration == 0)
        return prev;

    float lerpAmount = (time - prev.time) / poseDuration;
    return {
        time,
        (int) std::lerp(prev.fps, next.fps, lerpAmount),
        Lerp(prev.head, next.head, lerpAmount),
        Lerp(prev.leftHand, next.leftHand, lerpAmount),
        Lerp(prev.rightHand, next.rightHand, lerpAmount)
    };
}

void Playback::UpdateTime() {
    if (!Manager::Replaying())
        return;

    float time = MetaCore::Stats::GetSongTime();
    Frames::UpdateTime(time);
    Events::UpdateTime(time);

    auto& poses = Manager::GetCurrentReplay().poses;

    while (index < poses.size() && poses[index].time < time)
        index++;
    interpolatedPose = GetInterpolatedPose(poses, time);
}

void Playback::SeekTo(float time) {
    if (!Manager::Replaying())
        return;

    Frames::SeekTo(time);
    Events::SeekTo(time);

    auto& poses = Manager::GetCurrentReplay().poses;
    index = std::distance(poses.begin(), std::lower_bound(poses.begin(), poses.end(), time, Replay::TimeSearcher<Replay::Pose>()));
    interpolatedPose = GetInterpolatedPose(poses, time);
}

Replay::Pose const& Playback::GetPose() {
    return interpolatedPose;
}

bool Playback::DisableRealEvent(bool bad) {
    if (!Manager::Replaying())
        return false;
    if (Events::events)
        return true;
    if (bad && !Frames::AllowComboDrop())
        return true;
    return false;
}

bool Playback::DisableListSorting() {
    return Manager::Replaying() && Events::events;
}

void Playback::AddNoteController(NoteController* note) {
    if (!Manager::Replaying())
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
    logger.debug("process replay start, frames: {} events: {}", replay.frames.has_value(), replay.events.has_value());

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

    MetaCore::Events::AddCallback(MetaCore::Events::MapEnded, resetChanges, true);
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
        if (!System::MulticastDelegate::op_Equality(controller->scoreDidChangeEvent, nullptr))
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

    // make sure to process events first in case both are present
    Events::ProcessEnergy(counter);
    Frames::ProcessEnergy(counter);

    return false;
}
