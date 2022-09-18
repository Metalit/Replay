#include "Main.hpp"
#include "PauseMenu.hpp"

#include "ReplayManager.hpp"
#include "Formats/FrameReplay.hpp"
#include "Formats/EventReplay.hpp"

#include "CustomTypes/ScoringElement.hpp"

#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/GameEnergyCounter.hpp"
#include "GlobalNamespace/ComboController.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/PlayerHeadAndObstacleInteraction.hpp"
#include "GlobalNamespace/ListExtensions.hpp"
#include "GlobalNamespace/BeatmapObjectManager.hpp"
#include "GlobalNamespace/NoteCutSoundEffectManager.hpp"
#include "GlobalNamespace/MemoryPoolContainer_1.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/BeatmapObjectSpawnController.hpp"
#include "GlobalNamespace/BeatmapCallbacksController.hpp"
#include "GlobalNamespace/CallbacksInTime.hpp"
#include "GlobalNamespace/PrepareLevelCompletionResults.hpp"
#include "GlobalNamespace/SliderController.hpp"

#include "System/Action.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Single.hpp"

#include "questui/shared/BeatSaberUI.hpp"

using namespace GlobalNamespace;
using namespace QuestUI;

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

namespace Pause {
    UnityEngine::GameObject* parent;
    QuestUI::SliderSetting* slider;
    ScoreController* scoreController;
    ComboController* comboController;
    GameEnergyCounter* gameEnergyCounter;
    NoteCutSoundEffectManager* noteSoundManager;
    BeatmapObjectManager* beatmapObjectManager;
    BeatmapCallbacksController* callbackController;
    PauseMenuManager* lastPauseMenu = nullptr;

    void EnsureSetup(PauseMenuManager* pauseMenu) {
        static ConstString menuName("ReplayPauseMenu");
        if(lastPauseMenu != pauseMenu && !pauseMenu->pauseContainerTransform->Find(menuName)) {
            auto hasOtherObjects = UnityEngine::Resources::FindObjectsOfTypeAll<PrepareLevelCompletionResults*>().First();
            scoreController = (ScoreController*) hasOtherObjects->scoreController;
            comboController = hasOtherObjects->comboController;
            gameEnergyCounter = hasOtherObjects->gameEnergyCounter;
            noteSoundManager = UnityEngine::Resources::FindObjectsOfTypeAll<NoteCutSoundEffectManager*>().First();
            beatmapObjectManager = noteSoundManager->beatmapObjectManager;
            callbackController = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapObjectSpawnController*>().First()->beatmapCallbacksController;

            parent = BeatSaberUI::CreateCanvas();
            parent->set_name(menuName);
            parent->get_transform()->SetParent(pauseMenu->restartButton->get_transform()->GetParent(), false);
            parent->get_transform()->set_localScale(Vector3::one());
            auto rect = parent->GetComponent<UnityEngine::RectTransform*>();
            rect->set_anchoredPosition({0, -15});
            rect->set_sizeDelta({90, 10});
            
            auto madeSlider = BeatSaberUI::CreateSliderSetting(parent, "", 0, 0, 0, 0);
            slider = madeSlider->slider->get_gameObject()->AddComponent<SliderSetting*>();
            slider->slider = madeSlider->slider;
            float time = scoreController->audioTimeSyncController->songTime;
            float startTime = scoreController->audioTimeSyncController->startSongTime;
            float endTime = scoreController->audioTimeSyncController->get_songLength();
            slider->FormatString = [](float time) {
                return SecondsToString(time);
            };
            slider->Setup(startTime, int(endTime - 1), time, 1, 0.1, [](float time) {
                SetTime(time);
            });
            auto transform = (UnityEngine::RectTransform*) slider->slider->get_transform();
            auto object = transform->GetParent()->get_gameObject();
            transform->SetParent(parent->get_transform());
            transform->set_anchorMin({0, 0});
            transform->set_anchorMax({1, 1});
            transform->set_pivot({0.5, 0.5});
            transform->set_sizeDelta({0, 0});
            UnityEngine::Object::Destroy(object);
        }
        slider->set_value(scoreController->audioTimeSyncController->songTime);
        lastPauseMenu = pauseMenu;
    }

    void UpdateInReplay() {
        parent->SetActive(Manager::replaying);
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
        scoreController->immediateMaxPossibleModifiedScore = 0;
        scoreController->immediateMaxPossibleMultipliedScore = 0;
        scoreController->scoreMultiplierCounter->Reset();
        scoreController->maxScoreMultiplierCounter->Reset();
        gameEnergyCounter->didReach0Energy = false;
        gameEnergyCounter->energy = 0.5;
        comboController->combo = 0;
        comboController->maxCombo = 0;
        callbackController->prevSongTime = System::Single::MinValue;
        // clear all running callbacks or something like that
        auto callbacksList = callbackController->callbacksInTimes->entries;
        for(auto& entry : callbacksList)
            entry.value->lastProcessedNode = nullptr;
        // don't despawn elements as they might still be event recievers, so just make them do nothing to the score instead
        for(int i = 0; i < scoreController->sortedScoringElementsWithoutMultiplier->get_Count(); i++)
            scoreController->scoringElementsWithMultiplier->Add(scoreController->sortedScoringElementsWithoutMultiplier->get_Item(i));
        scoreController->sortedScoringElementsWithoutMultiplier->Clear();
        for(int i = 0; i < scoreController->scoringElementsWithMultiplier->get_Count(); i++)
            scoreController->scoringElementsWithMultiplier->get_Item(i)->SetMultipliers(0, 0);
    }

    void SetTime(float time) {
        LOG_INFO("Time set to {}", time);
        DespawnObjects();
        ResetControllers();
        callbackController->startFilterTime = time;
        Manager::ReplayRestarted(false);
        // restarted resets pause value as well
        Manager::paused = true;
        Manager::UpdateTime(time);
        float controllerTime = (time - scoreController->audioTimeSyncController->startSongTime) / scoreController->audioTimeSyncController->timeScale;
        scoreController->audioTimeSyncController->SeekTo(controllerTime);
        auto& replay = Manager::currentReplay;
        if(replay.type == ReplayType::Frame) {
            // let hooks update values
            gameEnergyCounter->ProcessEnergyChange(0);
            auto frame = Manager::Frames::GetScoreFrame();
            int maxScore = frame->score / frame->percent;
            float multiplier = scoreController->gameplayModifiersModel->GetTotalMultiplier(scoreController->gameplayModifierParams, frame->energy);
            int modifiedMaxScore = modifiedMaxScore * multiplier;
            scoreController->immediateMaxPossibleMultipliedScore = maxScore;
            scoreController->immediateMaxPossibleModifiedScore = modifiedMaxScore;
            scoreController->LateUpdate();
            NoteCutInfo info{}; il2cpp_utils::RunMethodUnsafe(comboController, "HandleNoteWasCut", nullptr, byref(info));
        } else {
            // reset energy as we will override it
            Manager::Events::GetWallEnergyLoss() = 0;
            auto eventReplay = (EventReplay*) replay.replay.get();
            float lastCalculatedWall = 0, wallEnd = 0;
            // simulate all events
            for(auto& event: eventReplay->events) {
                if(event.time > time)
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
                    auto& noteEvent = eventReplay->notes[event.index];
                    auto scoringElement = MakeFakeScoringElement(noteEvent);
                    InsertIntoSortedListFromEnd(scoreController->sortedScoringElementsWithoutMultiplier, scoringElement);
                    scoreController->LateUpdate();
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
    }
}
