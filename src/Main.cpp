#include "Main.hpp"

#include "ReplayMenu.hpp"
#include "ReplayHooks.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"

using namespace GlobalNamespace;

static ModInfo modInfo;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);
    
    Menu::EnsureSetup(self);
    Manager::SetLevel((CustomDifficultyBeatmap*) self->selectedDifficultyBeatmap);
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

extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;
	
    getLogger().info("Completed setup!");
}

extern "C" void load() {
    il2cpp_functions::Init();
    if(!Paper::Logger::IsInited())
        Paper::Logger::Init("/sdcard/Android/data/com.beatgames.beatsaber/files/logs");

    LOG_INFO("Installing hooks...");
    auto& logger = getLogger();
    Hooks::Install(logger);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed);
    LOG_INFO("Installed all hooks!");
}
