#include "Main.hpp"
#include "Config.hpp"
#include "CustomTypes/ReplaySettings.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "HMUI/Touchable.hpp"

#include "UnityEngine/RectTransform_Axis.hpp"

DEFINE_TYPE(ReplaySettings, MainSettings)
DEFINE_TYPE(ReplaySettings, RenderSettings)
DEFINE_TYPE(ReplaySettings, InputSettings)
DEFINE_TYPE(ReplaySettings, ModSettings)

using namespace GlobalNamespace;
using namespace QuestUI;
using namespace ReplaySettings;

const std::vector<const char*> buttonNames = {
    "None",
    "Side Trigger",
    "Front Trigger",
    "A/X Button",
    "B/Y Button",
    "Joystick Up",
    "Joystick Down",
    "Joystick Left",
    "Joystick Right"
};
const std::vector<const char*> controllerNames = {
    "Left",
    "Right",
    "Both"
};

void CreateInputDropdowns(UnityEngine::Transform* parent, std::string_view name, std::string_view hint, int buttonValue, int controllerValue, auto buttonCallback, auto controllerCallback) {
    static ConstString labelName("Label");

    auto layout = BeatSaberUI::CreateHorizontalLayoutGroup(parent)->get_transform();
    layout->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredHeight(7);

    std::vector<StringW> buttonNamesList(buttonNames.begin(), buttonNames.end());
    auto dropdown = BeatSaberUI::CreateDropdown(layout, name, buttonNamesList[buttonValue], buttonNamesList, [callback = std::move(buttonCallback)](StringW value){
        for(int i = 0; i < buttonNames.size(); i++) {
            if(value == buttonNames[i]) {
                callback(i);
                break;
            }
        }
    });
    BeatSaberUI::AddHoverHint(dropdown, hint);
    ((UnityEngine::RectTransform*) dropdown->get_transform())->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, 30);

    UnityEngine::Transform* dropdownParent = dropdown->get_transform()->get_parent();
    ((UnityEngine::RectTransform*) dropdownParent->Find(labelName))->set_anchorMax({2, 1});
    dropdownParent->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(53);

    std::vector<StringW> controllerNamesList(controllerNames.begin(), controllerNames.end());
    dropdown = BeatSaberUI::CreateDropdown(layout, "  Controller", controllerNamesList[controllerValue], controllerNamesList, [callback = std::move(controllerCallback)](StringW value){
        for(int i = 0; i < controllerNames.size(); i++) {
            if(value == controllerNames[i]) {
                callback(i);
                break;
            }
        }
    });
    BeatSaberUI::AddHoverHint(dropdown, hint);
    ((UnityEngine::RectTransform*) dropdown->get_transform())->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, 22);

    dropdownParent = dropdown->get_transform()->get_parent();
    ((UnityEngine::RectTransform*) dropdownParent->Find(labelName))->set_anchorMax({2, 1});
    dropdownParent->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(38);
}

std::pair<std::string, std::string> split(const std::string& str, auto delimiter) {
    auto pos = str.find(delimiter);
    return std::make_pair(str.substr(0, pos), str.substr(pos + 1, std::string::npos));
}

inline void AddConfigValueDropdown(UnityEngine::Transform* parent, ConfigUtils::ConfigValue<ButtonPair>& configValue) {
    auto strs = split(configValue.GetName(), "|");
    auto value = configValue.GetValue();
    CreateInputDropdowns(parent, strs.first, configValue.GetHoverHint(), value.ForwardButton, value.ForwardController,
        [&configValue](int newButton) {
            auto value = configValue.GetValue();
            value.ForwardButton = newButton;
            configValue.SetValue(value);
        },
        [&configValue](int newController) {
            auto value = configValue.GetValue();
            value.ForwardController = newController;
            configValue.SetValue(value);
        }
    );
    CreateInputDropdowns(parent, strs.second, configValue.GetHoverHint(), value.BackButton, value.BackController,
        [&configValue](int newButton) {
            auto value = configValue.GetValue();
            value.BackButton = newButton;
            configValue.SetValue(value);
        },
        [&configValue](int newController) {
            auto value = configValue.GetValue();
            value.BackController = newController;
            configValue.SetValue(value);
        }
    );
}

inline void AddConfigValueDropdown(UnityEngine::Transform* parent, ConfigUtils::ConfigValue<Button>& configValue) {
    auto value = configValue.GetValue();
    CreateInputDropdowns(parent, configValue.GetName(), configValue.GetHoverHint(), value.Button, value.Controller,
        [&configValue](int newButton) {
            auto value = configValue.GetValue();
            value.Button = newButton;
            configValue.SetValue(value);
        },
        [&configValue](int newController) {
            auto value = configValue.GetValue();
            value.Controller = newController;
            configValue.SetValue(value);
        }
    );
}

inline UnityEngine::Transform* MakeLayout(HMUI::ViewController* self) {
    self->get_gameObject()->AddComponent<HMUI::Touchable*>();
    auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(self);
    vertical->set_childControlHeight(false);
    vertical->set_childForceExpandHeight(false);
    vertical->set_spacing(1);
    return vertical->get_transform();
}

inline void MakeTitle(UnityEngine::Transform* vertical, std::string text) {
    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(vertical);
    horizontal->set_childControlWidth(true);
    BeatSaberUI::CreateText(horizontal, text)->set_alignment(TMPro::TextAlignmentOptions::Center);
}

const std::vector<std::string> resolutionStrings = {
    "480 x 640",
    "720 x 1280",
    "1080 x 1920",
    "1440 x 2560",
    "2160 x 3840"
};
const std::vector<std::string> fourLevelStrings = {
    "Off",
    "Low",
    "Medium",
    "High"
};

void MainSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    auto transform = MakeLayout(this);

    AddConfigValueIncrementFloat(transform, getConfig().Smoothing, 1, 0.1, 0.1, 5);
    
    AddConfigValueToggle(transform, getConfig().Correction);

    AddConfigValueIncrementVector3(transform, getConfig().Offset, 1, 0.1);

    AddConfigValueToggle(transform, getConfig().Relative);

    AddConfigValueToggle(transform, getConfig().HideText);

    AddConfigValueIncrementFloat(transform, getConfig().TextHeight, 1, 0.2, 0, 10);

    AddConfigValueToggle(transform, getConfig().SimMode);

    AddConfigValueToggle(transform, getConfig().Ding);
}

void RenderSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    auto transform = MakeLayout(this);
    MakeTitle(transform, "Render Settings");

    AddConfigValueToggle(transform, getConfig().Pauses);

    AddConfigValueToggle(transform, getConfig().Walls);

    // AddConfigValueIncrementEnum(transform, getConfig().Bloom, fourLevelStrings);

    AddConfigValueIncrementEnum(transform, getConfig().Mirrors, fourLevelStrings);
    
    shockwaveSetting = AddConfigValueIncrementInt(transform, getConfig().Shockwaves, 1, 1, 20)->get_gameObject();
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

    AddConfigValueIncrementEnum(transform, getConfig().Resolution, resolutionStrings);
    
    AddConfigValueIncrementInt(transform, getConfig().Bitrate, 1000, 0, 100000);
    
    AddConfigValueIncrementFloat(transform, getConfig().FOV, 0, 5, 30, 150);
    
    AddConfigValueIncrementInt(transform, getConfig().FPS, 5, 5, 120);
    
    AddConfigValueToggle(transform, getConfig().CameraOff);
}

void InputSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;
    
    auto transform = MakeLayout(this);
    MakeTitle(transform, "Input Settings");

    AddConfigValueDropdown(transform, getConfig().TimeButton);

    AddConfigValueIncrementInt(transform, getConfig().TimeSkip, 1, 1, 30);
    
    AddConfigValueDropdown(transform, getConfig().SpeedButton);

    AddConfigValueDropdown(transform, getConfig().MoveButton);

    AddConfigValueDropdown(transform, getConfig().TravelButton);

    AddConfigValueIncrementFloat(transform, getConfig().TravelSpeed, 1, 0.1, 0.2, 5);
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
    if(!inputSettings)
        inputSettings = BeatSaberUI::CreateViewController<InputSettings*>();
    
    showBackButton = true;
    static ConstString title("Replay Settings");
    SetTitle(title, HMUI::ViewController::AnimationType::In);

    ProvideInitialViewControllers(mainSettings, renderSettings, inputSettings, nullptr, nullptr);
}

void ModSettings::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
}
