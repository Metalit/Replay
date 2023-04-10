#include "Main.hpp"
#include "Config.hpp"
#include "CustomTypes/ReplaySettings.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include "custom-types/shared/delegate.hpp"
#include "System/Action_2.hpp"
#include "System/Threading/CancellationToken.hpp"
#include "System/Threading/Tasks/Task_1.hpp"
#include "System/Collections/IEnumerator.hpp"
#include "UnityEngine/WaitForSeconds.hpp"

#include "HMUI/Touchable.hpp"
#include "HMUI/TextSegmentedControl.hpp"

#include "UnityEngine/UI/LayoutRebuilder.hpp"
#include "UnityEngine/RectTransform_Axis.hpp"

#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "System/Collections/Generic/Dictionary_2.hpp"

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

template<BeatSaberUI::HasTransform P>
void CreateInputDropdowns(P parent, std::string_view name, std::string_view hint, int buttonValue, int controllerValue, auto buttonCallback, auto controllerCallback) {
    static ConstString labelName("Label");

    auto layout = BeatSaberUI::CreateHorizontalLayoutGroup(parent)->get_transform();
    layout->template GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredHeight(7);

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

template<BeatSaberUI::HasTransform P>
inline UnityEngine::UI::Button* CreateSmallButton(P parent, std::string text, auto callback) {
    auto button = BeatSaberUI::CreateUIButton(parent, text, callback);
    static ConstString contentName("Content");
    UnityEngine::Object::Destroy(button->get_transform()->Find(contentName)->template GetComponent<UnityEngine::UI::LayoutElement*>());
    auto fitter = button->get_gameObject()->template AddComponent<UnityEngine::UI::ContentSizeFitter*>();
    fitter->set_horizontalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
    fitter->set_verticalFit(UnityEngine::UI::ContentSizeFitter::FitMode::PreferredSize);
    return button;
}

std::pair<std::string, std::string> split(const std::string& str, auto delimiter) {
    auto pos = str.find(delimiter);
    return std::make_pair(str.substr(0, pos), str.substr(pos + 1, std::string::npos));
}

template<BeatSaberUI::HasTransform P>
inline void AddConfigValueDropdown(P parent, ConfigUtils::ConfigValue<ButtonPair>& configValue) {
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

template<BeatSaberUI::HasTransform P>
inline void AddConfigValueDropdown(P parent, ConfigUtils::ConfigValue<Button>& configValue) {
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

template<BeatSaberUI::HasTransform P>
inline UnityEngine::Transform* MakeVertical(P parent) {
    auto vertical = BeatSaberUI::CreateVerticalLayoutGroup(parent);
    vertical->set_childControlHeight(false);
    vertical->set_childForceExpandHeight(false);
    vertical->set_spacing(1);
    return vertical->get_transform();
}

inline UnityEngine::Transform* MakeLayout(HMUI::ViewController* self) {
    self->get_gameObject()->AddComponent<HMUI::Touchable*>();
    return MakeVertical(self->get_transform());
}

template<BeatSaberUI::HasTransform P>
HMUI::TextSegmentedControl* MakeTabSelector(P parent, std::vector<std::string> texts, auto callback) {
    static SafePtrUnity<HMUI::TextSegmentedControl> tabSelectorTagTemplate;
    if (!tabSelectorTagTemplate) {
        tabSelectorTagTemplate = UnityEngine::Resources::FindObjectsOfTypeAll<HMUI::TextSegmentedControl*>().FirstOrDefault(
            [](auto x) {
                auto parent = x->get_transform()->get_parent();
                if(!parent) return false;
                if(parent->get_name() != "PlayerStatisticsViewController") return false;
                return x->container != nullptr;
            }
        );
    }

    auto textSegmentedControl = UnityEngine::Object::Instantiate(tabSelectorTagTemplate.ptr(), parent->get_transform(), false);
    textSegmentedControl->dataSource = nullptr;

    auto gameObject = textSegmentedControl->get_gameObject();
    gameObject->set_name("ReplayTabSelector");
    textSegmentedControl->container = tabSelectorTagTemplate->container;

    auto transform = gameObject->template GetComponent<UnityEngine::RectTransform*>();
    transform->set_anchoredPosition({0, 0});
    int childCount = transform->get_childCount();
    for (int i = 1; i <= childCount; i++) {
        UnityEngine::Object::DestroyImmediate(transform->GetChild(childCount - i)->get_gameObject());
    }

    auto textList = List<StringW>::New_ctor(texts.size());
    for(auto text : texts)
        textList->Add(text);
    textSegmentedControl->SetTexts(textList->i_IReadOnlyList_1_T());

    auto delegate = custom_types::MakeDelegate<System::Action_2<HMUI::SegmentedControl*, int>*>(
        (std::function<void(HMUI::SegmentedControl*, int)>) [callback](HMUI::SegmentedControl* _, int selectedIndex) {
            callback(selectedIndex);
        }
    );
    textSegmentedControl->add_didSelectCellEvent(delegate);

    gameObject->SetActive(true);
    return textSegmentedControl;
}

template<BeatSaberUI::HasTransform P>
inline UnityEngine::UI::Toggle* AddConfigValueToggle(P parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = BeatSaberUI::CreateToggle(parent, configValue.GetName(), configValue.GetValue(),
        [&configValue, callback](bool value) {
            configValue.SetValue(value);
            callback(value);
        }
    );
    if(!configValue.GetHoverHint().empty())
        BeatSaberUI::AddHoverHint(object->get_gameObject(), configValue.GetHoverHint());
    ((UnityEngine::RectTransform*) object->get_transform())->set_sizeDelta({18, 8});
    return object;
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

    AddConfigValueToggle(transform, getConfig().Avatar);
}

#include "MenuSelection.hpp"

void RenderSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    auto transform = MakeLayout(this);
    auto graphics = MakeVertical(transform);
    auto rendering = MakeVertical(transform);
    MakeTabSelector(transform, {"Graphics", "Rendering"}, [graphics, rendering](int selected) {
        graphics->get_gameObject()->SetActive(selected == 0);
        rendering->get_gameObject()->SetActive(selected == 1);
    })->get_transform()->SetAsFirstSibling();

    AddConfigValueToggle(graphics, getConfig().Walls);

    // AddConfigValueIncrementEnum(graphics, getConfig().Bloom, fourLevelStrings);

    AddConfigValueIncrementEnum(graphics, getConfig().Mirrors, fourLevelStrings);

    auto shockwaveIncrement = AddConfigValueIncrementInt(graphics, getConfig().Shockwaves, 1, 1, 20)->get_transform()->GetChild(1);
    shockwaveIncrement->get_gameObject()->SetActive(getConfig().ShockwavesOn.GetValue());
    ((UnityEngine::RectTransform*) shockwaveIncrement)->set_anchoredPosition({-20, 0});

    auto shockwaveToggle = AddConfigValueToggle(graphics, getConfig().ShockwavesOn, [shockwaveIncrement](bool enabled) {
        shockwaveIncrement->get_gameObject()->SetActive(enabled);
    })->get_transform();
    auto oldParent = shockwaveToggle->GetParent()->get_gameObject();
    shockwaveToggle->SetParent(shockwaveIncrement->GetParent(), false);
    UnityEngine::Object::Destroy(oldParent);

    AddConfigValueIncrementEnum(graphics, getConfig().Resolution, resolutionStrings);

    AddConfigValueIncrementInt(graphics, getConfig().Bitrate, 5000, 10000, 1000000);

    AddConfigValueIncrementFloat(graphics, getConfig().FOV, 0, 5, 30, 90);

    AddConfigValueIncrementInt(graphics, getConfig().FPS, 5, 5, 120);

    AddConfigValueToggle(rendering, getConfig().CameraOff);

    AddConfigValueToggle(rendering, getConfig().Pauses);

    AddConfigValueToggle(rendering, getConfig().Ding);

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(rendering);

    beginQueueButton = CreateSmallButton(horizontal, "Begin Queue", [this]() {
        RenderLevelInConfig();
        OnEnable();
    });

    clearQueueButton = CreateSmallButton(horizontal, "Clear Queue", [this]() {
        getConfig().LevelsToSelect.SetValue({});
        OnEnable();
    });

    queueList = BeatSaberUI::CreateScrollableList(rendering, {80, 33});
    queueList->set_listStyle(CustomListTableData::List);
    queueList->expandCell = true;
    queueList->tableView->set_selectionType(HMUI::TableViewSelectionType::None);
    OnEnable();

    // for whatever reason, the button is misaligned for a bit, probably the content size fitter
    UnityEngine::UI::LayoutRebuilder::ForceRebuildLayoutImmediate((UnityEngine::RectTransform*) rendering);
    rendering->get_gameObject()->SetActive(false);
}

custom_types::Helpers::Coroutine RenderSettings::GetCoverCoro(IPreviewBeatmapLevel* level) {
    auto result = level->GetCoverImageAsync(System::Threading::CancellationToken::get_None());

    while(!result->get_IsCompleted())
        co_yield (System::Collections::IEnumerator*) UnityEngine::WaitForSeconds::New_ctor(0.2);

    UpdateCover(level, result->get_ResultOnSuccess());
    co_return;
}

void RenderSettings::GetCover(IPreviewBeatmapLevel* level) {
    StartCoroutine(custom_types::Helpers::CoroutineHelper::New(GetCoverCoro(level)));
}

void RenderSettings::UpdateCover(IPreviewBeatmapLevel* level, UnityEngine::Sprite* cover) {
    if(!queueList || !get_enabled())
        return;
    auto sprite = *il2cpp_utils::GetFieldValue<UnityEngine::Sprite*>(level, "_coverImage");
    auto levels = getConfig().LevelsToSelect.GetValue();
    for(int i = 0; i < levels.size(); i++) {
        if(levels[i].ID == level->get_levelID())
            queueList->data[i].icon = cover;
    }
    queueList->tableView->ReloadData();
}

void RenderSettings::OnEnable() {
    auto levels = getConfig().LevelsToSelect.GetValue();
    bool empty = levels.empty();
    if(beginQueueButton)
        beginQueueButton->set_interactable(!empty);
    if(clearQueueButton)
        clearQueueButton->set_interactable(!empty);
    if(queueList) {
        queueList->data.clear();
        for(auto& level : levels) {
            auto levelPreview = BeatSaberUI::GetMainFlowCoordinator()->beatmapLevelsModel->loadedPreviewBeatmapLevels->get_Item(level.ID);
            std::string name = levelPreview->get_songName();
            std::string author = levelPreview->get_songAuthorName();
            std::string mapper = levelPreview->get_levelAuthorName();
            std::string subtext = author + " [" + mapper + "] - Replay Index " + std::to_string(level.ReplayIndex + 1);
            auto sprite = *il2cpp_utils::GetFieldValue<UnityEngine::Sprite*>(levelPreview, "_coverImage");
            if(!sprite)
                GetCover(levelPreview);
            queueList->data.emplace_back(name, subtext, sprite);
        }
        queueList->tableView->ReloadData();
    }
}

void RenderSettings::OnDisable() {
    StopAllCoroutines();
}

void InputSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    auto transform = MakeLayout(this);
    auto inputs = MakeVertical(transform);
    auto position = MakeVertical(transform);
    MakeTabSelector(transform, {"Inputs", "Position"}, [inputs, position](int selected) {
        inputs->get_gameObject()->SetActive(selected == 0);
        position->get_gameObject()->SetActive(selected == 1);
    })->get_transform()->SetAsFirstSibling();

    AddConfigValueDropdown(inputs, getConfig().TimeButton);

    AddConfigValueIncrementInt(inputs, getConfig().TimeSkip, 1, 1, 30);

    AddConfigValueDropdown(inputs, getConfig().SpeedButton);

    AddConfigValueDropdown(inputs, getConfig().MoveButton);

    AddConfigValueDropdown(inputs, getConfig().TravelButton);

    AddConfigValueIncrementFloat(inputs, getConfig().TravelSpeed, 1, 0.1, 0.2, 5);

    auto positionSettings = AddConfigValueIncrementVector3(position, getConfig().ThirdPerPos, 1, 0.1);

    CreateSmallButton(position, "Reset Position", [positionSettings]() {
        auto def = getConfig().ThirdPerPos.GetDefaultValue();
        positionSettings[0]->CurrentValue = def.x;
        positionSettings[0]->UpdateValue();
        positionSettings[1]->CurrentValue = def.y;
        positionSettings[1]->UpdateValue();
        positionSettings[2]->CurrentValue = def.z;
        positionSettings[2]->UpdateValue();
    });

    auto rotationSettings = AddConfigValueIncrementVector3(position, getConfig().ThirdPerRot, 0, 1);

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(position);

    CreateSmallButton(horizontal, "Level Rotation", [rotationSettings]() {
        rotationSettings[0]->CurrentValue = 0;
        rotationSettings[0]->UpdateValue();
    });

    CreateSmallButton(horizontal, "Reset Rotation", [rotationSettings]() {
        auto def = getConfig().ThirdPerRot.GetDefaultValue();
        rotationSettings[0]->CurrentValue = def.x;
        rotationSettings[0]->UpdateValue();
        rotationSettings[1]->CurrentValue = def.y;
        rotationSettings[1]->UpdateValue();
        rotationSettings[2]->CurrentValue = def.z;
        rotationSettings[2]->UpdateValue();
    });

    position->get_gameObject()->SetActive(false);
}

#include "HMUI/ViewController_AnimationType.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"

void ModSettings::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;

    static SafePtrUnity<MainSettings> mainSettings;
    if(!mainSettings)
        mainSettings = BeatSaberUI::CreateViewController<MainSettings*>();
    static SafePtrUnity<RenderSettings> renderSettings;
    if(!renderSettings)
        renderSettings = BeatSaberUI::CreateViewController<RenderSettings*>();
    static SafePtrUnity<InputSettings> inputSettings;
    if(!inputSettings)
        inputSettings = BeatSaberUI::CreateViewController<InputSettings*>();

    showBackButton = true;
    static ConstString title("Replay Settings");
    SetTitle(title, HMUI::ViewController::AnimationType::In);

    ProvideInitialViewControllers(mainSettings.ptr(), renderSettings.ptr(), inputSettings.ptr(), nullptr, nullptr);
}

void ModSettings::BackButtonWasPressed(HMUI::ViewController* topViewController) {
    parentFlowCoordinator->DismissFlowCoordinator(this, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
}
