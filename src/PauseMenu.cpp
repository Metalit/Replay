#include "Main.hpp"
#include "PauseMenu.hpp"
#include "Config.hpp"

#include "ReplayManager.hpp"
#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "CustomTypes/ScoringElement.hpp"
#include "CustomTypes/Grabbable.hpp"

#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/AudioTimeSyncController_InitData.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/ListExtensions.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/MemoryPoolContainer_1.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/CallbacksInTime.hpp"
#include "GlobalNamespace/SliderController.hpp"
#include "GlobalNamespace/AudioManagerSO.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/GameEnergyUIPanel.hpp"

#include "UnityEngine/Time.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Playables/PlayableDirector.hpp"

#include "System/Action.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Single.hpp"

#include "questui/shared/BeatSaberUI.hpp"

using namespace GlobalNamespace;
using namespace QuestUI;
using namespace Manager::Objects;

void InsertIntoSortedListFromEnd(List<ScoringElement*>* sortedList, ScoringElement* newItem) {
    static auto* ___internal__method = il2cpp_utils::FindMethodUnsafe("", "ListExtensions", "InsertIntoSortedListFromEnd", 2);
    static auto* ___generic__method = THROW_UNLESS(il2cpp_utils::MakeGenericMethod(___internal__method, std::vector<Il2CppClass*>{classof(ScoringElement*)}));
    il2cpp_utils::RunMethodRethrow<void, false>(nullptr, ___generic__method, sortedList, newItem);
}

void DespawnObject(BeatmapObjectManager* manager, IBeatmapObjectController* object) {
    object->Pause(false);
    if(il2cpp_utils::try_cast<NoteController>(object).has_value())
        manager->Despawn((NoteController*) object);
    else if(il2cpp_utils::try_cast<ObstacleController>(object).has_value())
        manager->Despawn((ObstacleController*) object);
    else if(il2cpp_utils::try_cast<SliderController>(object).has_value())
        manager->Despawn((SliderController*) object);
}

SliderSetting* TextlessSlider(auto parent, float min, float max, float current, float increment, auto onChange, auto format) {
    auto madeSlider = BeatSaberUI::CreateSliderSetting(parent, "", 0, 0, 0, 0);
    SliderSetting* slider = madeSlider->slider->get_gameObject()->template AddComponent<SliderSetting*>();
    slider->slider = madeSlider->slider;
    slider->FormatString = std::move(format);
    slider->Setup(min, max, current, increment, 0, std::move(onChange));
    auto transform = (UnityEngine::RectTransform*) slider->slider->get_transform();
    auto object = transform->GetParent()->get_gameObject();
    transform->SetParent(parent->get_transform(), false);
    transform->set_anchorMin({0.5, 0.5});
    transform->set_anchorMax({0.5, 0.5});
    transform->set_pivot({0.5, 0.5});
    UnityEngine::Object::Destroy(object);
    return slider;
}

void SetTransform(auto obj, UnityEngine::Vector2 anchoredPos, UnityEngine::Vector2 sizeDelta) {
    auto transform = (UnityEngine::RectTransform*) obj->get_transform();
    transform->set_anchoredPosition(anchoredPos);
    transform->set_sizeDelta(sizeDelta);
}

void AddIncrement(SliderSetting* slider, float increment) {
    auto rect = (UnityEngine::RectTransform*) slider->slider->get_transform();
    // not sure how general purpose this really is
    auto width = rect->get_rect().get_width() / 2;
    auto delta = rect->get_sizeDelta();
    rect->set_sizeDelta({delta.x - 16, delta.y});
    BeatSaberUI::CreateUIButton(rect, "", "DecButton", {4 - width, 0}, {6, 10}, [slider, increment] {
        float newValue = slider->slider->get_value() - increment;
        slider->slider->set_value(newValue);
        // get_value to let it handle min/max
        slider->OnChange(slider->slider, slider->slider->get_value());
    });
    BeatSaberUI::CreateUIButton(rect, "", "IncButton", {-6 + width, 0}, {10, 10}, [slider, increment] {
        float newValue = slider->slider->get_value() + increment;
        slider->slider->set_value(newValue);
        // get_value to let it handle min/max
        slider->OnChange(slider->slider, slider->slider->get_value());
    });
}

void SetCameraModelToThirdPerson(UnityEngine::GameObject* cameraModel) {
    auto trans = cameraModel->get_transform();
    auto rot = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
    // model points upwards
    auto offset = Quaternion::AngleAxis(90, {1, 0, 0});
    rot = Sombrero::QuaternionMultiply(rot, offset);
    auto pos = getConfig().ThirdPerPos.GetValue();
    trans->SetPositionAndRotation(pos, rot);
}

void SetThirdPersonToCameraModel(UnityEngine::GameObject* cameraModel) {
    auto trans = cameraModel->get_transform();
    auto rot = trans->get_rotation();
    // model points upwards
    auto offset = Quaternion::AngleAxis(-90, {1, 0, 0});
    rot = Sombrero::QuaternionMultiply(rot, offset);
    getConfig().ThirdPerRot.SetValue(rot.get_eulerAngles());
    auto pos = trans->get_position();
    getConfig().ThirdPerPos.SetValue(pos);
}

UnityEngine::GameObject* CreateCube(UnityEngine::GameObject* parent, UnityEngine::Material* mat, Vector3 pos, Vector3 euler, Vector3 scale, std::string_view name) {
    auto cube = UnityEngine::GameObject::CreatePrimitive(UnityEngine::PrimitiveType::Cube);
    cube->GetComponent<UnityEngine::Renderer*>()->SetMaterial(mat);
    auto trans = cube->get_transform();
    if(parent)
        trans->SetParent(parent->get_transform(), false);
    trans->set_localPosition(pos);
    trans->set_localEulerAngles(euler);
    trans->set_localScale(scale);
    cube->set_name(name);
    return cube;
}

UnityEngine::GameObject* CreateCameraModel() {
    auto mat = UnityEngine::Resources::FindObjectsOfTypeAll<UnityEngine::Material*>().First([](auto m) { return m->get_name() == "Custom/SimpleLit (Instance)"; });
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
    SliderSetting* timeSlider;
    SliderSetting* speedSlider;
    PauseMenuManager* lastPauseMenu = nullptr;
    UnityEngine::GameObject* camera;

    bool touchedTime = false, touchedSpeed = false;

    void EnsureSetup(PauseMenuManager* pauseMenu) {
        static ConstString menuName("ReplayPauseMenu");
        if(lastPauseMenu != pauseMenu && !pauseMenu->pauseContainerTransform->Find(menuName)) {
            LOG_INFO("Creating pause UI");

            parent = BeatSaberUI::CreateCanvas();
            parent->AddComponent<HMUI::Screen*>();
            parent->set_name(menuName);
            parent->get_transform()->SetParent(pauseMenu->restartButton->get_transform()->GetParent(), false);
            parent->get_transform()->set_localScale(Vector3::one());
            SetTransform(parent, {0, -15}, {110, 25});

            float time = Manager::GetSongTime();
            float startTime = scoreController->audioTimeSyncController->startSongTime;
            float endTime = scoreController->audioTimeSyncController->get_songLength();
            auto& info = Manager::GetCurrentInfo();
            if(info.practice)
                startTime = info.startTime;
            if(info.failed)
                endTime = info.failTime;
            timeSlider = TextlessSlider(parent, startTime, endTime, time, 1, PreviewTime, [](float time) { return SecondsToString(time); });
            SetTransform(timeSlider, {0, 6}, {100, 10});
            speedSlider = TextlessSlider(parent, 0.5, 2, 1, 0.1, [](float _) { touchedSpeed = true; }, [](float speed) { return string_format("%.1fx", speed); });
            SetTransform(speedSlider, {-18, -4}, {65, 10});
            AddIncrement(speedSlider, 0.1);
            auto dropdown = AddConfigValueDropdownEnum(parent, getConfig().CamMode, cameraModes)->get_transform()->GetParent();
            dropdown->Find("Label")->GetComponent<TMPro::TextMeshProUGUI*>()->SetText("");
            SetTransform(dropdown, {-10, -11.5}, {10, 10});
        }
        camera = UnityEngine::GameObject::Find("ReplayCameraModel");
        if(Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson) {
            if(!camera)
                camera = CreateCameraModel();
            camera->set_active(true);
            SetCameraModelToThirdPerson(camera);
        } else if(camera)
            camera->set_active(false);

        timeSlider->set_value(scoreController->audioTimeSyncController->songTime);
        speedSlider->set_value(scoreController->audioTimeSyncController->timeScale);
        touchedTime = false;
        touchedSpeed = false;
        lastPauseMenu = pauseMenu;
    }

    void OnUnpause() {
        // sliders are like really imprecise for some reason
        if(touchedTime)
            SetTime(timeSlider->get_value());
        if(touchedSpeed)
            SetSpeed(speedSlider->get_value());
        if(camera)
            camera->set_active(false);
    }

    void UpdateInReplay() {
        parent->SetActive(Manager::replaying);
    }

    void SetSpeed(float speed) {
        auto audio = scoreController->audioTimeSyncController;
        // audio pitch can't be adjusted past these for some reason
        if(speed > 2)
            speed = 2;
        if(speed < 0.5)
            speed = 0.5;
        audio->timeScale = speed;
        audio->audioSource->set_pitch(speed);
        audioManager->set_musicPitch(1 / speed);
        audio->audioStartTimeOffsetSinceStart = (UnityEngine::Time::get_timeSinceLevelLoad() * speed) - (audio->songTime + audio->initData->songTimeOffset);
    }

    void ResetEnergyBar() {
        if(!energyBar->get_isActiveAndEnabled())
            return;
        static auto RebindPlayableGraphOutputs = il2cpp_utils::resolve_icall<void, UnityEngine::Playables::PlayableDirector*>
            ("UnityEngine.Playables.PlayableDirector::RebindPlayableGraphOutputs");
        energyBar->playableDirector->Stop();
        RebindPlayableGraphOutputs(energyBar->playableDirector);
        energyBar->playableDirector->Evaluate();
        energyBar->energyBar->set_enabled(true);
        static auto opacityColor = UnityEngine::Color(1, 1, 1, 0.3);
        auto emptyIcon = energyBar->get_transform()->Find("EnergyIconEmpty");
        emptyIcon->set_localPosition({-59, 0, 0});
        emptyIcon->GetComponent<HMUI::ImageView*>()->set_color(opacityColor);
        auto fullIcon = energyBar->get_transform()->Find("EnergyIconFull");
        fullIcon->set_localPosition({59, 0, 0});
        fullIcon->GetComponent<HMUI::ImageView*>()->set_color(opacityColor);
        energyBar->get_transform()->Find("Laser")->get_gameObject()->SetActive(false);
        // prevent recreation of battery energy UI
        auto energyType = gameEnergyCounter->energyType;
        gameEnergyCounter->energyType = -1;
        // make sure event is registered
        energyBar->Init();
        gameEnergyCounter->energyType = energyType;
    }

    void PreviewTime(float time) {
        touchedTime = true;
        auto values = MapAtTime(Manager::currentReplay, time);
        float modifierMult = ModifierMultiplier(Manager::currentReplay, values.energy == 0);
        gameEnergyCounter->energy = values.energy;
        gameEnergyCounter->ProcessEnergyChange(0);
        if(values.energy > 0 && !energyBar->energyBar->get_enabled())
            ResetEnergyBar();
        if(energyBar->get_isActiveAndEnabled())
            energyBar->RefreshEnergyUI(values.energy);
        scoreController->multipliedScore = values.score;
        scoreController->modifiedScore = values.score * modifierMult;
        scoreController->immediateMaxPossibleMultipliedScore = values.maxScore;
        scoreController->immediateMaxPossibleModifiedScore = values.maxScore * modifierMult;
        if(values.maxScore == 0)
            scoreController->immediateMaxPossibleModifiedScore = 1;
        scoreController->scoreDidChangeEvent->Invoke(values.score, values.score * modifierMult);
        comboController->combo = values.combo - 1;
        comboController->maxCombo = std::max(comboController->maxCombo, values.combo) - 1;
        // crashes on trying to invoke the event myself (with no hud) idk why
        NoteCutInfo goodCut{}; goodCut.speedOK = goodCut.directionOK = goodCut.saberTypeOK = true; goodCut.wasCutTooSoon = false;
        il2cpp_utils::RunMethodUnsafe(comboController, "HandleNoteWasCut", nullptr, byref(goodCut));
    }

    void DespawnObjects() {
        auto sounds = noteSoundManager->noteCutSoundEffectPoolContainer->get_activeItems();
        for(int i = 0; i < sounds->get_Count(); i++)
            sounds->get_Item(i)->StopPlayingAndFinish();
        noteSoundManager->prevNoteATime = -1;
        noteSoundManager->prevNoteBTime = -1;
        auto objects = beatmapObjectManager->allBeatmapObjects;
        for(int i = 0; i < objects->get_Count(); i++)
            DespawnObject(beatmapObjectManager, objects->get_Item(i));
    }

    void ResetControllers() {
        scoreController->modifiedScore = 0;
        scoreController->multipliedScore = 0;
        scoreController->immediateMaxPossibleModifiedScore = 1;
        scoreController->immediateMaxPossibleMultipliedScore = 0;
        scoreController->scoreMultiplierCounter->Reset();
        scoreController->maxScoreMultiplierCounter->Reset();
        gameEnergyCounter->didReach0Energy = false;
        gameEnergyCounter->energy = 0.5;
        if(!energyBar->energyBar->get_enabled())
            ResetEnergyBar();
        comboController->combo = 0;
        comboController->maxCombo = 0;
        callbackController->prevSongTime = System::Single::MinValue;
        // clear all running callbacks or something like that
        auto callbacksList = callbackController->callbacksInTimes->entries;
        int count = callbackController->callbacksInTimes->get_Count();
        for(int i = 0; i < count && i < callbacksList.Length(); i++) {
            auto& entry = callbacksList[i];
            if(entry.hashCode >= 0)
                entry.value->lastProcessedNode = nullptr;
        }
        // don't despawn elements as they might still be event recievers, so just make them do nothing to the score instead
        for(int i = 0; i < scoreController->sortedScoringElementsWithoutMultiplier->get_Count(); i++)
            scoreController->scoringElementsWithMultiplier->Add(scoreController->sortedScoringElementsWithoutMultiplier->get_Item(i));
        scoreController->sortedScoringElementsWithoutMultiplier->Clear();
        for(int i = 0; i < scoreController->scoringElementsWithMultiplier->get_Count(); i++)
            scoreController->scoringElementsWithMultiplier->get_Item(i)->SetMultipliers(0, 0);
    }

    void UpdateScoreController() {
        // literally the exact implementation of ScoreController::LateUpdate
        auto& self = scoreController;
        // Get the time of the next note that should be scored, or a large value if there are no more notes.
        float num = self->sortedNoteTimesWithoutScoringElements->get_Count() > 0 ? self->sortedNoteTimesWithoutScoringElements->get_Item(0) : 3000000000;
        // Get the current game time, plus a small offset.
        float num2 = self->audioTimeSyncController->get_songTime() + 0.15;
        // A counter for the number of scoring elements that have been processed.
        int num3 = 0;
        // A flag indicating whether the score multiplier has changed.
        bool flag = false;
        for(int i = 0; i < self->sortedScoringElementsWithoutMultiplier->get_Count(); i++) {
            auto item = self->sortedScoringElementsWithoutMultiplier->get_Item(i);
            // If the element's time is outside of the range of the current game time and the next note's time,
            // process the element's multiplier event and add it to the list of scoring elements with multipliers.
            if(item->get_time() < num2 || item->get_time() > num) {
                // Process the multiplier event for the element.
                flag |= self->scoreMultiplierCounter->ProcessMultiplierEvent(item->get_multiplierEventType());
                // If the element would have given a positive multiplier, also process it for the maximum score multiplier counter.
                if(item->get_wouldBeCorrectCutBestPossibleMultiplierEventType() == ScoreMultiplierCounter::MultiplierEventType::Positive)
                    self->maxScoreMultiplierCounter->ProcessMultiplierEvent(ScoreMultiplierCounter::MultiplierEventType::Positive);
                item->SetMultipliers(self->scoreMultiplierCounter->get_multiplier(), self->maxScoreMultiplierCounter->get_multiplier());
                self->scoringElementsWithMultiplier->Add(item);
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
        self->sortedScoringElementsWithoutMultiplier->RemoveRange(0, num3);
        // If the score multiplier has changed, invoke the appropriate event.
        if(flag)
            self->multiplierDidChangeEvent->Invoke(self->scoreMultiplierCounter->get_multiplier(), self->scoreMultiplierCounter->get_normalizedProgress());
        // A flag indicating whether the score has changed.
        bool flag2 = false;
        self->scoringElementsToRemove->Clear();
        for(int i = 0; i < self->scoringElementsWithMultiplier->get_Count(); i++) {
            auto item2 = self->scoringElementsWithMultiplier->get_Item(i);
            // If the element is finished, add it to the list of elements to remove and add its score
            // to the current multiplied and modified scores.
            if (item2->get_isFinished()) {
                // If the element has a positive maximum possible cut score, set the flag indicating that
                // the score has changed and add the element's score to the current multiplied and modified scores.
                if (item2->get_maxPossibleCutScore() > 0) {
                    flag2 = true;
                    self->multipliedScore += item2->get_cutScore() * item2->get_multiplier();
                    self->immediateMaxPossibleMultipliedScore += item2->get_maxPossibleCutScore() * item2->get_maxMultiplier();
                }
                self->scoringElementsToRemove->Add(item2);
                if(self->scoringForNoteFinishedEvent)
                    self->scoringForNoteFinishedEvent->Invoke(item2);
            }
        }
        for(int i = 0; i < self->scoringElementsToRemove->get_Count(); i++) {
            auto item3 = self->scoringElementsToRemove->get_Item(i);
            self->DespawnScoringElement(item3);
            self->scoringElementsWithMultiplier->Remove(item3);
        }
        self->scoringElementsToRemove->Clear();
        float totalMultiplier = self->gameplayModifiersModel->GetTotalMultiplier(self->gameplayModifierParams, self->gameEnergyCounter->get_energy());
        // If the total multiplier has changed, set the flag indicating that the score has changed.
        if (self->prevMultiplierFromModifiers != totalMultiplier) {
            self->prevMultiplierFromModifiers = totalMultiplier;
            flag2 = true;
        }
        // If the score has changed, update the current multiplied and modified scores and invoke the appropriate event.
        if (flag2) {
            self->modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->multipliedScore, totalMultiplier);
            self->immediateMaxPossibleModifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->immediateMaxPossibleMultipliedScore, totalMultiplier);
            if(self->scoreDidChangeEvent)
                self->scoreDidChangeEvent->Invoke(self->multipliedScore, self->modifiedScore);
        }
    }

    void SetTime(float time) {
        float startTime = scoreController->audioTimeSyncController->startSongTime;
        float endTime = scoreController->audioTimeSyncController->get_songLength();
        auto& info = Manager::GetCurrentInfo();
        if(info.practice)
            startTime = info.startTime;
        if(info.failed)
            endTime = info.failTime;
        if(time < startTime)
            time = startTime;
        if(time > endTime)
            time = endTime;
        LOG_INFO("Time set to {}", time);
        DespawnObjects();
        ResetControllers();
        callbackController->startFilterTime = time;
        Manager::ReplayRestarted(false);
        Manager::UpdateTime(time);
        float controllerTime = (time - scoreController->audioTimeSyncController->startSongTime) / scoreController->audioTimeSyncController->timeScale;
        scoreController->audioTimeSyncController->SeekTo(controllerTime);
        auto& replay = Manager::currentReplay;
        if(replay.type & ReplayType::Event) {
            // reset energy as we will override it
            Manager::Events::wallEnergyLoss = 0;
            auto eventReplay = dynamic_cast<EventReplay*>(replay.replay.get());
            float lastCalculatedWall = 0, wallEnd = 0;
            // simulate all events
            for(auto& event: eventReplay->events) {
                if(event.time > time)
                    break;
                if(gameEnergyCounter->didReach0Energy && !eventReplay->info.modifiers.noFail)
                    break;
                // add wall energy change since last event
                // needs to be done before the new event is processed in case it is a new wall
                if(lastCalculatedWall != wallEnd) {
                    if(event.time < wallEnd) {
                        gameEnergyCounter->ProcessEnergyChange(1.3 * (lastCalculatedWall - event.time));
                        lastCalculatedWall = event.time;
                    } else {
                        gameEnergyCounter->ProcessEnergyChange(1.3 * (lastCalculatedWall - wallEnd));
                        lastCalculatedWall = wallEnd;
                    }
                }
                switch(event.eventType) {
                case EventRef::Note: {
                    Manager::SetLastCutTime(event.time);
                    auto& noteEvent = eventReplay->notes[event.index];
                    auto scoringElement = MakeFakeScoringElement(noteEvent);
                    InsertIntoSortedListFromEnd(scoreController->sortedScoringElementsWithoutMultiplier, scoringElement);
                    UpdateScoreController();
                    auto noteCutInfo = GetNoteCutInfo(nullptr, nullptr, noteEvent.noteCutInfo);
                    il2cpp_utils::RunMethodUnsafe(comboController, "HandleNoteWasCut", nullptr, byref(noteCutInfo));
                    gameEnergyCounter->ProcessEnergyChange(EnergyForNote(noteEvent.info));
                    break;
                } case EventRef::Wall:
                    // combo doesn't get set back to 0 if you enter a wall while inside of one
                    if(event.time > wallEnd)
                        scoreController->playerHeadAndObstacleInteraction->headDidEnterObstaclesEvent->Invoke();
                    // step through wall energy loss instead of doing it all at once
                    lastCalculatedWall = event.time;
                    wallEnd = std::max(wallEnd, eventReplay->walls[event.index].endTime);
                    break;
                default:
                    break;
                }
            }
        }
        if(replay.type & ReplayType::Frame) {
            // let hooks update values
            gameEnergyCounter->ProcessEnergyChange(0);
            if(Manager::Frames::AllowScoreOverride()) {
                auto frame = Manager::Frames::GetScoreFrame();
                // might be redundant but honestly I don't really want to think about it
                if(frame->percent > 0) {
                    int maxScore = (int) (frame->score / frame->percent);
                    float multiplier = scoreController->gameplayModifiersModel->GetTotalMultiplier(scoreController->gameplayModifierParams, frame->energy);
                    int modifiedMaxScore = maxScore * multiplier;
                    if(maxScore == 0)
                        modifiedMaxScore = 1;
                    scoreController->immediateMaxPossibleMultipliedScore = maxScore;
                    scoreController->immediateMaxPossibleModifiedScore = modifiedMaxScore;
                }
                scoreController->LateUpdate();
            }
            NoteCutInfo info{}; il2cpp_utils::RunMethodUnsafe(comboController, "HandleNoteWasCut", nullptr, byref(info));
        }
    }
}
