#include "pause.hpp"

#include "CustomTypes/Grabbable.hpp"
#include "GlobalNamespace/AudioManagerSO.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/BeatmapCallbacksUpdater.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/CallbacksInTime.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/ComboUIController.hpp"
#include "GlobalNamespace/CutScoreBuffer.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/GameEnergyUIPanel.hpp"
#include "GlobalNamespace/GoodCutScoringElement.hpp"
#include "GlobalNamespace/MemoryPoolContainer_1.hpp"
#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/SaberSwingRatingCounter.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreMultiplierCounter.hpp"
#include "GlobalNamespace/ScoringElement.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "System/Collections/Generic/HashSet_1.hpp"
#include "UnityEngine/Animator.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Playables/PlayableDirector.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Time.hpp"
#include "config.hpp"
#include "manager.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/il2cpp.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/stats.hpp"
#include "metacore/shared/strings.hpp"
#include "metacore/shared/unity.hpp"
#include "playback.hpp"
#include "utils.hpp"

static bool inited = false;

static GlobalNamespace::PauseMenuManager* pauseMenu;
static GlobalNamespace::ComboUIController* comboPanel;
static GlobalNamespace::NoteCutSoundEffectManager* soundManager;
static GlobalNamespace::AudioManagerSO* audioManager;
static GlobalNamespace::GameEnergyUIPanel* energyPanel;

static UnityEngine::GameObject* cameraModel;
static BSML::SliderSetting* timeSlider;
static BSML::SliderSetting* speedSlider;

static void SetCameraModelToThirdPerson() {
    auto trans = cameraModel->transform;
    Quaternion rot = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
    // model points upwards
    Quaternion offset = Quaternion::AngleAxis(90, {1, 0, 0});
    rot = rot * offset;
    auto pos = getConfig().ThirdPerPos.GetValue();
    trans->SetPositionAndRotation(pos, rot);
}

static void SetThirdPersonToCameraModel() {
    Quaternion rot = cameraModel->transform->rotation;
    // model points upwards
    auto offset = Quaternion::AngleAxis(-90, {1, 0, 0});
    rot = rot * offset;
    getConfig().ThirdPerRot.SetValue(rot.eulerAngles);
    getConfig().ThirdPerPos.SetValue(cameraModel->transform->position);
}

static UnityEngine::GameObject*
CreateCube(UnityEngine::GameObject* parent, UnityEngine::Material* mat, Vector3 pos, Vector3 euler, Vector3 scale, std::string_view name) {
    auto cube = UnityEngine::GameObject::CreatePrimitive(UnityEngine::PrimitiveType::Cube);
    cube->GetComponent<UnityEngine::Renderer*>()->SetMaterial(mat);
    auto trans = cube->transform;
    if (parent)
        trans->SetParent(parent->transform, false);
    trans->localPosition = pos;
    trans->localEulerAngles = euler;
    trans->localScale = scale;
    cube->name = name;
    return cube;
}

static UnityEngine::GameObject* CreateCameraModel() {
    auto shader = UnityEngine::Shader::Find("Hidden/Internal-Colored");
    if (!shader) {
        logger.error("Failed to find Hidden/Internal-Colored shader!");
        return UnityEngine::GameObject::New_ctor("ReplayFailedCameraModel");
    }
    auto mat = UnityEngine::Material::New_ctor(shader);
    mat->color = UnityEngine::Color::get_white();
    auto ret = CreateCube(nullptr, mat, {0, 0, 0}, {0, 0, 0}, {0.075, 0.075, 0.075}, "ReplayCameraModel");
    CreateCube(ret, mat, {-1.461, 1.08, 1.08}, {45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 0");
    CreateCube(ret, mat, {1.461, 1.08, 1.08}, {45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 1");
    CreateCube(ret, mat, {1.461, 1.08, -1.08}, {-45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 2");
    CreateCube(ret, mat, {-1.461, 1.08, -1.08}, {-45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 3");
    CreateCube(ret, mat, {0, 2.08, 0}, {0, 0, 0}, {5.845, 0.07, 4.322}, "Camera Screen");
    ret->active = false;
    return ret;
}

static void UpdateCameraActive() {
    if (!inited)
        return;
    SetCameraModelToThirdPerson();
    cameraModel->active = !Manager::Rendering() && Manager::Paused() && getConfig().CamMode.GetValue() == (int) CameraMode::ThirdPerson;
}

static BSML::SliderSetting* TextlessSlider(
    BSML::Lite::TransformWrapper parent,
    float min,
    float max,
    float current,
    float increment,
    std::function<void(float)> onChange,
    std::function<StringW(float)> format,
    bool showButtons = false
) {
    auto madeSlider = BSML::Lite::CreateSliderSetting(parent, "", 0, 0, 0, 0, 0, showButtons, {});
    auto slider = madeSlider->slider->gameObject->AddComponent<BSML::SliderSetting*>();
    slider->slider = madeSlider->slider;
    slider->onChange = std::move(onChange);
    slider->formatter = std::move(format);
    slider->increments = increment;
    slider->slider->minValue = min;
    slider->slider->maxValue = max;
    slider->Setup();
    auto transform = slider->slider->GetComponent<UnityEngine::RectTransform*>();
    auto object = transform->parent->gameObject;
    transform->SetParent(parent->transform, false);
    transform->anchorMin = {0.5, 0.5};
    transform->anchorMax = {0.5, 0.5};
    transform->pivot = {0.5, 0.5};
    UnityEngine::Object::Destroy(object);
    return slider;
}

static void SetTransform(BSML::Lite::TransformWrapper obj, UnityEngine::Vector2 anchoredPos, UnityEngine::Vector2 sizeDelta) {
    auto transform = obj->GetComponent<UnityEngine::RectTransform*>();
    transform->anchoredPosition = anchoredPos;
    transform->sizeDelta = sizeDelta;
}

static void CreateUI() {
    auto parent = BSML::Lite::CreateCanvas();
    parent->AddComponent<HMUI::Screen*>();
    parent->name = "ReplayPauseMenu";
    parent->transform->SetParent(pauseMenu->_restartButton->transform->parent, false);
    parent->transform->localScale = Vector3::one();
    SetTransform(parent, {0, -15}, {110, 25});

    float startTime = 0;
    float endTime = MetaCore::Stats::GetSongLength();

    auto& info = Manager::GetCurrentInfo();
    if (info.practice)
        startTime = info.startTime;
    if (info.failed && info.failTime >= 0)
        endTime = info.failTime;
    else if (info.quit)
        endTime = info.quitTime;
    if (endTime < startTime + 1)
        endTime = startTime + 1;

    timeSlider = TextlessSlider(parent, startTime, endTime, startTime, (endTime - startTime) / 1000, Pause::SetTime, [](float time) {
        return MetaCore::Strings::SecondsToString(time);
    });
    SetTransform(timeSlider, {0, 6}, {100, 10});

    speedSlider = TextlessSlider(parent, 0.1, 2.5, 1, 0.05, Pause::SetSpeed, [](float speed) { return fmt::format("{:.2f}x", speed); }, true);
    SetTransform(speedSlider, {-18, -4}, {65, 10});

    auto dropdown = AddConfigValueDropdownEnum(parent, getConfig().CamMode, CameraModes);
    dropdown->onChange = [onChange = dropdown->onChange](System::Object* value) {
        onChange(value);
        UpdateCameraActive();
    };
    auto transform = dropdown->transform->parent;
    transform->Find("Label")->GetComponent<TMPro::TextMeshProUGUI*>()->text = "";
    SetTransform(transform, {-10, -11.5}, {10, 10});
}

static void UpdateUI() {
    if (!inited)
        return;
    timeSlider->set_Value(MetaCore::Stats::GetSongTime());
    speedSlider->set_Value(MetaCore::Internals::audioTimeSyncController->_timeScale);
}

static bool LazyInit() {
    if (inited || !MetaCore::Internals::referencesValid)
        return inited;
    inited = true;

    pauseMenu = UnityEngine::Object::FindObjectOfType<GlobalNamespace::PauseMenuManager*>(true);
    comboPanel = UnityEngine::Object::FindObjectOfType<GlobalNamespace::ComboUIController*>(true);
    soundManager = UnityEngine::Object::FindObjectOfType<GlobalNamespace::NoteCutSoundEffectManager*>(true);
    audioManager = soundManager->_audioManager;
    energyPanel = UnityEngine::Object::FindObjectOfType<GlobalNamespace::GameEnergyUIPanel*>(true);

    CreateUI();
    cameraModel = CreateCameraModel();
    cameraModel->AddComponent<Replay::Grabbable*>()->onRelease = SetThirdPersonToCameraModel;
    MetaCore::Engine::SetOnDestroy(cameraModel, []() { inited = false; });
    UpdateCameraActive();

    return true;
}

static void StopSounds() {
    ListW<GlobalNamespace::NoteCutSoundEffect*> sounds = soundManager->_noteCutSoundEffectPoolContainer->activeItems;
    for (auto sound : sounds)
        sound->StopPlayingAndFinish();
    soundManager->_prevNoteATime = -1;
    soundManager->_prevNoteBTime = -1;
}

void Pause::OnPause() {
    if (!LazyInit())
        return;
    UpdateCameraActive();
    UpdateUI();
    StopSounds();
}

void Pause::OnUnpause() {
    if (!inited)
        return;
    cameraModel->active = false;
    SetThirdPersonToCameraModel();
    MetaCore::Internals::audioTimeSyncController->_inBetweenDSPBufferingTimeEstimate = 0;
}

void Pause::SetSpeed(float value) {
    if (!LazyInit())
        return;
    auto audioController = MetaCore::Internals::audioTimeSyncController;
    audioController->_timeScale = value;
    audioController->_audioSource->pitch = value;
    audioController->_audioStartTimeOffsetSinceStart =
        (UnityEngine::Time::get_timeSinceLevelLoad() * value) - (audioController->songTime + audioController->_initData->songTimeOffset);
    audioManager->musicPitch = 1 / value;
}

using EventsIterator = decltype(Replay::Events::Data::events)::iterator;

static EventsIterator FindNextEvent(Replay::Events::Data& events, float time) {
    return events.events.lower_bound(time);
}

static bool ShouldCountNote(Replay::Events::NoteInfo const& note, bool oldScoring) {
    if (note.eventType == Replay::Events::NoteInfo::Type::BOMB)
        return false;
    return Utils::ScoringTypeMatches(note.scoringType, GlobalNamespace::NoteData::ScoringType::Normal, oldScoring) ||
           Utils::ScoringTypeMatches(note.scoringType, GlobalNamespace::NoteData::ScoringType::ArcHead, oldScoring) ||
           Utils::ScoringTypeMatches(note.scoringType, GlobalNamespace::NoteData::ScoringType::ArcTail, oldScoring) ||
           Utils::ScoringTypeMatches(note.scoringType, GlobalNamespace::NoteData::ScoringType::ArcHeadArcTail, oldScoring) ||
           Utils::ScoringTypeMatches(note.scoringType, GlobalNamespace::NoteData::ScoringType::ChainHead, oldScoring) ||
           Utils::ScoringTypeMatches(note.scoringType, GlobalNamespace::NoteData::ScoringType::ChainHeadArcTail, oldScoring);
}

#define MODIFY(var, num) \
    var += forwards ? (num) : -(num)

#define SIDED_MOD_1(post, num) \
    MODIFY((left ? MetaCore::Internals::left##post : MetaCore::Internals::right##post), num)

#define SIDED_MOD_2(pre, post, num) \
    MODIFY((left ? MetaCore::Internals::pre##Left##post : MetaCore::Internals::pre##Right##post), num)

static void CalculateNoteChanges(Replay::Events::Data& events, EventsIterator event, bool forwards) {
    auto& note = events.notes[event->index];

    bool left = note.info.colorType == 0;
    bool mistake = note.info.eventType != Replay::Events::NoteInfo::Type::GOOD;
    bool fixed = note.info.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ChainLink ||
                 note.info.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ChainLinkArcHead;
    bool count = ShouldCountNote(note.info, events.hasOldScoringTypes);

    auto [pre, post, acc, score] = Utils::ScoreForNote(note, events.hasOldScoringTypes);
    auto [maxPre, maxPost, maxAcc, maxScore] = Utils::ScoreForNote(note, events.hasOldScoringTypes, true);

    // chains are set to full post swing for average calculations
    if (maxPost == 0 && !fixed)
        post = 30;

    int mult = event->multiplier;
    int maxMult = MetaCore::Stats::GetMaxMultiplier();

    if (count)
        SIDED_MOD_2(remainingNotes, , -1);

    if (note.info.eventType == Replay::Events::NoteInfo::Type::BOMB)
        SIDED_MOD_2(bombs, Hit, 1);
    else if (note.info.eventType == Replay::Events::NoteInfo::Type::BAD)
        SIDED_MOD_2(notes, BadCut, 1);
    else if (note.info.eventType == Replay::Events::NoteInfo::Type::MISS)
        SIDED_MOD_2(notes, Missed, 1);
    else if (count) {
        SIDED_MOD_2(notes, Cut, 1);
        SIDED_MOD_1(PreSwing, pre);
        SIDED_MOD_1(PostSwing, post);
        SIDED_MOD_1(Accuracy, acc);
        SIDED_MOD_1(TimeDependence, std::abs(note.noteCutInfo.cutNormal.z));
        // the game uses the multipliers from after the cut, and we calculate it based on the notes cut (and missed etc)
        if (forwards)
            maxMult = MetaCore::Stats::GetMaxMultiplier();
    }

    SIDED_MOD_1(Score, score * mult);
    SIDED_MOD_1(MaxScore, maxScore * maxMult);
    if (mistake) {
        if (fixed)
            SIDED_MOD_1(MissedFixedScore, maxScore * maxMult);
        else
            SIDED_MOD_1(MissedMaxScore, maxScore * maxMult);
    } else
        SIDED_MOD_1(MissedFixedScore, score * maxMult - score * mult);
}

static void CalculateWallChanges(Replay::Events::Wall const& event, bool forwards) {
    MODIFY(MetaCore::Internals::wallsHit, 1);
}

static void CalculateEventChanges(Replay::Data& replay, float time) {
    float current = MetaCore::Stats::GetSongTime();
    bool forwards = time > current;

    bool hadFailed = MetaCore::Internals::health == 0;

    auto& events = *replay.events;
    auto event = FindNextEvent(events, current);
    auto stop = FindNextEvent(events, time);

    if (event == stop)
        return;

    while (event != stop) {
        // if going forwards, we want to stop before the next event to be processed,
        // but for going backwards, we want to include it in the calculations
        auto next = forwards ? event++ : --event;
        switch (next->eventType) {
            case Replay::Events::Reference::Note:
                CalculateNoteChanges(events, next, forwards);
                break;
            case Replay::Events::Reference::Wall:
                CalculateWallChanges(events.walls[next->index], forwards);
                break;
            default:
                break;
        }
    }

    // since stop is the next event to be processed whether forwards or backwards, we want to set these values to the "current" event instead
    if (stop != events.events.begin())
        stop--;

    if (time >= stop->time) {
        // precalculated because it's a pain going backwards
        MetaCore::Internals::combo = stop->combo;
        MetaCore::Internals::leftCombo = stop->leftCombo;
        MetaCore::Internals::rightCombo = stop->rightCombo;

        MetaCore::Internals::highestCombo = stop->maxCombo;
        MetaCore::Internals::highestLeftCombo = stop->maxLeftCombo;
        MetaCore::Internals::highestRightCombo = stop->maxRightCombo;

        MetaCore::Internals::multiplier = stop->multiplier;
        MetaCore::Internals::multiplierProgress = stop->multiplierProgress;

        MetaCore::Internals::health = stop->energy;
    } else {
        // the first event will often be the first cut, and therefore will have the values from after it
        MetaCore::Internals::combo = 0;
        MetaCore::Internals::leftCombo = 0;
        MetaCore::Internals::rightCombo = 0;

        MetaCore::Internals::highestCombo = 0;
        MetaCore::Internals::highestLeftCombo = 0;
        MetaCore::Internals::highestRightCombo = 0;

        MetaCore::Internals::multiplier = 1;
        MetaCore::Internals::multiplierProgress = 0;

        MetaCore::Internals::health = replay.info.modifiers.oneLife || replay.info.modifiers.fourLives ? 1 : 0.5;
    }

    bool nowFailed = MetaCore::Internals::health == 0;

    if (MetaCore::Internals::noFail) {
        if (hadFailed && !nowFailed)
            MetaCore::Internals::negativeMods += 0.5;
        else if (!hadFailed && nowFailed)
            MetaCore::Internals::negativeMods -= 0.5;
    }
}

static void CalculateFrameChanges(Replay::Data& replay, float time) {
    auto& frames = *replay.frames;
    auto nextScore = std::lower_bound(frames.scores.begin(), frames.scores.end(), time, Replay::TimeSearcher<Replay::Frames::Score>());
    if (nextScore != frames.scores.begin())
        nextScore--;

    // divide equally since we don't have the info
    MetaCore::Internals::leftScore = nextScore->score / 2;
    MetaCore::Internals::rightScore = (nextScore->score + 1) / 2;

    if (nextScore->percent >= 0) {
        int maxScore = nextScore->score / nextScore->percent;
        MetaCore::Internals::leftMaxScore = maxScore / 2;
        MetaCore::Internals::rightMaxScore = (maxScore + 1) / 2;
    }

    MetaCore::Internals::combo = nextScore->combo;
    MetaCore::Internals::leftCombo = nextScore->combo / 2;
    MetaCore::Internals::rightCombo = (nextScore->combo + 1) / 2;

    MetaCore::Internals::highestCombo = nextScore->maxCombo;
    MetaCore::Internals::highestLeftCombo = nextScore->maxCombo / 2;
    MetaCore::Internals::highestRightCombo = (nextScore->maxCombo + 1) / 2;

    MetaCore::Internals::health = nextScore->energy;

    MetaCore::Internals::multiplier = nextScore->multiplier;
    MetaCore::Internals::multiplierProgress = nextScore->multiplierProgress;
}

static void ResetEnergyBar() {
    if (!energyPanel->isActiveAndEnabled)
        return;
    energyPanel->_playableDirector->Stop();
    energyPanel->_playableDirector->RebindPlayableGraphOutputs();
    energyPanel->_playableDirector->Evaluate();
    energyPanel->_energyBar->enabled = true;
    static auto opacityColor = UnityEngine::Color(1, 1, 1, 0.3);
    auto emptyIcon = energyPanel->transform->Find("EnergyIconEmpty");
    emptyIcon->localPosition = {-59, 0, 0};
    emptyIcon->GetComponent<HMUI::ImageView*>()->color = opacityColor;
    auto fullIcon = energyPanel->transform->Find("EnergyIconFull");
    fullIcon->localPosition = {59, 0, 0};
    fullIcon->GetComponent<HMUI::ImageView*>()->color = opacityColor;
    energyPanel->transform->Find("Laser")->gameObject->active = false;
    // prevent recreation of battery energy UI
    auto energyType = MetaCore::Internals::gameEnergyCounter->energyType;
    MetaCore::Internals::gameEnergyCounter->energyType = -1;
    // reregister HandleGameEnergyDidChange
    energyPanel->Init();
    MetaCore::Internals::gameEnergyCounter->energyType = energyType;
}

static void FinishScoringElements() {
    MetaCore::Internals::scoreController->_sortedNoteTimesWithoutScoringElements->Clear();
    MetaCore::Internals::scoreController->LateUpdate();
    for (auto element : ListW<GlobalNamespace::ScoringElement*>(MetaCore::Internals::scoreController->_scoringElementsWithMultiplier)) {
        if (auto goodElement = il2cpp_utils::try_cast<GlobalNamespace::GoodCutScoringElement>(element).value_or(nullptr))
            goodElement->_cutScoreBuffer->_saberSwingRatingCounter->Finish();
    }
    MetaCore::Internals::scoreController->LateUpdate();
}

static void UpdateBaseGameState() {
    float time = MetaCore::Stats::GetSongTime();

    // remove currently spawned objects
    ListW<GlobalNamespace::IBeatmapObjectController*> objects = MetaCore::Internals::beatmapObjectManager->_allBeatmapObjects;
    for (auto object : objects) {
        object->Hide(false);
        object->Pause(false);
        ((UnityEngine::Component*) object)->gameObject->active = true;
        // scoresaber does all the above, is it necessary?
        object->Dissolve(0);
    }
    MetaCore::Internals::scoreController->_playerHeadAndObstacleInteraction->_intersectingObstacles->Clear();

    // (effectively) clear all current callbacks
    auto callbacks = MetaCore::Internals::beatmapCallbacksController->_callbacksInTimes;
    for (auto callback : DictionaryW(callbacks).values())
        callback->lastProcessedNode = nullptr;
    MetaCore::Internals::beatmapCallbacksController->_prevSongTime = time - 0.01;
    MetaCore::Internals::beatmapCallbacksController->_songTime = time;
    MetaCore::Internals::beatmapCallbacksController->_startFilterTime = time;

    // update song time
    float controllerTime =
        (time - MetaCore::Internals::audioTimeSyncController->_startSongTime) / MetaCore::Internals::audioTimeSyncController->timeScale;
    MetaCore::Internals::audioTimeSyncController->SeekTo(controllerTime);

    // update energy and its ui
    float health = MetaCore::Stats::GetHealth();
    MetaCore::Internals::gameEnergyCounter->_energy_k__BackingField = health;
    MetaCore::Internals::gameEnergyCounter->_nextFrameEnergyChange = 0;
    MetaCore::Internals::gameEnergyCounter->_didReach0Energy = health == 0;
    if (health > 0 && !energyPanel->_energyBar->enabled)
        ResetEnergyBar();
    if (!System::Object::Equals(MetaCore::Internals::gameEnergyCounter->gameEnergyDidChangeEvent, nullptr))
        MetaCore::Internals::gameEnergyCounter->gameEnergyDidChangeEvent->Invoke(health);

    auto scoreController = MetaCore::Internals::scoreController;
    // update score multipliers
    auto multiplierCounter = scoreController->_scoreMultiplierCounter;
    multiplierCounter->_multiplier = MetaCore::Stats::GetMultiplier();
    multiplierCounter->_multiplierIncreaseProgress = MetaCore::Stats::GetMultiplierProgress(false) * multiplierCounter->_multiplier * 2;
    multiplierCounter->_multiplierIncreaseMaxProgress = multiplierCounter->_multiplier * 2;

    auto maxMultiplierCounter = scoreController->_maxScoreMultiplierCounter;
    maxMultiplierCounter->_multiplier = MetaCore::Stats::GetMaxMultiplier();
    maxMultiplierCounter->_multiplierIncreaseProgress = MetaCore::Stats::GetMaxMultiplierProgress(false) * maxMultiplierCounter->_multiplier * 2;
    maxMultiplierCounter->_multiplierIncreaseMaxProgress = maxMultiplierCounter->_multiplier * 2;

    // update score and max score
    float modifiers = MetaCore::Stats::GetModifierMultiplier(true, true);
    scoreController->_multipliedScore = MetaCore::Stats::GetScore(MetaCore::Stats::BothSabers);
    scoreController->_modifiedScore = scoreController->_multipliedScore * modifiers;
    scoreController->_immediateMaxPossibleMultipliedScore = MetaCore::Stats::GetMaxScore(MetaCore::Stats::BothSabers);
    scoreController->_immediateMaxPossibleModifiedScore = scoreController->_immediateMaxPossibleMultipliedScore * modifiers;
    if (scoreController->_immediateMaxPossibleMultipliedScore == 0)
        scoreController->_immediateMaxPossibleModifiedScore = 1;
    scoreController->scoreDidChangeEvent->Invoke(scoreController->_multipliedScore, scoreController->_modifiedScore);

    // update combo
    MetaCore::Internals::comboController->_combo = MetaCore::Stats::GetCombo(MetaCore::Stats::BothSabers);
    MetaCore::Internals::comboController->_maxCombo = MetaCore::Stats::GetHighestCombo(MetaCore::Stats::BothSabers);
    if (!System::Object::Equals(MetaCore::Internals::comboController->comboDidChangeEvent, nullptr))
        MetaCore::Internals::comboController->comboDidChangeEvent->Invoke(MetaCore::Internals::comboController->_combo);

    bool full = MetaCore::Stats::GetFullCombo(MetaCore::Stats::BothSabers);
    if (full && comboPanel->_fullComboLost)
        comboPanel->_animator->Rebind();
    else if (!full && !comboPanel->_fullComboLost)
        comboPanel->_animator->SetTrigger(comboPanel->_comboLostId);
    comboPanel->_fullComboLost = !full;
}

void Pause::SetTime(float value) {
    if (!Manager::Replaying() || MetaCore::Stats::GetSongTime() == value || !LazyInit())
        return;

    logger.info("Setting replay time to {}", value);

    // I don't fully understand it, but there is some weirdness with the score calculation if any elements are left unfinished
    // this function is *probably* unnecessary, since the custom saber movement data finishes everything within the frame,
    // but I'm going to leave it here just to be safe (even though without the custom movement this function doesn't fix it)
    FinishScoringElements();

    auto& replay = Manager::GetCurrentReplay();

    if (replay.events)
        CalculateEventChanges(replay, value);
    else if (replay.frames)
        CalculateFrameChanges(replay, value);

    Playback::SeekTo(value);

    MetaCore::Internals::songTime = value;

    // would be extra cool if I could do lighting, but man that seems annoying, especially backwards
    UpdateBaseGameState();

    // technically not all guaranteed to have happened, but the idea is just to make stuff update
    MetaCore::Events::Broadcast(MetaCore::Events::ScoreChanged);
    MetaCore::Events::Broadcast(MetaCore::Events::ComboChanged);
    MetaCore::Events::Broadcast(MetaCore::Events::HealthChanged);
    MetaCore::Events::Broadcast(MetaCore::Events::NoteCut);
    MetaCore::Events::Broadcast(MetaCore::Events::NoteMissed);
    MetaCore::Events::Broadcast(MetaCore::Events::BombCut);
    MetaCore::Events::Broadcast(MetaCore::Events::WallHit);
    MetaCore::Events::Broadcast(MetaCore::Events::Update);
}

void Pause::UpdateInputs() {
    int skip = Utils::IsButtonDown(getConfig().TimeButton.GetValue());
    if (skip)
        SetTime(MetaCore::Stats::GetSongTime() + getConfig().TimeSkip.GetValue() * skip);

    int speed = Utils::IsButtonDown(getConfig().SpeedButton.GetValue());
    if (speed)
        SetSpeed(MetaCore::Internals::audioTimeSyncController->_timeScale + speed * 0.05);
}
