#include "CustomTypes/ReplaySettings.hpp"

#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/IPreviewMediaData.hpp"
#include "HMUI/TextSegmentedControl.hpp"
#include "HMUI/Touchable.hpp"
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
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/delegates.hpp"
#include "metacore/shared/ui.hpp"
#include "utils.hpp"

DEFINE_TYPE(Replay, MainSettings)
DEFINE_TYPE(Replay, RenderSettings)
DEFINE_TYPE(Replay, InputSettings)
DEFINE_TYPE(Replay, ModSettings)

using namespace GlobalNamespace;
using namespace Replay;

MainSettings* MainSettings::instance = nullptr;
RenderSettings* RenderSettings::instance = nullptr;
InputSettings* InputSettings::instance = nullptr;

static std::vector<std::string_view> const ButtonNames = {
    "None", "Side Trigger", "Front Trigger", "A/X Button", "B/Y Button", "Joystick Up", "Joystick Down", "Joystick Left", "Joystick Right"
};
static std::vector<std::string_view> const ControllerNames = {"Left", "Right", "Both"};
static std::vector<std::string> const ResolutionStrings = {"640 x 480", "1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160"};
static std::vector<std::string> const WallStrings = {"Transparent", "Textured", "Distorted"};
static std::vector<std::string> const MirrorStrings = {"Off", "Low", "Medium", "High"};
static std::vector<std::string> const AAStrings = {"0", "2", "4", "8"};

template <class T>
static inline T* LazyCreate(T*& instance, char const* name) {
    if (!instance) {
        instance = BSML::Helpers::CreateViewController<T*>();
        instance->name = name;
    }
    return instance;
}

static void CreateInputDropdowns(
    BSML::Lite::TransformWrapper parent,
    std::string name,
    std::string hint,
    int buttonValue,
    int controllerValue,
    std::function<void(int)> buttonCallback,
    std::function<void(int)> controllerCallback
) {
    static ConstString labelName("Label");

    auto layout = BSML::Lite::CreateHorizontalLayoutGroup(parent);
    layout->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredHeight = 7;

    BSML::DropdownListSetting* dropdown = MetaCore::UI::CreateDropdownEnum(layout, name, buttonValue, ButtonNames, buttonCallback);
    BSML::Lite::AddHoverHint(dropdown, hint);
    dropdown->GetComponent<UnityEngine::RectTransform*>()->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, 30);

    auto dropdownParent = dropdown->transform->parent;
    dropdownParent->Find(labelName)->GetComponent<UnityEngine::RectTransform*>()->anchorMax = {2, 1};
    dropdownParent->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 53;

    dropdown = MetaCore::UI::CreateDropdownEnum(layout, "  Controller", controllerValue, ControllerNames, controllerCallback);
    BSML::Lite::AddHoverHint(dropdown, hint);
    dropdown->GetComponent<UnityEngine::RectTransform*>()->SetSizeWithCurrentAnchors(UnityEngine::RectTransform::Axis::Horizontal, 22);

    dropdownParent = dropdown->transform->parent;
    dropdownParent->Find(labelName)->GetComponent<UnityEngine::RectTransform*>()->anchorMax = {2, 1};
    dropdownParent->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = 38;
}

static inline UnityEngine::UI::Button* CreateSmallButton(BSML::Lite::TransformWrapper parent, std::string text, auto callback) {
    auto button = BSML::Lite::CreateUIButton(parent, text, callback);
    auto fitter = button->gameObject->template AddComponent<UnityEngine::UI::ContentSizeFitter*>();
    fitter->horizontalFit = UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize;
    fitter->verticalFit = UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize;
    return button;
}

static std::pair<std::string, std::string> split(std::string const& str, auto delimiter) {
    auto pos = str.find(delimiter);
    return std::make_pair(str.substr(0, pos), str.substr(pos + 1, std::string::npos));
}

static void AddConfigValueDropdown(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<ButtonPair>& configValue) {
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

static void AddConfigValueDropdown(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<Button>& configValue) {
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

static BSML::ToggleSetting* AddConfigValueToggle(BSML::Lite::TransformWrapper parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = BSML::Lite::CreateToggle(parent, configValue.GetName(), configValue.GetValue(), [&configValue, callback](bool value) mutable {
        configValue.SetValue(value);
        callback(value);
    });
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object->gameObject, configValue.GetHoverHint());
    object->template GetComponent<UnityEngine::RectTransform*>()->sizeDelta = {18, 8};
    return object;
}

static inline UnityEngine::Transform* MakeVertical(BSML::Lite::TransformWrapper parent) {
    auto vertical = BSML::Lite::CreateVerticalLayoutGroup(parent);
    vertical->childControlHeight = false;
    vertical->childForceExpandHeight = false;
    vertical->spacing = 1;
    return vertical->transform;
}

static inline UnityEngine::Transform* MakeLayout(HMUI::ViewController* self, float verticalOffset = 0) {
    self->gameObject->AddComponent<HMUI::Touchable*>();
    auto ret = MakeVertical(self);
    if (verticalOffset != 0)
        self->rectTransform->sizeDelta = {0, verticalOffset * 2};
    return ret;
}

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

void MainSettings::OnDestroy() {
    instance = nullptr;
}

MainSettings* MainSettings::GetInstance() {
    return LazyCreate(instance, "ReplayMainSettings");
}

void RenderSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    auto transform = MakeLayout(this, 8);
    auto graphics = MakeVertical(transform);
    auto rendering = MakeVertical(transform);
    MetaCore::UI::CreateTextSegmentedControl(transform, {"Graphics", "Rendering"}, [graphics, rendering](int selected) {
        graphics->gameObject->active = selected == 0;
        rendering->gameObject->active = selected == 1;
    })->transform->SetAsFirstSibling();

    AddConfigValueIncrementEnum(graphics, getConfig().Walls, WallStrings);

    AddConfigValueToggle(graphics, getConfig().Bloom);

    AddConfigValueIncrementEnum(graphics, getConfig().Mirrors, MirrorStrings);

    AddConfigValueIncrementEnum(graphics, getConfig().AntiAlias, AAStrings);

    auto shockwaveIncrement = AddConfigValueIncrementInt(graphics, getConfig().Shockwaves, 1, 1, 20);
    auto incrementObject = shockwaveIncrement->transform->GetChild(1)->gameObject;
    incrementObject->active = getConfig().ShockwavesOn.GetValue();
    incrementObject->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {-20, 0};

    auto shockwaveToggle =
        AddConfigValueToggle(graphics, getConfig().ShockwavesOn, [incrementObject](bool enabled) mutable { incrementObject->active = enabled; });
    shockwaveToggle->toggle->transform->SetParent(shockwaveIncrement->transform, false);
    shockwaveToggle->transform->SetParent(shockwaveIncrement->transform, false);
    UnityEngine::Object::Destroy(shockwaveToggle->text->gameObject);

    AddConfigValueIncrementEnum(graphics, getConfig().Resolution, ResolutionStrings);

    auto bitrateSlider = AddConfigValueSliderIncrement(graphics, getConfig().Bitrate, 2500, 2500, 25000);
    bitrateSlider->isInt = true;
    bitrateSlider->text->text = bitrateSlider->TextForValue(bitrateSlider->get_Value());

    AddConfigValueIncrementFloat(graphics, getConfig().FOV, 0, 5, 30, 90);

    AddConfigValueIncrementInt(graphics, getConfig().FPS, 5, 5, 120);

    AddConfigValueToggle(rendering, getConfig().Pauses);

    AddConfigValueToggle(rendering, getConfig().Ding);

    AddConfigValueToggle(rendering, getConfig().HEVC);

    auto horizontal = BSML::Lite::CreateHorizontalLayoutGroup(rendering);

    beginQueueButton = CreateSmallButton(horizontal, "Begin Queue", [this]() {
        Manager::BeginQueue();
        OnEnable();
    });

    clearQueueButton = CreateSmallButton(horizontal, "Clear Queue", [this]() {
        getConfig().LevelsToSelect.SetValue({});
        OnEnable();
    });

    queueList = BSML::Lite::CreateScrollableList(rendering, {90, 45}, [this](int idx) {
        queueList->tableView->ClearSelection();
        Manager::SelectLevelInConfig(idx);
    });
    queueList->listStyle = BSML::CustomListTableData::ListStyle::List;
    queueList->expandCell = true;
    OnEnable();

    // for whatever reason, the button is misaligned for a bit, probably the content size fitter
    UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate((UnityEngine::RectTransform*) rendering);
    rendering->gameObject->active = false;
}

custom_types::Helpers::Coroutine RenderSettings::GetCoverCoro(BeatmapLevel* level) {
    auto result = level->previewMediaData->GetCoverSpriteAsync();

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

static void AddCoverlessCell(BSML::CustomListTableData* list, LevelSelection const& level, BeatmapLevel* beatmap) {
    std::string name = beatmap->songName;
    std::string difficulty = Utils::GetDifficultyName(level.Difficulty);
    std::string characteristic = Utils::GetCharacteristicName(level.Characteristic);
    std::string toptext = "<voffset=0.1em>" + name + "  <size=75%><color=#D6D6D6>" + characteristic + " " + difficulty;
    std::string author = beatmap->songAuthorName;
    std::string mapper = "";
    if (!beatmap->allMappers.Empty())
        mapper = fmt::format(" [{}]", beatmap->allMappers->First());
    std::string subtext = fmt::format("{}{} - Replay Index {}", author, mapper, level.ReplayIndex + 1);
    list->data->Add(BSML::CustomCellInfo::construct(toptext, subtext, nullptr));
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
        for (auto& level : levels) {
            auto beatmap = BSML::Helpers::GetMainFlowCoordinator()->_beatmapLevelsModel->GetBeatmapLevel(level.ID);
            if (!beatmap) {
                queueList->data->Add(BSML::CustomCellInfo::construct("Couldn't load level", "Exit and reopen settings to try again", nullptr));
                continue;
            }
            AddCoverlessCell(queueList, level, beatmap);
            GetCover(beatmap);
        }
        queueList->tableView->ReloadData();
    }
}

void RenderSettings::OnDisable() {
    StopAllCoroutines();
}

void RenderSettings::OnDestroy() {
    instance = nullptr;
}

RenderSettings* RenderSettings::GetInstance() {
    return LazyCreate(instance, "ReplayRenderSettings");
}

void InputSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    auto transform = MakeLayout(this, 8);
    auto inputs = MakeVertical(transform);
    auto position = MakeVertical(transform);
    MetaCore::UI::CreateTextSegmentedControl(transform, {"Inputs", "Position"}, [inputs, position](int selected) {
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
        MetaCore::UI::AddIncrementIncrement(setting, 1);

    CreateSmallButton(position, "Reset Position", [this]() {
        getConfig().ThirdPerPos.SetValue(getConfig().ThirdPerPos.GetDefaultValue());
        OnEnable();
    });

    rotationSettings = AddConfigValueIncrementVector3(position, getConfig().ThirdPerRot, 0, 1);
    for (auto& setting : rotationSettings)
        MetaCore::UI::AddIncrementIncrement(setting, 10);

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
    presetDropdown = BSML::Lite::CreateDropdown(layout, "Preset", getConfig().CurrentThirdPerPreset.GetValue(), presetNames, [this](StringW name) {
        OnDisable();
        getConfig().CurrentThirdPerPreset.SetValue(name);
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
        presets.erase(getConfig().CurrentThirdPerPreset.GetValue());
        getConfig().ThirdPerPresets.SetValue(presets);
        getConfig().CurrentThirdPerPreset.SetValue(presets.begin()->first);
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
    MetaCore::UI::AddStringSettingOnClose(nameInput, nullptr, [this]() {
        std::string val = nameInput->text;
        if (val.empty())
            return;
        nameModal->Hide(true, nullptr);
        OnDisable();  // save current preset
        getConfig().CurrentThirdPerPreset.SetValue(val);
        ThirdPerPreset preset;
        preset.Position = getConfig().ThirdPerPos.GetDefaultValue();
        preset.Rotation = getConfig().ThirdPerRot.GetDefaultValue();
        getConfig().ThirdPerPos.SetValue(preset.Position);
        getConfig().ThirdPerRot.SetValue(preset.Rotation);
        auto presets = getConfig().ThirdPerPresets.GetValue();
        presets[val] = preset;
        getConfig().ThirdPerPresets.SetValue(presets);
        OnEnable();
    });

    position->gameObject->active = false;
}

void InputSettings::OnEnable() {
    if (!positionSettings[0] || !rotationSettings[0] || !removePresetButton || !presetDropdown)
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
    // update presets dropdown
    auto presets = getConfig().ThirdPerPresets.GetValue();
    auto preset = getConfig().CurrentThirdPerPreset.GetValue();
    std::vector<std::string> presetNames;
    for (auto& entry : presets)
        presetNames.emplace_back(entry.first);
    MetaCore::UI::SetDropdownValues(presetDropdown, presetNames, preset);
}

void InputSettings::OnDisable() {
    ThirdPerPreset preset;
    preset.Position = getConfig().ThirdPerPos.GetValue();
    preset.Rotation = getConfig().ThirdPerRot.GetValue();
    auto presets = getConfig().ThirdPerPresets.GetValue();
    presets[getConfig().CurrentThirdPerPreset.GetValue()] = preset;
    getConfig().ThirdPerPresets.SetValue(presets);
}

void InputSettings::OnDestroy() {
    instance = nullptr;
}

InputSettings* InputSettings::GetInstance() {
    return LazyCreate(instance, "ReplayInputSettings");
}

void ModSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation)
        return;

    showBackButton = true;
    static ConstString title("Replay Settings");
    SetTitle(title, HMUI::ViewController::AnimationType::In);

    ProvideInitialViewControllers(MainSettings::GetInstance(), RenderSettings::GetInstance(), InputSettings::GetInstance(), nullptr, nullptr);
}

void ModSettings::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    _parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
}
