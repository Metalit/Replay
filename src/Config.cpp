#include "Main.hpp"
#include "Config.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "HMUI/Touchable.hpp"

DEFINE_CONFIG(Config)

using namespace GlobalNamespace;
using namespace QuestUI;

void SettingsDidActivate(HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    self->get_gameObject()->AddComponent<HMUI::Touchable*>();
    auto transform = BeatSaberUI::CreateVerticalLayoutGroup(self)->get_transform();

    AddConfigValueIncrementFloat(transform, getConfig().Smoothing, 1, 0.1, 0.1, 5);
    
    AddConfigValueToggle(transform, getConfig().Correction);

    AddConfigValueIncrementVector3(transform, getConfig().Offset, 1, 0.1);

    AddConfigValueToggle(transform, getConfig().Relative);

    AddConfigValueToggle(transform, getConfig().Walls);

    AddConfigValueToggle(transform, getConfig().Shockwaves);

    AddConfigValueIncrementInt(transform, getConfig().Resolution, 1, 0, resolutions.size() - 1);
    
    AddConfigValueIncrementInt(transform, getConfig().Bitrate, 1000, 0, 25000);
    
    AddConfigValueIncrementInt(transform, getConfig().FPS, 5, 5, 120);
    
    AddConfigValueIncrementFloat(transform, getConfig().FOV, 0, 5, 30, 150);
}
