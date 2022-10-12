#include "Main.hpp"
#include "Config.hpp"

#include "CustomTypes/ReplayMenu.hpp"
#include "Hooks.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "CustomTypes/ReplaySettings.hpp"

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"

#include "hollywood/shared/Hollywood.hpp"

#include "QuestUI/shared/QuestUI.hpp"

using namespace GlobalNamespace;

ModInfo modInfo;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);
    
    Menu::EnsureSetup(self);
    Manager::SetLevel(self->selectedDifficultyBeatmap);
    Menu::CheckMultiplayer();
}

MAKE_HOOK_MATCH(SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange, &SinglePlayerLevelSelectionFlowCoordinator::LevelSelectionFlowCoordinatorTopViewControllerWillChange,
        void, SinglePlayerLevelSelectionFlowCoordinator* self, HMUI::ViewController* oldViewController, HMUI::ViewController* newViewController, HMUI::ViewController::AnimationType animationType) {
    
    if(newViewController->get_name() == "ReplayViewController") {
        self->SetLeftScreenViewController(nullptr, animationType);
        self->SetRightScreenViewController(nullptr, animationType);
        self->SetBottomScreenViewController(nullptr, animationType);
        self->SetTitle("REPLAY", animationType);
        self->set_showBackButton(true);
        return;
    }
    
    SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange(self, oldViewController, newViewController, animationType);
}

MAKE_HOOK_MATCH(SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed, &SinglePlayerLevelSelectionFlowCoordinator::BackButtonWasPressed,
        void, SinglePlayerLevelSelectionFlowCoordinator* self, HMUI::ViewController* topViewController) {
    
    if(topViewController->get_name() == "ReplayViewController") {
        self->DismissViewController(topViewController, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
        return;
    }
    
    SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed(self, topViewController);
}

#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/ScoringElement.hpp"
#include "GlobalNamespace/GameplayModifiersModelSO.hpp"
#include "GlobalNamespace/IGameEnergyCounter.hpp"
#include "System/Action_1.hpp"
#include "System/Action_2.hpp"

MAKE_HOOK_MATCH(WeirdFix, &ScoreController::LateUpdate, void, ScoreController* self) {
    LOG_DEBUG("LateUpdate");
    float num = self->sortedNoteTimesWithoutScoringElements->get_Count() > 0 ? self->sortedNoteTimesWithoutScoringElements->get_Item(0) : 3000000000;
    float num2 = self->audioTimeSyncController->get_songTime() + 0.15;
    int num3 = 0;
    bool flag = false;
    LOG_DEBUG("Processing without multiplier");
    for(int i = 0; i < self->sortedScoringElementsWithoutMultiplier->get_Count(); i++) {
        auto item = self->sortedScoringElementsWithoutMultiplier->get_Item(i);
        if(item->get_time() < num2 || item->get_time() > num) {
            flag |= self->scoreMultiplierCounter->ProcessMultiplierEvent(item->get_multiplierEventType());
            if(item->get_wouldBeCorrectCutBestPossibleMultiplierEventType() == ScoreMultiplierCounter::MultiplierEventType::Positive)
                self->maxScoreMultiplierCounter->ProcessMultiplierEvent(ScoreMultiplierCounter::MultiplierEventType::Positive);
            item->SetMultipliers(self->scoreMultiplierCounter->get_multiplier(), self->maxScoreMultiplierCounter->get_multiplier());
            self->scoringElementsWithMultiplier->Add(item);
            num3++;
            continue;
        }
        break;
    }
    LOG_DEBUG("Removing without multiplier");
    self->sortedScoringElementsWithoutMultiplier->RemoveRange(0, num3);
    if(flag)
        self->multiplierDidChangeEvent->Invoke(self->scoreMultiplierCounter->get_multiplier(), self->scoreMultiplierCounter->get_normalizedProgress());
    LOG_DEBUG("Processing with multiplier");
    bool flag2 = false;
    self->scoringElementsToRemove->Clear();
    for(int i = 0; i < self->scoringElementsWithMultiplier->get_Count(); i++) {
        auto item2 = self->scoringElementsWithMultiplier->get_Item(i);
        if (item2->get_isFinished()) {
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
    LOG_DEBUG("Removing with multiplier");
    for(int i = 0; i < self->scoringElementsToRemove->get_Count(); i++) {
        auto item3 = self->scoringElementsToRemove->get_Item(i);
        self->DespawnScoringElement(item3);
        self->scoringElementsWithMultiplier->Remove(item3);
    }
    self->scoringElementsToRemove->Clear();
    LOG_DEBUG("Setting values");
    float totalMultiplier = self->gameplayModifiersModel->GetTotalMultiplier(self->gameplayModifierParams, self->gameEnergyCounter->get_energy());
    if (self->prevMultiplierFromModifiers != totalMultiplier) {
        self->prevMultiplierFromModifiers = totalMultiplier;
        flag2 = true;
    }
    if (flag2) {
        self->modifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->multipliedScore, totalMultiplier);
        self->immediateMaxPossibleModifiedScore = ScoreModel::GetModifiedScoreForGameplayModifiersScoreMultiplier(self->immediateMaxPossibleMultipliedScore, totalMultiplier);
        if(self->scoreDidChangeEvent)
            self->scoreDidChangeEvent->Invoke(self->multipliedScore, self->modifiedScore);
    }
    LOG_DEBUG("Done");
}

extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;
    
    Hollywood::initialize();

    getConfig().Init(info);
	
    getLogger().info("Completed setup!");
}

extern "C" void load() {
    Paper::Logger::RegisterFileContextId("Replay");
    
    il2cpp_functions::Init();

    QuestUI::Register::RegisterModSettingsFlowCoordinator<ReplaySettings::ModSettings*>(modInfo);

    LOG_INFO("Installing hooks...");
    auto& logger = getLogger();
    Hooks::Install(logger);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed);
    INSTALL_HOOK_ORIG(logger, WeirdFix);
    LOG_INFO("Installed all hooks!");
}
