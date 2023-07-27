#include "Main.hpp"
#include "Config.hpp"

#include "CustomTypes/ReplayMenu.hpp"
#include "Hooks.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "JNIUtils.hpp"
#include "CustomTypes/ReplaySettings.hpp"

using namespace GlobalNamespace;

ModInfo modInfo;

bool recorderInstalled = false;

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

#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"

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

#include "GlobalNamespace/LevelFilteringNavigationController.hpp"
#include "MenuSelection.hpp"

static bool selectedAlready = false;
MAKE_HOOK_MATCH(LevelFilteringNavigationController_UpdateCustomSongs, &LevelFilteringNavigationController::UpdateCustomSongs, void, LevelFilteringNavigationController* self) {

    LevelFilteringNavigationController_UpdateCustomSongs(self);

    if(!selectedAlready) {
        selectedAlready = true;
        RenderLevelInConfig();
    }
}

void RenderLevelOnNextSongRefresh() {
    selectedAlready = false;
}

extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;

    getConfig().Init(info);

    if(!direxists(RendersFolder))
        mkpath(RendersFolder);

    // in case it crashed during a render, unmute
    // the quest only adjusts volume and doesn't have a mute button so this shouldn't mess with anyone
    JNIUtils::SetMute(false);

    getLogger().info("Completed setup!");
}

#include "questui/shared/QuestUI.hpp"

#include "custom-types/shared/register.hpp"

extern "C" void load() {
    Paper::Logger::RegisterFileContextId("Replay");

    il2cpp_functions::Init();

    custom_types::Register::AutoRegister();

    QuestUI::Register::RegisterModSettingsFlowCoordinator<ReplaySettings::ModSettings*>(modInfo);

    LOG_INFO("Installing hooks...");
    auto& logger = getLogger();
    Hooks::Install(logger);
    INSTALL_HOOK(logger, StandardLevelDetailView_RefreshContent);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange);
    INSTALL_HOOK(logger, SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed);
    INSTALL_HOOK(logger, LevelFilteringNavigationController_UpdateCustomSongs);
    LOG_INFO("Installed all hooks!");

    selectedAlready = !getConfig().RenderLaunch.GetValue();
    getConfig().RenderLaunch.SetValue(false);

    if(Modloader::requireMod("bl"))
        recorderInstalled = true;
    else if(Modloader::requireMod("ScoreSaber"))
        recorderInstalled = true;

    LOG_INFO("Recording mod installed: {}", recorderInstalled);
}
