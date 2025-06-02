#include "pause.hpp"

#include "CustomTypes/Grabbable.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/BeatmapCallbacksUpdater.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/GameEnergyUIPanel.hpp"
#include "GlobalNamespace/MemoryPoolContainer_1.hpp"
#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreMultiplierCounter.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Playables/PlayableDirector.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Time.hpp"
#include "config.hpp"
#include "manager.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/stats.hpp"
#include "metacore/shared/strings.hpp"
#include "metacore/shared/unity.hpp"
#include "playback.hpp"
#include "utils.hpp"

static bool inited = false;

static GlobalNamespace::PauseMenuManager* pauseMenu;
static GlobalNamespace::ScoreController* scoreController;
static GlobalNamespace::AudioTimeSyncController* audioController;
static GlobalNamespace::BeatmapObjectManager* objectManager;
static GlobalNamespace::PlayerHeadAndObstacleInteraction* obstacleInteraction;
static GlobalNamespace::BeatmapCallbacksController* callbackController;
static GlobalNamespace::ComboController* comboController;
static GlobalNamespace::NoteCutSoundEffectManager* soundManager;
static GlobalNamespace::GameEnergyCounter* energyCounter;
static GlobalNamespace::GameEnergyUIPanel* energyPanel;

static UnityEngine::GameObject* cameraModel;

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
    auto trans = cameraModel->transform;
    Quaternion rot = trans->rotation;
    // model points upwards
    auto offset = Quaternion::AngleAxis(-90, {1, 0, 0});
    rot = rot * offset;
    getConfig().ThirdPerRot.SetValue(rot.eulerAngles);
    auto pos = trans->position;
    getConfig().ThirdPerPos.SetValue(pos);
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
    auto mat = UnityEngine::Material::New_ctor(UnityEngine::Shader::Find("Custom/SimpleLit"));
    mat->color = UnityEngine::Color::get_white();
    auto ret = CreateCube(nullptr, mat, {}, {}, {0.075, 0.075, 0.075}, "ReplayCameraModel");
    CreateCube(ret, mat, {-1.461, 1.08, 1.08}, {45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 0");
    CreateCube(ret, mat, {1.461, 1.08, 1.08}, {45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 1");
    CreateCube(ret, mat, {1.461, 1.08, -1.08}, {-45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 2");
    CreateCube(ret, mat, {-1.461, 1.08, -1.08}, {-45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 3");
    CreateCube(ret, mat, {0, 2.08, 0}, {}, {5.845, 0.07, 4.322}, "Camera Screen");
    return ret;
}

static void UpdateCameraActive() {
    if (!Manager::Rendering() && getConfig().CamMode.GetValue() == (int) CameraMode::ThirdPerson) {
        SetCameraModelToThirdPerson();
        cameraModel->active = true;
    } else
        cameraModel->active = false;
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
    if (info.failed)
        endTime = info.failTime;
    if (endTime < startTime + 1)
        endTime = startTime + 1;

    auto timeSlider = TextlessSlider(parent, startTime, endTime, startTime, (endTime - startTime) / 100, Pause::SetTime, [](float time) {
        return MetaCore::Strings::SecondsToString(time);
    });
    SetTransform(timeSlider, {0, 6}, {100, 10});

    auto speedSlider = TextlessSlider(parent, 0.5, 2, 1, 0.1, Pause::SetSpeed, [](float speed) { return fmt::format("{:.1f}x", speed); }, true);
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

static void LazyInit() {
    if (inited)
        return;
    inited = true;

    pauseMenu = UnityEngine::Object::FindObjectOfType<GlobalNamespace::PauseMenuManager*>(true);
    scoreController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::ScoreController*>(true);
    audioController = scoreController->_audioTimeSyncController;
    objectManager = scoreController->_beatmapObjectManager;
    obstacleInteraction = scoreController->_playerHeadAndObstacleInteraction;
    callbackController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::BeatmapCallbacksUpdater*>(true)->_beatmapCallbacksController;
    comboController = UnityEngine::Object::FindObjectOfType<GlobalNamespace::ComboController*>(true);
    soundManager = UnityEngine::Object::FindObjectOfType<GlobalNamespace::NoteCutSoundEffectManager*>(true);
    energyCounter = il2cpp_utils::try_cast<GlobalNamespace::GameEnergyCounter>(scoreController->_gameEnergyCounter).value_or(nullptr);
    energyPanel = UnityEngine::Object::FindObjectOfType<GlobalNamespace::GameEnergyUIPanel*>(true);

    CreateUI();
    cameraModel = CreateCameraModel();
    cameraModel->AddComponent<Replay::Grabbable*>()->onRelease = SetThirdPersonToCameraModel;
    MetaCore::Engine::SetOnDestroy(cameraModel, []() { inited = false; });

    UpdateCameraActive();
}

static void StopSounds() {
    ListW<GlobalNamespace::NoteCutSoundEffect*> sounds = soundManager->_noteCutSoundEffectPoolContainer->activeItems;
    for (auto sound : sounds)
        sound->StopPlayingAndFinish();
    soundManager->_prevNoteATime = -1;
    soundManager->_prevNoteBTime = -1;
}

void Pause::OnPause() {
    LazyInit();
    StopSounds();
}

void Pause::OnUnpause() {
    if (!inited)
        return;
    cameraModel->active = false;
    SetThirdPersonToCameraModel();
}

void Pause::SetSpeed(float value) {
    LazyInit();
    audioController->_timeScale = value;
    audioController->_audioSource->pitch = value;
    audioController->_audioStartTimeOffsetSinceStart =
        (UnityEngine::Time::get_timeSinceLevelLoad() * value) - (audioController->songTime + audioController->_initData->songTimeOffset);
}

using EventsIterator = decltype(Replay::Events::Data::events)::iterator;

static EventsIterator FindNextEvent(Replay::Events::Data& events, float time) {
    return std::lower_bound(events.events.begin(), events.events.end(), time, Replay::Events::Reference::Searcher());
}

static int GetMaxMultiplier() {
    int notesCount = MetaCore::Stats::GetTotalNotes(MetaCore::Stats::BothSabers);
    if (notesCount < 2)
        return 1;
    if (notesCount < 2 + 4)
        return 2;
    if (notesCount < 2 + 4 + 8)
        return 4;
    return 8;
}

static int GetMaxMultiplierProgress() {
    int notesCount = MetaCore::Stats::GetTotalNotes(MetaCore::Stats::BothSabers);
    if (notesCount < 2)
        return notesCount;
    if (notesCount < 2 + 4)
        return notesCount - 2;
    if (notesCount < 2 + 4 + 8)
        return notesCount - 2 - 4;
    return 0;
}

static bool ShouldCountNote(Replay::Events::NoteInfo const& note) {
    if (note.eventType == Replay::Events::NoteInfo::Type::BOMB)
        return false;
    return note.scoringType == (int) GlobalNamespace::NoteData::ScoringType::Normal ||
           note.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ArcHead ||
           note.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ArcTail ||
           note.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ChainHead ||
           note.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ArcHeadArcTail ||
           note.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ChainHeadArcTail;
}

#define MODIFY(var, num) \
    var += forwards ? num : -num

#define SIDED_MOD_1(post, num) \
    MODIFY((left ? MetaCore::Internals::left##post() : MetaCore::Internals::right##post()), num)

#define SIDED_MOD_2(pre, post, num) \
    MODIFY((left ? MetaCore::Internals::pre##Left##post() : MetaCore::Internals::pre##Right##post()), num)

static void CalculateNoteChanges(Replay::Events::Data& events, EventsIterator event, bool forwards) {
    auto& note = events.notes[event->index];

    bool left = note.info.colorType == 0;
    bool mistake = note.info.eventType != Replay::Events::NoteInfo::Type::GOOD;
    bool fixed = note.info.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ChainLink ||
                 note.info.scoringType == (int) GlobalNamespace::NoteData::ScoringType::ChainLinkArcHead;

    if (ShouldCountNote(note.info))
        SIDED_MOD_2(remainingNotes, , -1);
    if (note.info.eventType == Replay::Events::NoteInfo::Type::BOMB)
        SIDED_MOD_2(bombs, Hit, 1);
    else if (note.info.eventType == Replay::Events::NoteInfo::Type::BAD)
        SIDED_MOD_2(notes, BadCut, 1);
    else if (note.info.eventType == Replay::Events::NoteInfo::Type::MISS)
        SIDED_MOD_2(notes, Missed, 1);
    else {
        SIDED_MOD_2(notes, Cut, 1);
        SIDED_MOD_1(PreSwing, std::clamp(note.noteCutInfo.beforeCutRating, (float) 0, (float) 1));
        SIDED_MOD_1(PostSwing, std::clamp(note.noteCutInfo.afterCutRating, (float) 0, (float) 1));
        SIDED_MOD_1(Accuracy, Utils::AccuracyForDistance(note.noteCutInfo.cutDistanceToCenter));
    }

    int mult = event->multiplier;
    int maxMult = GetMaxMultiplier();

    int score = mistake ? 0 : Utils::ScoreForNote(note);
    int max = Utils::ScoreForNote(note, true) * maxMult;

    SIDED_MOD_1(Score, score * mult);
    SIDED_MOD_1(MaxScore, max);
    if (mistake) {
        if (fixed)
            SIDED_MOD_1(MissedFixedScore, max);
        else
            SIDED_MOD_1(MissedMaxScore, max);
    } else
        SIDED_MOD_1(MissedFixedScore, score * maxMult - score * mult);

    if (MetaCore::Internals::health() > 0 || !forwards)
        MODIFY(MetaCore::Internals::health(), Utils::EnergyForNote(note.info));
}

static void CalculateWallChanges(Replay::Events::Wall const& event, bool forwards) {
    MODIFY(MetaCore::Internals::wallsHit(), 1);

    if (MetaCore::Internals::health() > 0 || !forwards)
        MODIFY(MetaCore::Internals::health(), (event.time - event.endTime) * 1.3);
}

static void CalculateEventChanges(Replay::Events::Data& events, float time, int& multiplier, int& multiplierProgress, int& maxCombo) {
    float current = MetaCore::Stats::GetSongTime();
    bool forwards = time > current;

    bool hadFailed = MetaCore::Internals::health() == 0;

    auto event = FindNextEvent(events, current);
    auto stop = FindNextEvent(events, time);

    while (event != stop) {
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

    if (forwards)
        stop--;
    // precalculated because it's a pain going backwards
    MetaCore::Internals::combo() = stop->combo;
    MetaCore::Internals::leftCombo() = stop->leftCombo;
    MetaCore::Internals::rightCombo() = stop->rightCombo;

    bool nowFailed = MetaCore::Internals::health() == 0;

    if (MetaCore::Internals::noFail()) {
        if (hadFailed && !nowFailed)
            MetaCore::Internals::negativeMods() += 0.5;
        else if (!hadFailed && nowFailed)
            MetaCore::Internals::negativeMods() -= 0.5;
    }

    multiplier = stop->multiplier;
    multiplierProgress = stop->multiplierProgress;
    maxCombo = stop->maxCombo;
}

static void CalculateFrameChanges(Replay::Frames::Data& frames, float time, int& multiplier, int& multiplierProgress, int& maxCombo) {
    auto nextScore = std::lower_bound(frames.scores.begin(), frames.scores.end(), time, Replay::Frames::Score::Searcher());
    if (nextScore != frames.scores.begin())
        nextScore--;

    // divide equally since we don't have the info
    MetaCore::Internals::leftScore() = nextScore->score / 2;
    MetaCore::Internals::rightScore() = (nextScore->score + 1) / 2;

    if (nextScore->percent >= 0) {
        int maxScore = nextScore->score / nextScore->percent;
        MetaCore::Internals::leftMaxScore() = maxScore / 2;
        MetaCore::Internals::rightMaxScore() = (maxScore + 1) / 2;
    }

    MetaCore::Internals::combo() = nextScore->combo;
    MetaCore::Internals::leftCombo() = nextScore->combo / 2;
    MetaCore::Internals::rightCombo() = (nextScore->combo + 1) / 2;

    MetaCore::Internals::health() = nextScore->energy;

    multiplier = nextScore->multiplier;
    multiplierProgress = nextScore->multiplierProgress;
    maxCombo = nextScore->maxCombo;
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
    auto energyType = energyCounter->energyType;
    energyCounter->energyType = -1;
    // reregister HandleGameEnergyDidChange
    energyPanel->Init();
    energyCounter->energyType = energyType;
}

static void UpdateBaseGameState(int multiplier, int multiplierProgress, int maxCombo) {
    // update song time (also updates note controllers)
    float time = MetaCore::Stats::GetSongTime();
    float controllerTime = (time - audioController->_startSongTime) / audioController->timeScale;
    audioController->SeekTo(controllerTime);

    // update energy and its ui
    float health = MetaCore::Stats::GetHealth();
    energyCounter->_energy_k__BackingField = health;
    energyCounter->_nextFrameEnergyChange = 0;
    if (health > 0 && !energyPanel->_energyBar->enabled)
        ResetEnergyBar();
    if (!energyCounter->gameEnergyDidChangeEvent->Equals(nullptr))
        energyCounter->gameEnergyDidChangeEvent->Invoke(health);

    // update score multipliers
    auto multiplierCounter = scoreController->_scoreMultiplierCounter;
    multiplierCounter->_multiplier = multiplier;
    multiplierCounter->_multiplierIncreaseProgress = multiplierProgress;
    multiplierCounter->_multiplierIncreaseMaxProgress = multiplier * 2;

    auto maxMultiplierCounter = scoreController->_maxScoreMultiplierCounter;
    maxMultiplierCounter->_multiplier = GetMaxMultiplier();
    maxMultiplierCounter->_multiplierIncreaseProgress = GetMaxMultiplierProgress();
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
    comboController->_combo = MetaCore::Stats::GetCombo(MetaCore::Stats::BothSabers);
    comboController->_maxCombo = maxCombo;
    if (!comboController->comboDidChangeEvent->Equals(nullptr))
        comboController->comboDidChangeEvent->Invoke(comboController->_combo);
    // does the combo ui panel need to be updated more? (for its animated full combo lines)
}

void Pause::SetTime(float value) {
    if (!Manager::Replaying() || MetaCore::Stats::GetSongTime() == value)
        return;

    LazyInit();

    auto& replay = Manager::GetCurrentReplay();

    // replace with MetaCore internal state when added
    int multiplier;
    int multiplierProgress;
    int maxCombo;
    if (replay.events)
        CalculateEventChanges(*replay.events, value, multiplier, multiplierProgress, maxCombo);
    else if (replay.frames)
        CalculateFrameChanges(*replay.frames, value, multiplier, multiplierProgress, maxCombo);

    Playback::SeekTo(value);

    MetaCore::Internals::songTime() = value;

    // would be extra cool if I could do lighting, but man that seems annoying, especially backwards
    UpdateBaseGameState(multiplier, multiplierProgress, maxCombo);

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
