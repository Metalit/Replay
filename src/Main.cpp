#include "Main.hpp"

#include "Config.hpp"
#include "CustomTypes/ReplayMenu.hpp"
#include "CustomTypes/ReplaySettings.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "HMUI/ViewController.hpp"
#include "Hooks.hpp"
#include "MenuSelection.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "custom-types/shared/register.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "songcore/shared/SongCore.hpp"

using namespace GlobalNamespace;

modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

bool recorderInstalled = false;

MAKE_AUTO_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
    StandardLevelDetailView_RefreshContent(self);

    Menu::EnsureSetup(self);
    Manager::SetLevel({self->beatmapKey, self->_beatmapLevel});
    Menu::CheckMultiplayer();
}

MAKE_AUTO_HOOK_MATCH(
    SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange,
    &SinglePlayerLevelSelectionFlowCoordinator::LevelSelectionFlowCoordinatorTopViewControllerWillChange,
    void,
    SinglePlayerLevelSelectionFlowCoordinator* self,
    HMUI::ViewController* oldViewController,
    HMUI::ViewController* newViewController,
    HMUI::ViewController::AnimationType animationType
) {
    if (newViewController->get_name() == "ReplayViewController") {
        self->SetLeftScreenViewController(nullptr, animationType);
        self->SetRightScreenViewController(nullptr, animationType);
        self->SetBottomScreenViewController(nullptr, animationType);
        self->SetTitle("REPLAY", animationType);
        self->showBackButton = true;
        return;
    }

    SinglePlayerLevelSelectionFlowCoordinator_LevelSelectionFlowCoordinatorTopViewControllerWillChange(
        self, oldViewController, newViewController, animationType
    );
}

MAKE_AUTO_HOOK_MATCH(
    SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed,
    &SinglePlayerLevelSelectionFlowCoordinator::BackButtonWasPressed,
    void,
    SinglePlayerLevelSelectionFlowCoordinator* self,
    HMUI::ViewController* topViewController
) {
    if (topViewController->name == "ReplayViewController") {
        self->DismissViewController(topViewController, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
        return;
    }

    SinglePlayerLevelSelectionFlowCoordinator_BackButtonWasPressed(self, topViewController);
}

static bool selectedAlready = false;
static bool doRender = true;
static int nonRenderIdx = 0;

void OnSongsLoaded(std::span<SongCore::SongLoader::CustomBeatmapLevel* const>) {
    if (!selectedAlready) {
        BSML::MainThreadScheduler::ScheduleNextFrame([]() {
            LOG_DEBUG("Selecting level {} {}", doRender, nonRenderIdx);
            selectedAlready = true;
            if (doRender)
                RenderLevelInConfig();
            else
                SelectLevelInConfig(nonRenderIdx);
        });
    }
}

void SelectLevelOnNextSongRefresh(bool render, int idx) {
    selectedAlready = false;
    doRender = render;
    nonRenderIdx = idx;
}

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);

    getConfig().Init(modInfo);

    if (!direxists(RendersFolder))
        mkpath(RendersFolder);

    LOG_INFO("Completed setup!");
}

extern "C" void load() {
    il2cpp_functions::Init();

    InstallBlitHook();

    Hollywood::Init();

    // in case it crashed during a render, unmute
    // the quest only adjusts volume and doesn't have a mute button so this shouldn't mess with anyone
    Hollywood::SetMuteSpeakers(false);
    // fix proximity sensor state too
    Hollywood::SetScreenOn(false);
}

extern "C" void late_load() {
    custom_types::Register::AutoRegister();

    BSML::Register::RegisterSettingsMenu<ReplaySettings::ModSettings*>(MOD_ID);

    SongCore::API::Loading::GetSongsLoadedEvent().addCallback(OnSongsLoaded);

    LOG_INFO("Installing hooks...");
    Hooks::Install();
    LOG_INFO("Installed all hooks!");

    selectedAlready = !getConfig().RenderLaunch.GetValue();
    getConfig().RenderLaunch.SetValue(false);

    if (getConfig().Version.GetValue() == 1) {
        LOG_INFO("Migrating config from v1 to v2");
        getConfig().TextHeight.SetValue(getConfig().TextHeight.GetValue() / 2);
        getConfig().Bitrate.SetValue(getConfig().Bitrate.GetValue() / 2);
        getConfig().Version.SetValue(2);
    }

    if (getConfig().ThirdPerPresets.GetValue().empty()) {
        ThirdPerPreset preset;
        preset.Position = getConfig().ThirdPerPos.GetValue();
        preset.Rotation = getConfig().ThirdPerRot.GetValue();
        getConfig().ThirdPerPresets.SetValue({{"Default", preset}});
    }
    if (!getConfig().ThirdPerPresets.GetValue().contains(getConfig().CurrentThirdPerPreset.GetValue()))
        getConfig().CurrentThirdPerPreset.SetValue(getConfig().ThirdPerPresets.GetValue().begin()->first);

    // no making the text offscreen through config editing
    if (getConfig().TextHeight.GetValue() > 5)
        getConfig().TextHeight.SetValue(5);
    if (getConfig().TextHeight.GetValue() < 1)
        getConfig().TextHeight.SetValue(1);

    CModInfo beatleader{.id = "bl"};
    CModInfo scoresaber{.id = "ScoreSaber"};

    if (modloader_require_mod(&beatleader, CMatchType::MatchType_IdOnly) == CLoadResultEnum::MatchType_Loaded)
        recorderInstalled = true;
    else if (modloader_require_mod(&scoresaber, CMatchType::MatchType_IdOnly) == CLoadResultEnum::MatchType_Loaded)
        recorderInstalled = true;

    LOG_INFO("Recording mod installed: {}", recorderInstalled);
}
