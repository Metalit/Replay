#include "Config.hpp"

#include "CustomTypes/ReplaySettings.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/IPreviewMediaData.hpp"
#include "HMUI/TextSegmentedControl.hpp"
#include "HMUI/Touchable.hpp"
#include "Main.hpp"
#include "MenuSelection.hpp"
#include "System/Action.hpp"
#include "System/Action_2.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "UnityEngine/WaitForSeconds.hpp"
#include "Utils.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/delegates.hpp"
#include "bsml/shared/Helpers/getters.hpp"

DEFINE_TYPE(ReplaySettings, MainSettings)
DEFINE_TYPE(ReplaySettings, RenderSettings)
DEFINE_TYPE(ReplaySettings, InputSettings)
DEFINE_TYPE(ReplaySettings, ModSettings)
DEFINE_TYPE(ReplaySettings, KeyboardCloseHandler)

using namespace GlobalNamespace;
using namespace ReplaySettings;

ModSettings* modSettingsInstance = nullptr;

std::vector<std::string_view> buttonNames = {
    "None", "Side Trigger", "Front Trigger", "A/X Button", "B/Y Button", "Joystick Up", "Joystick Down", "Joystick Left", "Joystick Right"
};
std::vector<std::string_view> controllerNames = {"Left", "Right", "Both"};

void CreateInputDropdowns(
    BSML::Lite::TransformWrapper parent,
    std::string_view name,
    std::string_view hint,
    int buttonValue,
    int controllerValue,
    auto buttonCallback,
    auto controllerCallback
) {
    static ConstString labelName("Label");

    auto layout = BSML::Lite::CreateHorizontalLayoutGroup(parent);
    layout->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredHeight = 7;

    BSML::DropdownListSetting* dropdown =
        BSML::Lite::CreateDropdown(layout, name, buttonNames[buttonValue], buttonNames, [callback = std::move(buttonCallback)](StringW value) {
            for (int i = 0; i < buttonNames.size(); i++) {
                if (value == buttonNames[i]) {
                    callback(i);
                    break;
                }
            }
        });
    BSML::Lite::AddHoverHint(dropdown, hint);
    dropdown->GetComponent<UnityEngine::RectTransform*>()->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, 30);

    auto dropdownParent = dropdown->transform->parent;
    dropdownParent->Find(labelName)->GetComponent<UnityEngine::RectTransform*>()->anchorMax = {2, 1};
    dropdownParent->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 53;

    dropdown = BSML::Lite::CreateDropdown(
        layout,
        "  Controller",
        controllerNames[controllerValue],
        controllerNames,
        [callback = std::move(controllerCallback)](StringW value) {
            for (int i = 0; i < controllerNames.size(); i++) {
                if (value == controllerNames[i]) {
                    callback(i);
                    break;
                }
            }
        }
    );
    BSML::Lite::AddHoverHint(dropdown, hint);
    dropdown->GetComponent<UnityEngine::RectTransform*>()->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, 22);

    dropdownParent = dropdown->transform->parent;
    dropdownParent->Find(labelName)->GetComponent<UnityEngine::RectTransform*>()->anchorMax = {2, 1};
    dropdownParent->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 38;
}

inline UnityEngine::UI::Button* CreateSmallButton(BSML::Lite::TransformWrapper parent, std::string text, auto callback) {
    auto button = BSML::Lite::CreateUIButton(parent, text, callback);
    auto fitter = button->gameObject->template AddComponent<UnityEngine::UI::ContentSizeFitter*>();
    fitter->horizontalFit = UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize;
    fitter->verticalFit = UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize;
    return button;
}

std::pair<std::string, std::string> split(std::string const& str, auto delimiter) {
    auto pos = str.find(delimiter);
    return std::make_pair(str.substr(0, pos), str.substr(pos + 1, std::string::npos));
}

inline void AddConfigValueDropdown(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<ButtonPair>& configValue) {
    auto strs = split(configValue.GetName(), "|");
    auto value = configValue.GetValue();
    CreateInputDropdowns(
        parent,
        strs.first,
        configValue.GetHoverHint(),
        value.ForwardButton,
        value.ForwardController,
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
    CreateInputDropdowns(
        parent,
        strs.second,
        configValue.GetHoverHint(),
        value.BackButton,
        value.BackController,
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

inline void AddConfigValueDropdown(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<Button>& configValue) {
    auto value = configValue.GetValue();
    CreateInputDropdowns(
        parent,
        configValue.GetName(),
        configValue.GetHoverHint(),
        value.Button,
        value.Controller,
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

inline UnityEngine::Transform* MakeVertical(BSML::Lite::TransformWrapper parent) {
    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(parent);
    vertical->childControlHeight = false;
    vertical->childForceExpandHeight = false;
    vertical->spacing = 1;
    return vertical->transform;
}

inline UnityEngine::Transform* MakeLayout(HMUI::ViewController* self, float verticalOffset = 0) {
    self->gameObject->AddComponent<HMUI::Touchable*>();
    auto ret = MakeVertical(self);
    if (verticalOffset != 0)
        self->rectTransform->sizeDelta = {0, verticalOffset * 2};
    return ret;
}

HMUI::TextSegmentedControl* MakeTabSelector(BSML::Lite::TransformWrapper parent, std::vector<std::string> texts, auto callback) {
    static UnityW<HMUI::TextSegmentedControl> tabSelectorTagTemplate;
    if (!tabSelectorTagTemplate) {
        tabSelectorTagTemplate = UnityEngine::Resources::FindObjectsOfTypeAll<HMUI::TextSegmentedControl*>()->FirstOrDefault([](auto x) {
            UnityEngine::Transform* parent = x->transform->parent;
            if (!parent)
                return false;
            if (parent->name != "PlayerStatisticsViewController")
                return false;
            return x->_container != nullptr;
        });
    }

    auto textSegmentedControl = UnityEngine::Object::Instantiate(tabSelectorTagTemplate.ptr(), parent->transform, false);
    textSegmentedControl->_dataSource = nullptr;

    auto gameObject = textSegmentedControl->gameObject;
    gameObject->name = "ReplayTabSelector";
    textSegmentedControl->_container = tabSelectorTagTemplate->_container;

    auto transform = gameObject->template GetComponent<UnityEngine::RectTransform*>();
    transform->anchoredPosition = {0, 0};
    int childCount = transform->childCount;
    for (int i = 1; i <= childCount; i++)
        UnityEngine::Object::DestroyImmediate(transform->GetChild(childCount - i)->gameObject);

    auto textList = ListW<StringW>::New(texts.size());
    for (auto text : texts)
        textList->Add(text);
    textSegmentedControl->SetTexts(textList->i___System__Collections__Generic__IReadOnlyList_1_T_());

    auto delegate = BSML::MakeSystemAction((std::function<void(UnityW<HMUI::SegmentedControl>, int)>) [callback](
        UnityW<HMUI::SegmentedControl>, int selectedIndex
    ) { callback(selectedIndex); });
    textSegmentedControl->add_didSelectCellEvent(delegate);

    gameObject->SetActive(true);
    return textSegmentedControl;
}

inline BSML::ToggleSetting* AddConfigValueToggle(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = BSML::Lite::CreateToggle(parent, configValue.GetName(), configValue.GetValue(), [&configValue, callback](bool value) mutable {
        configValue.SetValue(value);
        callback(value);
    });
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object->gameObject, configValue.GetHoverHint());
    object->template GetComponent<UnityEngine::RectTransform*>()->sizeDelta = {18, 8};
    return object;
}

inline void AddIncrementIncrement(BSML::IncrementSetting* setting, float increment) {
    auto transform = setting->transform->Find("ValuePicker").cast<UnityEngine::RectTransform>();
    transform->anchoredPosition = {-6, 0};

    auto leftButton = BSML::Lite::CreateUIButton(transform, "", "DecButton", {-20, 0}, {6, 8}, [setting, increment]() {
        setting->currentValue -= increment;
        setting->EitherPressed();
    });
    auto rightButton = BSML::Lite::CreateUIButton(transform, "", "IncButton", {7, 0}, {8, 8}, [setting, increment]() {
        setting->currentValue += increment;
        setting->EitherPressed();
    });
}

std::vector<std::string> const resolutionStrings = {"480 x 640", "720 x 1280", "1080 x 1920", "1440 x 2560", "2160 x 3840"};
std::vector<std::string> const wallStrings = {"Transparent", "Textured", "Distorted"};
std::vector<std::string> const mirrorStrings = {"Off", "Low", "Medium", "High"};
std::vector<std::string> const aaStrings = {"0", "2", "4", "8"};

void MainSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    auto transform = MakeLayout(this);

    AddConfigValueIncrementFloat(transform, getConfig().Smoothing, 1, 0.1, 0.1, 5);

    AddConfigValueToggle(transform, getConfig().Correction);

    AddConfigValueIncrementFloat(transform, getConfig().TargetTilt, 0, 1, -60, 60);

    AddConfigValueIncrementVector3(transform, getConfig().Offset, 1, 0.1);

    AddConfigValueToggle(transform, getConfig().Relative);

    AddConfigValueToggle(transform, getConfig().HideText);

    AddConfigValueIncrementFloat(transform, getConfig().TextHeight, 1, 0.1, 1, 5);

    AddConfigValueToggle(transform, getConfig().Avatar);
}

void RenderSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    auto transform = MakeLayout(this, 8);
    auto graphics = MakeVertical(transform);
    auto rendering = MakeVertical(transform);
    MakeTabSelector(transform, {"Graphics", "Rendering"}, [graphics, rendering](int selected) {
        graphics->gameObject->active = selected == 0;
        rendering->gameObject->active = selected == 1;
    })->transform->SetAsFirstSibling();

    AddConfigValueIncrementEnum(graphics, getConfig().Walls, wallStrings);

    AddConfigValueToggle(graphics, getConfig().Bloom);

    AddConfigValueIncrementEnum(graphics, getConfig().Mirrors, mirrorStrings);

    AddConfigValueIncrementEnum(graphics, getConfig().AntiAlias, aaStrings);

    auto shockwaveIncrement = AddConfigValueIncrementInt(graphics, getConfig().Shockwaves, 1, 1, 20);
    auto incrementObject = shockwaveIncrement->transform->GetChild(1)->gameObject;
    incrementObject->active = getConfig().ShockwavesOn.GetValue();
    incrementObject->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {-20, 0};

    auto shockwaveToggle =
        AddConfigValueToggle(graphics, getConfig().ShockwavesOn, [incrementObject](bool enabled) mutable { incrementObject->active = enabled; });
    shockwaveToggle->toggle->transform->SetParent(shockwaveIncrement->transform, false);
    shockwaveToggle->transform->SetParent(shockwaveIncrement->transform, false);
    UnityEngine::Object::Destroy(shockwaveToggle->text->gameObject);

    AddConfigValueIncrementEnum(graphics, getConfig().Resolution, resolutionStrings);

    auto bitrateSlider = AddConfigValueSliderIncrement(graphics, getConfig().Bitrate, 2500, 2500, 25000);
    bitrateSlider->isInt = true;
    bitrateSlider->text->text = bitrateSlider->TextForValue(bitrateSlider->get_Value());

    AddConfigValueIncrementFloat(graphics, getConfig().FOV, 0, 5, 30, 90);

    AddConfigValueIncrementInt(graphics, getConfig().FPS, 5, 5, 120);

    AddConfigValueToggle(rendering, getConfig().Pauses);

    AddConfigValueToggle(rendering, getConfig().Ding);

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(rendering);

    beginQueueButton = CreateSmallButton(horizontal, "Begin Queue", [this]() {
        RenderLevelInConfig();
        OnEnable();
    });

    clearQueueButton = CreateSmallButton(horizontal, "Clear Queue", [this]() {
        getConfig().LevelsToSelect.SetValue({});
        OnEnable();
    });

    queueList = BSML::Lite::CreateScrollableList(rendering, {90, 51}, [this](int idx) {
        queueList->tableView->ClearSelection();
        if (modSettingsInstance) {
            auto delegate = custom_types::MakeDelegate<System::Action*>((std::function<void()>) [idx]() { SelectLevelInConfig(idx); });
            modSettingsInstance->_parentFlowCoordinator->DismissFlowCoordinator(
                modSettingsInstance, HMUI::ViewController::AnimationDirection::Horizontal, delegate, false
            );
        } else
            SelectLevelInConfig(idx);
    });
    queueList->listStyle = BSML::CustomListTableData::ListStyle::List;
    queueList->expandCell = true;
    OnEnable();

    // for whatever reason, the button is misaligned for a bit, probably the content size fitter
    UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate((UnityEngine::RectTransform*) rendering);
    rendering->gameObject->active = false;
}

custom_types::Helpers::Coroutine RenderSettings::GetCoverCoro(BeatmapLevel* level) {
    auto result = level->previewMediaData->GetCoverSpriteAsync(System::Threading::CancellationToken::get_None());

    // make sure all the cells have been constructed
    co_yield nullptr;

    while (!result->IsCompleted)
        co_yield (System::Collections::IEnumerator*) UnityEngine::WaitForSeconds::New_ctor(0.1);

    UpdateCover(level, result->ResultOnSuccess);
    co_return;
}

void RenderSettings::GetCover(BeatmapLevel* level) {
    StartCoroutine(custom_types::Helpers::CoroutineHelper::New(GetCoverCoro(level)));
}

void RenderSettings::UpdateCover(BeatmapLevel* level, UnityEngine::Sprite* cover) {
    if (!queueList || !enabled)
        return;
    auto levels = getConfig().LevelsToSelect.GetValue();
    for (int i = 0; i < levels.size(); i++) {
        if (levels[i].ID == level->levelID)
            queueList->data[i]->icon = cover;
    }
    queueList->tableView->ReloadData();
}

void RenderSettings::OnEnable() {
    auto levels = getConfig().LevelsToSelect.GetValue();
    bool empty = levels.empty();
    if (beginQueueButton)
        beginQueueButton->interactable = !empty;
    if (clearQueueButton)
        clearQueueButton->interactable = !empty;
    if (queueList) {
        queueList->data->Clear();
        for (auto& selection : levels) {
            auto level = BSML::Helpers::GetMainFlowCoordinator()->_beatmapLevelsModel->GetBeatmapLevel(selection.ID);
            if (!level) {
                queueList->data->Add(BSML::CustomCellInfo::construct("Couldn't load level", "Exit and reopen settings to try again", nullptr));
                continue;
            }
            std::string name = level->songName;
            std::string difficulty = GetDifficultyName(selection.Difficulty);
            std::string characteristic = GetCharacteristicName(selection.Characteristic);
            std::string toptext = "<voffset=0.1em>" + name + "  <size=75%><color=#D6D6D6>" + characteristic + " " + difficulty;
            std::string author = level->songAuthorName;
            std::string mapper = "";
            if (!level->allMappers.Empty())
                mapper = fmt::format(" [{}]", level->allMappers->First());
            std::string subtext = fmt::format("{}{} - Replay Index {}", author, mapper, selection.ReplayIndex + 1);
            queueList->data->Add(BSML::CustomCellInfo::construct(toptext, subtext, nullptr));
            GetCover(level);
        }
        queueList->tableView->ReloadData();
    }
}

void RenderSettings::OnDisable() {
    StopAllCoroutines();
}

void InputSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    auto transform = MakeLayout(this, 8);
    auto inputs = MakeVertical(transform);
    auto position = MakeVertical(transform);
    MakeTabSelector(transform, {"Inputs", "Position"}, [inputs, position](int selected) {
        inputs->gameObject->SetActive(selected == 0);
        position->gameObject->SetActive(selected == 1);
    })->transform->SetAsFirstSibling();

    AddConfigValueDropdown(inputs, getConfig().TimeButton);

    AddConfigValueIncrementInt(inputs, getConfig().TimeSkip, 1, 1, 30);

    AddConfigValueDropdown(inputs, getConfig().SpeedButton);

    AddConfigValueDropdown(inputs, getConfig().MoveButton);

    AddConfigValueDropdown(inputs, getConfig().TravelButton);

    AddConfigValueIncrementFloat(inputs, getConfig().TravelSpeed, 1, 0.1, 0.2, 5);

    positionSettings = AddConfigValueIncrementVector3(position, getConfig().ThirdPerPos, 1, 0.1);
    for (auto& setting : positionSettings)
        AddIncrementIncrement(setting, 1);

    CreateSmallButton(position, "Reset Position", [this]() {
        getConfig().ThirdPerPos.SetValue(getConfig().ThirdPerPos.GetDefaultValue());
        OnEnable();
    });

    rotationSettings = AddConfigValueIncrementVector3(position, getConfig().ThirdPerRot, 0, 1);
    for (auto& setting : rotationSettings)
        AddIncrementIncrement(setting, 10);

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(position);

    CreateSmallButton(horizontal, "Level Rotation", [this]() {
        auto value = getConfig().ThirdPerRot.GetValue();
        getConfig().ThirdPerRot.SetValue({0, value.y, value.z});
        OnEnable();
    });

    CreateSmallButton(horizontal, "Reset Rotation", [this]() {
        getConfig().ThirdPerRot.SetValue(getConfig().ThirdPerRot.GetDefaultValue());
        OnEnable();
    });

    auto layout = BSML::Lite::CreateHorizontalLayoutGroup(position);
    layout->spacing = 1;

    auto presets = getConfig().ThirdPerPresets.GetValue();
    std::vector<std::string_view> presetNames = {};
    for (auto& [name, _] : presets)
        presetNames.emplace_back(name);

    // too lazy to update values and stuff properly
    presetDropdown = BSML::Lite::CreateDropdown(layout, "Preset", getConfig().ThirdPerPreset.GetValue(), presetNames, [this](StringW name) {
        OnDisable();
        getConfig().ThirdPerPreset.SetValue(name);
        auto preset = getConfig().ThirdPerPresets.GetValue()[name];
        getConfig().ThirdPerPos.SetValue(preset.Position);
        getConfig().ThirdPerRot.SetValue(preset.Rotation);
        OnEnable();
    });
    presetDropdown->transform->parent->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 50;

    auto addPresetButton = BSML::Lite::CreateUIButton(layout, "Add", [this]() { nameModal->Show(true, true, nullptr); });
    addPresetButton->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 15;

    removePresetButton = BSML::Lite::CreateUIButton(layout, "Remove", [this]() {
        auto presets = getConfig().ThirdPerPresets.GetValue();
        if (presets.size() <= 1)
            return;
        presets.erase(getConfig().ThirdPerPreset.GetValue());
        getConfig().ThirdPerPresets.SetValue(presets);
        getConfig().ThirdPerPreset.SetValue(presets.begin()->first);
        getConfig().ThirdPerPos.SetValue(presets.begin()->second.Position);
        getConfig().ThirdPerRot.SetValue(presets.begin()->second.Rotation);
        OnEnable();
    });
    removePresetButton->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 20;
    removePresetButton->interactable = getConfig().ThirdPerPresets.GetValue().size() > 1;

    nameModal = BSML::Lite::CreateModal(this, {95, 20}, nullptr);
    auto modalLayout2 = BSML::Lite::CreateVerticalLayoutGroup(nameModal);
    modalLayout2->childControlHeight = false;
    modalLayout2->childForceExpandHeight = true;
    modalLayout2->spacing = 1;

    auto text2 = BSML::Lite::CreateText(modalLayout2, "Enter new preset name", {0, 0}, {50, 8});
    text2->alignment = TMPro::TextAlignmentOptions::Bottom;

    nameInput = BSML::Lite::CreateStringSetting(modalLayout2, "Name", "", {0, 0}, {0, 0, 0});
    nameInput->gameObject->AddComponent<KeyboardCloseHandler*>()->okCallback = [this]() {
        std::string val = nameInput->text;
        if (val.empty())
            return;
        nameModal->Hide(true, nullptr);
        OnDisable();  // save current preset
        getConfig().ThirdPerPreset.SetValue(val);
        ThirdPerPreset preset;
        preset.Position = getConfig().ThirdPerPos.GetDefaultValue();
        preset.Rotation = getConfig().ThirdPerRot.GetDefaultValue();
        getConfig().ThirdPerPos.SetValue(preset.Position);
        getConfig().ThirdPerRot.SetValue(preset.Rotation);
        auto presets = getConfig().ThirdPerPresets.GetValue();
        presets[val] = preset;
        getConfig().ThirdPerPresets.SetValue(presets);
        OnEnable();
    };

    position->gameObject->active = false;
}

void InputSettings::OnEnable() {
    if (!positionSettings[0] || !rotationSettings[0] || !removePresetButton)
        return;
    auto pos = getConfig().ThirdPerPos.GetValue();
    positionSettings[0]->set_Value(pos.x);
    positionSettings[1]->set_Value(pos.y);
    positionSettings[2]->set_Value(pos.z);
    auto rot = getConfig().ThirdPerRot.GetValue();
    rotationSettings[0]->set_Value(rot.x);
    rotationSettings[1]->set_Value(rot.y);
    rotationSettings[2]->set_Value(rot.z);
    removePresetButton->interactable = getConfig().ThirdPerPresets.GetValue().size() > 1;
    // all this to update a dropdown's values
    auto presets = getConfig().ThirdPerPresets.GetValue();
    auto preset = getConfig().ThirdPerPreset.GetValue();
    std::vector<std::string_view> presetNames;
    for (auto& [name, _] : presets)
        presetNames.emplace_back(name);
    auto texts = ListW<System::Object*>::New(presetNames.size());
    int idx = 0;
    for (int i = 0; i < presetNames.size(); i++) {
        texts->Add((System::Object*) StringW(presetNames[i]).convert());
        if (presetNames[i] == preset)
            idx = i;
    }
    presetDropdown->values = texts;
    presetDropdown->UpdateChoices();
    if (!texts.empty())
        presetDropdown->set_Value(texts[idx]);
}

void InputSettings::OnDisable() {
    ThirdPerPreset preset;
    preset.Position = getConfig().ThirdPerPos.GetValue();
    preset.Rotation = getConfig().ThirdPerRot.GetValue();
    auto presets = getConfig().ThirdPerPresets.GetValue();
    presets[getConfig().ThirdPerPreset.GetValue()] = preset;
    getConfig().ThirdPerPresets.SetValue(presets);
}

void ModSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    modSettingsInstance = this;

    if (!firstActivation)
        return;

    static SafePtrUnity<MainSettings> mainSettings;
    if (!mainSettings)
        mainSettings = BSML::Helpers::CreateViewController<MainSettings*>();
    static SafePtrUnity<RenderSettings> renderSettings;
    if (!renderSettings)
        renderSettings = BSML::Helpers::CreateViewController<RenderSettings*>();
    static SafePtrUnity<InputSettings> inputSettings;
    if (!inputSettings)
        inputSettings = BSML::Helpers::CreateViewController<InputSettings*>();

    showBackButton = true;
    static ConstString title("Replay Settings");
    SetTitle(title, HMUI::ViewController::AnimationType::In);

    ProvideInitialViewControllers(mainSettings.ptr(), renderSettings.ptr(), inputSettings.ptr(), nullptr, nullptr);
}

void ModSettings::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    _parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
    modSettingsInstance = nullptr;
}
