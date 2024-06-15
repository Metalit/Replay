#include "PauseMenu.hpp"

#include "Config.hpp"
#include "CustomTypes/Grabbable.hpp"
#include "CustomTypes/ScoringElement.hpp"
#include "Formats/EventReplay.hpp"
#include "Formats/FrameReplay.hpp"
#include "GlobalNamespace/AudioManagerSO.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/CallbacksInTime.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/GameEnergyUIPanel.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/IBeatmapObjectController.hpp"
#include "GlobalNamespace/IGameEnergyCounter.hpp"
#include "GlobalNamespace/ListExtensions.hpp"
#include "GlobalNamespace/MemoryPoolContainer_1.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/NoteCutSoundEffect.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/SliderController.hpp"
#include "Main.hpp"
#include "ReplayManager.hpp"
#include "System/Action.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Single.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Playables/PlayableDirector.hpp"
#include "UnityEngine/PrimitiveType.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Shader.hpp"
#include "UnityEngine/Time.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "bsml/shared/BSML.hpp"

using namespace GlobalNamespace;
using namespace Manager::Objects;

void InsertIntoSortedListFromEnd(ListW<ScoringElement*> sortedList, ScoringElement* newItem) {
    static auto* ___internal__method = il2cpp_utils::FindMethodUnsafe("", "ListExtensions", "InsertIntoSortedListFromEnd", 2);
    static auto* ___generic__method =
        THROW_UNLESS(il2cpp_utils::MakeGenericMethod(___internal__method, std::vector<Il2CppClass*>{classof(ScoringElement*)}));
    il2cpp_utils::RunMethodRethrow<void, false>(nullptr, ___generic__method, sortedList, newItem);
}

void DespawnObject(BeatmapObjectManager* manager, IBeatmapObjectController* object) {
    object->Pause(false);
    if (il2cpp_utils::try_cast<NoteController>(object).has_value())
        manager->Despawn((NoteController*) object);
    else if (il2cpp_utils::try_cast<ObstacleController>(object).has_value())
        manager->Despawn((ObstacleController*) object);
    else if (il2cpp_utils::try_cast<SliderController>(object).has_value())
        manager->Despawn((SliderController*) object);
}

BSML::SliderSetting* TextlessSlider(
    BSML::Lite::TransformWrapper parent, float min, float max, float current, float increment, auto onChange, auto format, bool showButtons = false
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

void SetTransform(auto obj, UnityEngine::Vector2 anchoredPos, UnityEngine::Vector2 sizeDelta) {
    auto transform = obj->template GetComponent<UnityEngine::RectTransform*>();
    transform->anchoredPosition = anchoredPos;
    transform->sizeDelta = sizeDelta;
}

void SetCameraModelToThirdPerson(UnityEngine::GameObject* cameraModel) {
    auto trans = cameraModel->transform;
    Quaternion rot = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
    // model points upwards
    Quaternion offset = Quaternion::AngleAxis(90, {1, 0, 0});
    rot = rot * offset;
    auto pos = getConfig().ThirdPerPos.GetValue();
    trans->SetPositionAndRotation(pos, rot);
}

void SetThirdPersonToCameraModel(UnityEngine::GameObject* cameraModel) {
    auto trans = cameraModel->transform;
    Quaternion rot = trans->rotation;
    // model points upwards
    auto offset = Quaternion::AngleAxis(-90, {1, 0, 0});
    rot = rot * offset;
    getConfig().ThirdPerRot.SetValue(rot.eulerAngles);
    auto pos = trans->position;
    getConfig().ThirdPerPos.SetValue(pos);
}

UnityEngine::GameObject*
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

UnityEngine::GameObject* CreateCameraModel() {
    static ConstString searchName("Custom/SimpleLit");
    auto shader = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::Shader*>()->First([](auto s) { return s->name == searchName; });
    auto mat = UnityEngine::Material::New_ctor(shader);
    mat->set_color(UnityEngine::Color::get_white());
    auto ret = CreateCube(nullptr, mat, {}, {}, {0.075, 0.075, 0.075}, "ReplayCameraModel");
    ret->AddComponent<ReplayHelpers::Grabbable*>()->onRelease = SetThirdPersonToCameraModel;
    CreateCube(ret, mat, {-1.461, 1.08, 1.08}, {45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 0");
    CreateCube(ret, mat, {1.461, 1.08, 1.08}, {45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 1");
    CreateCube(ret, mat, {1.461, 1.08, -1.08}, {-45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 2");
    CreateCube(ret, mat, {-1.461, 1.08, -1.08}, {-45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 3");
    CreateCube(ret, mat, {0, 2.08, 0}, {}, {5.845, 0.07, 4.322}, "Camera Screen");
    return ret;
}

namespace Pause {
    UnityEngine::GameObject* parent;
    BSML::SliderSetting* timeSlider;
    BSML::SliderSetting* speedSlider;
    PauseMenuManager* lastPauseMenu = nullptr;
    UnityEngine::GameObject* camera;

    bool touchedTime = false, touchedSpeed = false;

    void EnsureSetup(PauseMenuManager* pauseMenu) {
        static ConstString menuName("ReplayPauseMenu");
        if (lastPauseMenu != pauseMenu && !pauseMenu->_pauseContainerTransform->Find(menuName)) {
            LOG_INFO("Creating pause UI");

            parent = BSML::Lite::CreateCanvas();
            parent->AddComponent<HMUI::Screen*>();
            parent->name = menuName;
            parent->transform->SetParent(pauseMenu->_restartButton->transform->parent, false);
            parent->transform->localScale = Vector3::one();
            SetTransform(parent, {0, -15}, {110, 25});

            float time = Manager::GetSongTime();
            float startTime = scoreController->_audioTimeSyncController->_startSongTime;
            float endTime = scoreController->_audioTimeSyncController->songLength;
            auto& info = Manager::GetCurrentInfo();
            if (info.practice)
                startTime = info.startTime;
            if (info.failed)
                endTime = info.failTime;
            timeSlider = TextlessSlider(parent, startTime, endTime, time, 1, PreviewTime, [](float time) { return SecondsToString(time); });
            SetTransform(timeSlider, {0, 6}, {100, 10});
            speedSlider = TextlessSlider(
                parent, 0.5, 2, 1, 0.1, [](float _) { touchedSpeed = true; }, [](float speed) { return fmt::format("{:.1f}x", speed); }, true
            );
            SetTransform(speedSlider, {-18, -4}, {65, 10});
            auto dropdown = AddConfigValueDropdownEnum(parent, getConfig().CamMode, cameraModes)->transform->parent;
            dropdown->Find("Label")->GetComponent<TMPro::TextMeshProUGUI*>()->text = "";
            SetTransform(dropdown, {-10, -11.5}, {10, 10});
        }
        camera = UnityEngine::GameObject::Find("ReplayCameraModel");
        if (Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson) {
            if (!camera)
                camera = CreateCameraModel();
            camera->active = true;
            SetCameraModelToThirdPerson(camera);
        } else if (camera)
            camera->active = false;

        timeSlider->set_Value(scoreController->_audioTimeSyncController->songTime);
        speedSlider->set_Value(scoreController->_audioTimeSyncController->timeScale);
        touchedTime = false;
        touchedSpeed = false;
        lastPauseMenu = pauseMenu;
    }

    void OnUnpause() {
        // sliders are like really imprecise for some reason
        if (touchedTime)
            SetTime(timeSlider->get_Value());
        if (touchedSpeed)
            SetSpeed(speedSlider->get_Value());
        if (camera)
            camera->active = false;
    }

    void UpdateInReplay() {
        parent->SetActive(Manager::replaying);
    }

    void SetSpeed(float speed) {
        auto audio = scoreController->_audioTimeSyncController;
        // audio pitch can't be adjusted past these for some reason
        if (speed > 2)
            speed = 2;
        if (speed < 0.5)
            speed = 0.5;
        audio->_timeScale = speed;
        audio->_audioSource->pitch = speed;
        audioManager->musicPitch = 1 / speed;
        audio->_audioStartTimeOffsetSinceStart =
            (UnityEngine::Time::get_timeSinceLevelLoad() * speed) - (audio->songTime + audio->_initData->songTimeOffset);
    }

    void ResetEnergyBar() {
        if (!energyBar->get_isActiveAndEnabled())
            return;
        static auto RebindPlayableGraphOutputs = il2cpp_utils::resolve_icall<void, UnityEngine::Playables::PlayableDirector*>(
            "UnityEngine.Playables.PlayableDirector::RebindPlayableGraphOutputs"
        );
        energyBar->_playableDirector->Stop();
        RebindPlayableGraphOutputs(energyBar->_playableDirector);
        energyBar->_playableDirector->Evaluate();
        energyBar->_energyBar->enabled = true;
        static auto opacityColor = UnityEngine::Color(1, 1, 1, 0.3);
        auto emptyIcon = energyBar->transform->Find("EnergyIconEmpty");
        emptyIcon->localPosition = {-59, 0, 0};
        emptyIcon->GetComponent<HMUI::ImageView*>()->color = opacityColor;
        auto fullIcon = energyBar->transform->Find("EnergyIconFull");
        fullIcon->localPosition = {59, 0, 0};
        fullIcon->GetComponent<HMUI::ImageView*>()->color = opacityColor;
        energyBar->transform->Find("Laser")->get_gameObject()->active = false;
        // prevent recreation of battery energy UI
        auto energyType = gameEnergyCounter->energyType;
        gameEnergyCounter->energyType = -1;
        // make sure event is registered
        energyBar->Init();
        gameEnergyCounter->energyType = energyType;
    }

    void PreviewTime(float time) {
        static auto handleCut = il2cpp_utils::FindMethodUnsafe(classof(ComboController*), "HandleNoteWasCut", 2);

        touchedTime = true;
        auto values = MapAtTime(Manager::currentReplay, time);
        float modifierMult = ModifierMultiplier(Manager::currentReplay, values.energy == 0);
        gameEnergyCounter->energy = values.energy;
        gameEnergyCounter->ProcessEnergyChange(0);
        if (values.energy > 0 && !energyBar->_energyBar->get_enabled())
            ResetEnergyBar();
        if (energyBar->isActiveAndEnabled)
            energyBar->RefreshEnergyUI(values.energy);
        scoreController->_multipliedScore = values.score;
        scoreController->_modifiedScore = values.score * modifierMult;
        scoreController->_immediateMaxPossibleMultipliedScore = values.maxScore;
        scoreController->_immediateMaxPossibleModifiedScore = values.maxScore * modifierMult;
        if (values.maxScore == 0)
            scoreController->_immediateMaxPossibleModifiedScore = 1;
        scoreController->scoreDidChangeEvent->Invoke(values.score, values.score * modifierMult);
        comboController->_combo = values.combo - 1;
        comboController->_maxCombo = std::max(comboController->maxCombo, values.combo) - 1;
        // crashes on trying to invoke the event myself (with no hud) idk why
        NoteCutInfo goodCut{};
        goodCut.speedOK = goodCut.directionOK = goodCut.saberTypeOK = true;
        goodCut.wasCutTooSoon = false;
        il2cpp_utils::RunMethod<void, false>(comboController, handleCut, nullptr, byref(goodCut));
    }

    void DespawnObjects() {
        ListW<NoteCutSoundEffect*> sounds = noteSoundManager->_noteCutSoundEffectPoolContainer->activeItems;
        for (auto sound : sounds)
            sound->StopPlayingAndFinish();
        noteSoundManager->_prevNoteATime = -1;
        noteSoundManager->_prevNoteBTime = -1;
        ListW<IBeatmapObjectController*> objects = beatmapObjectManager->_allBeatmapObjects;
        std::vector<IBeatmapObjectController*> copy = {objects.begin(), objects.end()};
        for (auto object : copy)
            DespawnObject(beatmapObjectManager, object);
    }

    void ResetControllers() {
        scoreController->_modifiedScore = 0;
        scoreController->_multipliedScore = 0;
        scoreController->_immediateMaxPossibleModifiedScore = 1;
        scoreController->_immediateMaxPossibleMultipliedScore = 0;
        scoreController->_scoreMultiplierCounter->Reset();
        scoreController->_maxScoreMultiplierCounter->Reset();
        gameEnergyCounter->_didReach0Energy = false;
        gameEnergyCounter->energy = 0.5;
        if (!energyBar->_energyBar->enabled)
            ResetEnergyBar();
        comboController->_combo = 0;
        comboController->_maxCombo = 0;
        callbackController->_prevSongTime = System::Single::MinValue;
        // clear all running callbacks or something like that
        auto callbacksList = callbackController->_callbacksInTimes->_entries;
        int count = callbackController->_callbacksInTimes->Count;
        for (int i = 0; i < count && i < callbacksList.size(); i++) {
            auto& entry = callbacksList[i];
            if (entry.hashCode >= 0)
                entry.value->lastProcessedNode = nullptr;
        }
        // don't despawn elements as they might still be event recievers, so just make them do nothing to the score instead
        for (auto element : ListW<ScoringElement*>(scoreController->_sortedScoringElementsWithoutMultiplier))
            scoreController->_scoringElementsWithMultiplier->Add(element);
        scoreController->_sortedScoringElementsWithoutMultiplier->Clear();
        for (auto element : ListW<ScoringElement*>(scoreController->_scoringElementsWithMultiplier))
            element->SetMultipliers(0, 0);
    }

    void UpdateScoreController() {
        // literally the exact implementation of ScoreController::LateUpdate
        auto& self = scoreController;
        // Get the time of the next note that should be scored, or a large value if there are no more notes.
        float num = self->_sortedNoteTimesWithoutScoringElements->Count > 0 ? self->_sortedNoteTimesWithoutScoringElements->get_Item(0) : 3000000000;
        // Get the current game time, plus a small offset.
        float num2 = self->_audioTimeSyncController->songTime + 0.15;
        // A counter for the number of scoring elements that have been processed.
        int num3 = 0;
        // A flag indicating whether the score multiplier has changed.
        bool flag = false;
        for (auto item : ListW<ScoringElement*>(self->_sortedScoringElementsWithoutMultiplier)) {
            // If the element's time is outside of the range of the current game time and the next note's time,
            // process the element's multiplier event and add it to the list of scoring elements with multipliers.
            if (item->time < num2 || item->time > num) {
                // Process the multiplier event for the element.
                flag |= self->_scoreMultiplierCounter->ProcessMultiplierEvent(item->multiplierEventType);
                // If the element would have given a positive multiplier, also process it for the maximum score multiplier counter.
                if (item->get_wouldBeCorrectCutBestPossibleMultiplierEventType() == ScoreMultiplierCounter::MultiplierEventType::Positive)
                    self->_maxScoreMultiplierCounter->ProcessMultiplierEvent(ScoreMultiplierCounter::MultiplierEventType::Positive);
                item->SetMultipliers(self->_scoreMultiplierCounter->multiplier, self->_maxScoreMultiplierCounter->multiplier);
                self->_scoringElementsWithMultiplier->Add(item);
                // Increment the counter for processed elements.
                num3++;
                // Skip to the next element.
                continue;
            }
            // If the element's time is within the range of the current game time and the next note's time,
            // stop processing elements.
            break;
        }
        // Remove the processed elements from the list of scoring elements without multipliers.
        self->_sortedScoringElementsWithoutMultiplier->RemoveRange(0, num3);
        // If the score multiplier has changed, invoke the appropriate event.
        if (flag)
            self->multiplierDidChangeEvent->Invoke(self->_scoreMultiplierCounter->multiplier, self->_scoreMultiplierCounter->normalizedProgress);
        // A flag indicating whether the score has changed.
        bool flag2 = false;
        self->_scoringElementsToRemove->Clear();
        for (auto item2 : ListW<ScoringElement*>(self->_scoringElementsWithMultiplier)) {
            // If the element is finished, add it to the list of elements to remove and add its score
            // to the current multiplied and modified scores.
            if (item2->isFinished) {
                // If the element has a positive maximum possible cut score, set the flag indicating that
                // the score has changed and add the element's score to the current multiplied and modified scores.
                if (item2->maxPossibleCutScore > 0) {
                    flag2 = true;
                    self->_multipliedScore += item2->cutScore * item2->multiplier;
                    self->_immediateMaxPossibleMultipliedScore += item2->maxPossibleCutScore * item2->maxMultiplier;
                }
                self->_scoringElementsToRemove->Add(item2);
                if (self->scoringForNoteFinishedEvent)
                    self->scoringForNoteFinishedEvent->Invoke(item2);
            }
        }
        for (auto item3 : ListW<ScoringElement*>(self->_scoringElementsToRemove)) {
            self->DespawnScoringElement(item3);
            self->_scoringElementsWithMultiplier->Remove(item3);
        }
        self->_scoringElementsToRemove->Clear();
        float totalMultiplier = self->_gameplayModifiersModel->GetTotalMultiplier(self->_gameplayModifierParams, self->_gameEnergyCounter->energy);
        // If the total multiplier has changed, set the flag indicating that the score has changed.
        if (self->_prevMultiplierFromModifiers != totalMultiplier) {
            self->_prevMultiplierFromModifiers = totalMultiplier;
            flag2 = true;
        }
        // If the score has changed, update the current multiplied and modified scores and invoke the appropriate event.
        if (flag2) {
            self->_modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->multipliedScore, totalMultiplier);
            self->_immediateMaxPossibleModifiedScore =
                ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->immediateMaxPossibleMultipliedScore, totalMultiplier);
            if (self->scoreDidChangeEvent)
                self->scoreDidChangeEvent->Invoke(self->multipliedScore, self->modifiedScore);
        }
    }

    void SetTime(float time) {
        static auto handleCut = il2cpp_utils::FindMethodUnsafe(classof(ComboController*), "HandleNoteWasCut", 2);

        float startTime = scoreController->_audioTimeSyncController->_startSongTime;
        float endTime = scoreController->_audioTimeSyncController->songLength;
        auto& info = Manager::GetCurrentInfo();
        if (info.practice)
            startTime = info.startTime;
        if (info.failed)
            endTime = info.failTime;
        if (time < startTime)
            time = startTime;
        if (time > endTime)
            time = endTime;
        LOG_INFO("Time set to {}", time);
        DespawnObjects();
        ResetControllers();
        callbackController->_startFilterTime = time;
        Manager::ReplayRestarted(false);
        Manager::UpdateTime(time);
        float controllerTime =
            (time - scoreController->_audioTimeSyncController->_startSongTime) / scoreController->_audioTimeSyncController->timeScale;
        scoreController->_audioTimeSyncController->SeekTo(controllerTime);
        auto& replay = Manager::currentReplay;
        if (replay.type & ReplayType::Event) {
            // reset energy as we will override it
            Manager::Events::wallEnergyLoss = 0;
            auto eventReplay = dynamic_cast<EventReplay*>(replay.replay.get());
            float lastCalculatedWall = 0, wallEnd = 0;
            // simulate all events
            for (auto& event : eventReplay->events) {
                if (event.time > time)
                    break;
                if (gameEnergyCounter->_didReach0Energy && !eventReplay->info.modifiers.noFail)
                    break;
                // add wall energy change since last event
                // needs to be done before the new event is processed in case it is a new wall
                if (lastCalculatedWall != wallEnd) {
                    if (event.time < wallEnd) {
                        gameEnergyCounter->ProcessEnergyChange(1.3 * (lastCalculatedWall - event.time));
                        lastCalculatedWall = event.time;
                    } else {
                        gameEnergyCounter->ProcessEnergyChange(1.3 * (lastCalculatedWall - wallEnd));
                        lastCalculatedWall = wallEnd;
                    }
                }
                switch (event.eventType) {
                    case EventRef::Note: {
                        Manager::SetLastCutTime(event.time);
                        auto& noteEvent = eventReplay->notes[event.index];
                        auto scoringElement = MakeFakeScoringElement(noteEvent);
                        InsertIntoSortedListFromEnd(scoreController->_sortedScoringElementsWithoutMultiplier, scoringElement);
                        UpdateScoreController();
                        auto noteCutInfo = GetNoteCutInfo(nullptr, nullptr, noteEvent.noteCutInfo);
                        il2cpp_utils::RunMethod<void, false>(comboController, handleCut, nullptr, byref(noteCutInfo));
                        gameEnergyCounter->ProcessEnergyChange(EnergyForNote(noteEvent.info));
                        break;
                    }
                    case EventRef::Wall:
                        // combo doesn't get set back to 0 if you enter a wall while inside of one
                        if (event.time > wallEnd)
                            scoreController->_playerHeadAndObstacleInteraction->headDidEnterObstaclesEvent->Invoke();
                        // step through wall energy loss instead of doing it all at once
                        lastCalculatedWall = event.time;
                        wallEnd = std::max(wallEnd, eventReplay->walls[event.index].endTime);
                        break;
                    default:
                        break;
                }
            }
        }
        if (replay.type & ReplayType::Frame) {
            // let hooks update values
            gameEnergyCounter->ProcessEnergyChange(0);
            if (Manager::Frames::AllowScoreOverride()) {
                auto frame = Manager::Frames::GetScoreFrame();
                // might be redundant but honestly I don't really want to think about it
                if (frame->percent > 0) {
                    int maxScore = (int) (frame->score / frame->percent);
                    float multiplier =
                        scoreController->_gameplayModifiersModel->GetTotalMultiplier(scoreController->_gameplayModifierParams, frame->energy);
                    int modifiedMaxScore = maxScore * multiplier;
                    if (maxScore == 0)
                        modifiedMaxScore = 1;
                    scoreController->_immediateMaxPossibleMultipliedScore = maxScore;
                    scoreController->_immediateMaxPossibleModifiedScore = modifiedMaxScore;
                }
                scoreController->LateUpdate();
            }
            NoteCutInfo info{};
            il2cpp_utils::RunMethod<void, false>(comboController, handleCut, nullptr, byref(info));
        }
    }
}
