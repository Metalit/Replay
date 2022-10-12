#include "Main.hpp"
#include "Config.hpp"
#include "CustomTypes/ReplaySettings.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "HMUI/Touchable.hpp"

DEFINE_CONFIG(Config)

DEFINE_TYPE(ReplaySettings, MainSettings)
DEFINE_TYPE(ReplaySettings, RenderSettings)
DEFINE_TYPE(ReplaySettings, ModSettings)

using namespace GlobalNamespace;
using namespace QuestUI;
using namespace ReplaySettings;

inline UnityEngine::UI::Toggle* AddConfigValueToggle(UnityEngine::Transform* parent, ConfigUtils::ConfigValue<bool>& configValue, std::function<void(bool value)> callback) {
    auto object = BeatSaberUI::CreateToggle(parent, configValue.GetName(), configValue.GetValue(), 
        [&configValue, callback = std::move(callback)](bool value) { 
            configValue.SetValue(value); 
            callback(value);
        }
    );
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(object->get_gameObject(), configValue.GetHoverHint());
    return object;
}

inline IncrementSetting* AddConfigValueIncrementEnum(UnityEngine::Transform* parent, ConfigUtils::ConfigValue<int>& configValue, int increment, int min, int max, auto& strings) {
    auto inc = BeatSaberUI::CreateIncrementSetting(parent, configValue.GetName(), 0, increment, configValue.GetValue(), min, max);
    auto child = inc->get_gameObject()->get_transform()->GetChild(1);
    auto decButton = child->GetComponentsInChildren<UnityEngine::UI::Button*>().First();
    auto incButton = child->GetComponentsInChildren<UnityEngine::UI::Button*>().Last();
    inc->OnValueChange = [&configValue, inc, &strings, decButton, incButton](float value) {
        configValue.SetValue((int) value);
        inc->Text->set_text(strings[value]);
        decButton->set_interactable(value > inc->MinValue);
        incButton->set_interactable(value < inc->MaxValue);
    };
    inc->Text->set_text(strings[inc->CurrentValue]);
    decButton->set_interactable(inc->CurrentValue > inc->MinValue);
    incButton->set_interactable(inc->CurrentValue < inc->MaxValue);
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(inc, configValue.GetHoverHint());
    return inc;
}

inline IncrementSetting* SetButtons(IncrementSetting* increment) {
    auto child = increment->get_gameObject()->get_transform()->GetChild(1);
    auto decButton = child->GetComponentsInChildren<UnityEngine::UI::Button*>().First();
    auto incButton = child->GetComponentsInChildren<UnityEngine::UI::Button*>().Last();
    increment->OnValueChange = [oldFunc = std::move(increment->OnValueChange), increment, decButton, incButton](float value) {
        oldFunc(value);
        decButton->set_interactable(value > increment->MinValue);
        incButton->set_interactable(value < increment->MaxValue);
    };
    decButton->set_interactable(increment->CurrentValue > increment->MinValue);
    incButton->set_interactable(increment->CurrentValue < increment->MaxValue);
    return increment;
}

const std::vector<const char*> resolutionStrings = {
    "480 x 640",
    "720 x 1280",
    "1080 x 1920",
    "1440 x 2560",
    "2160 x 3840"
};
const std::vector<const char*> fourLevelStrings = {
    "Off",
    "Low",
    "Medium",
    "High"
};

void MainSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    get_gameObject()->AddComponent<HMUI::Touchable*>();
    auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(this);
    vertical->set_childControlHeight(false);
    vertical->set_childForceExpandHeight(false);
    vertical->set_spacing(1);
    auto transform = vertical->get_transform();

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(transform);
    horizontal->set_childControlWidth(true);
    BeatSaberUI::CreateText(horizontal, "Main Settings")->set_alignment(TMPro::TextAlignmentOptions::Center);

    SetButtons(AddConfigValueIncrementFloat(transform, getConfig().Smoothing, 1, 0.1, 0.1, 5));
    
    AddConfigValueToggle(transform, getConfig().Correction);

    AddConfigValueIncrementVector3(transform, getConfig().Offset, 1, 0.1);

    AddConfigValueToggle(transform, getConfig().Relative);

    AddConfigValueToggle(transform, getConfig().HideText);

    SetButtons(AddConfigValueIncrementFloat(transform, getConfig().TextHeight, 1, 0.2, 0, 10));
}

void RenderSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    get_gameObject()->AddComponent<HMUI::Touchable*>();
    auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(this);
    vertical->set_childControlHeight(false);
    vertical->set_childForceExpandHeight(false);
    vertical->set_spacing(1);
    auto transform = vertical->get_transform();

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(transform);
    horizontal->set_childControlWidth(true);
    BeatSaberUI::CreateText(horizontal, "Render Settings")->set_alignment(TMPro::TextAlignmentOptions::Center);

    AddConfigValueToggle(transform, getConfig().Pauses);

    AddConfigValueToggle(transform, getConfig().Walls);

    // AddConfigValueIncrementEnum(transform, getConfig().Bloom, 1, 0, 3, fourLevelStrings);

    AddConfigValueIncrementEnum(transform, getConfig().Mirrors, 1, 0, 3, fourLevelStrings);
    
    shockwaveSetting = SetButtons(AddConfigValueIncrementInt(transform, getConfig().Shockwaves, 1, 1, 20))->get_gameObject();
    auto incrementObject = shockwaveSetting->get_transform()->GetChild(1);
    incrementObject->get_gameObject()->SetActive(getConfig().ShockwavesOn.GetValue());
    incrementObject->GetComponent<UnityEngine::RectTransform*>()->set_anchoredPosition({-20, 0});

    auto shockwaveToggle = BeatSaberUI::CreateToggle(transform, "", getConfig().ShockwavesOn.GetValue(), [this](bool enabled) {
        getConfig().ShockwavesOn.SetValue(enabled);
        shockwaveSetting->get_transform()->GetChild(1)->get_gameObject()->SetActive(enabled);
    })->get_transform();
    auto oldParent = shockwaveToggle->GetParent()->get_gameObject();
    shockwaveToggle->SetParent(shockwaveSetting->get_transform(), false);
    UnityEngine::Object::Destroy(oldParent);

    AddConfigValueIncrementEnum(transform, getConfig().Resolution, 1, 0, resolutions.size() - 1, resolutionStrings);
    
    SetButtons(AddConfigValueIncrementInt(transform, getConfig().Bitrate, 1000, 0, 100000));
    
    SetButtons(AddConfigValueIncrementFloat(transform, getConfig().FOV, 0, 5, 30, 150));
    
    AddConfigValueToggle(transform, getConfig().ForceFPS, [this](bool enabled) {
        fpsSetting->SetActive(enabled);
    });
    
    fpsSetting = SetButtons(AddConfigValueIncrementInt(transform, getConfig().FPS, 5, 5, 120))->get_gameObject();
    fpsSetting->SetActive(getConfig().ForceFPS.GetValue());
}

#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"

void ModSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;
    
    if(!mainSettings)
        mainSettings = BeatSaberUI::CreateViewController<MainSettings*>();
    if(!renderSettings)
        renderSettings = BeatSaberUI::CreateViewController<RenderSettings*>();
    
    showBackButton = true;
    static ConstString title("Replay Settings");
    SetTitle(title, HMUI::ViewController::AnimationType::In);

    ProvideInitialViewControllers(mainSettings, renderSettings, nullptr, nullptr, nullptr);
}

void ModSettings::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
}
