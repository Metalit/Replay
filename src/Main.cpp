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
static bool doRender = true;
static int nonRenderIdx = 0;
MAKE_HOOK_MATCH(LevelFilteringNavigationController_UpdateCustomSongs, &LevelFilteringNavigationController::UpdateCustomSongs, void, LevelFilteringNavigationController* self) {

    LevelFilteringNavigationController_UpdateCustomSongs(self);

    if(!selectedAlready) {
        selectedAlready = true;
        if (doRender)
            RenderLevelInConfig();
        else
            SelectLevelInConfig(nonRenderIdx);
    }
}

void SelectLevelOnNextSongRefresh(bool render, int idx) {
    selectedAlready = false;
    doRender = render;
    nonRenderIdx = idx;
}

ALooper* mainThreadLooper;
int messagePipe[2];
jobject activity = nullptr;
bool looperMade = false;

MAKE_HOOK_NO_CATCH(unity_initJni, 0x0, void, JNIEnv* env, jobject jobj, jobject context) {

    unity_initJni(env, jobj, context);

    if(looperMade)
        return;

    mainThreadLooper = ALooper_forThread();
    ALooper_acquire(mainThreadLooper);
    pipe(messagePipe);
    ALooper_addFd(mainThreadLooper, messagePipe[0], 0, ALOOPER_EVENT_INPUT, LooperCallback, nullptr);
    looperMade = true;
    LOG_INFO("Created looper and registered pipe");
}

int LooperCallback(int fd, int events, void* data) {
    char msg;
    read(fd, &msg, 1);
    bool on = msg;
    JNIUtils::SetScreenOnImpl(on);
    return 1;
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
    // screen on should be cleared when the activity closes

    LOG_INFO("Installing UI thread hook...");
    uintptr_t libunity = baseAddr("libunity.so");
    uintptr_t unity_initJni_addr = findPattern(libunity, "ff c3 00 d1 f5 53 01 a9 f3 7b 02 a9 ff 07 00 f9 08 00 40 f9 f4 03 01 aa e1 23 00 91 f3 03 02 aa 08 6d 43 f9");
    INSTALL_HOOK_DIRECT(getLogger(), unity_initJni, (void*) unity_initJni_addr);
    LOG_INFO("Installed UI thread hook!");

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

    getConfig().AudioMode.SetValue(false);

    if(getConfig().Version.GetValue() == 1) {
        LOG_INFO("Migrating config from v1 to v2");
        getConfig().TextHeight.SetValue(getConfig().TextHeight.GetValue() / 2);
        getConfig().Bitrate.SetValue(getConfig().Bitrate.GetValue() / 2);
        getConfig().Version.SetValue(2);
    }

    // no making the text offscreen through config editing
    if(getConfig().TextHeight.GetValue() > 5)
        getConfig().TextHeight.SetValue(5);
    if(getConfig().TextHeight.GetValue() < 1)
        getConfig().TextHeight.SetValue(1);

    // clamp bitrate to 100 mbps
    if(getConfig().Bitrate.GetValue() > 100000)
        getConfig().Bitrate.SetValue(100000);

    getConfig().AudioMode.SetValue(false);

    if(Modloader::requireMod("bl"))
        recorderInstalled = true;
    else if(Modloader::requireMod("ScoreSaber"))
        recorderInstalled = true;

    LOG_INFO("Recording mod installed: {}", recorderInstalled);
}
