#include "main.hpp"

#include "CustomTypes/ReplaySettings.hpp"
#include "bsml/shared/BSML.hpp"
#include "config.hpp"
#include "custom-types/shared/register.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "hooks.hpp"
#include "config.hpp"

using namespace GlobalNamespace;

modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

bool recorderInstalled = false;

extern "C" void setup(CModInfo* info) {
    *info = modInfo.to_c();

    Paper::Logger::RegisterFileContextId(MOD_ID);

    getConfig().Init(modInfo);

    if (!direxists(RendersFolder))
        mkpath(RendersFolder);

    logger.info("Completed setup!");
}

extern "C" void late_load() {
    il2cpp_functions::Init();
    custom_types::Register::AutoRegister();

    // fix proximity sensor state just in case
    // (there is an option to disable it non-indefinitely for this exact purpose but I can't be bothered)
    Hollywood::SetScreenOn(false);

    BSML::Register::RegisterSettingsMenu<Replay::ModSettings*>(MOD_ID);

    Hooks::Install();

    if (getConfig().Version.GetValue() == 1) {
        logger.info("Migrating config from v1 to v2");
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

    logger.info("Recording mod installed: {}", recorderInstalled);
}
