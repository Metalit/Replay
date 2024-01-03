#include "Main.hpp"
#include "CustomTypes/ReplayMenu.hpp"

#include "Formats/FrameReplay.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "Sprites.hpp"
#include "Config.hpp"
#include "MenuSelection.hpp"
#include "CustomTypes/ReplaySettings.hpp"

#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "GlobalNamespace/MultiplayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"
#include "VRUIControls/VRGraphicRaycaster.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#include <filesystem>

DEFINE_TYPE(Menu, ReplayViewController);

using namespace GlobalNamespace;
using namespace QuestUI;

UnityEngine::GameObject* canvas;
Menu::ReplayViewController* viewController;
StandardLevelDetailView* levelView;
bool usingLocalReplays = true;

void OnReplayButtonClick() {
    if(!usingLocalReplays)
        Manager::RefreshLevelReplays();
    Menu::PresentMenu();
}

void OnWatchButtonClick() {
    Manager::Camera::rendering = false;
    Manager::ReplayStarted(viewController->GetReplay());
    levelView->actionButton->get_onClick()->Invoke();
}

void OnCameraModeSet(StringW value) {
    for(int i = 0; i < cameraModes.size(); i++) {
        if(cameraModes[i] == value) {
            getConfig().CamMode.SetValue(i);
            return;
        }
    }
}

void OnRenderButtonClick() {
    Manager::Camera::rendering = true;
    Manager::ReplayStarted(viewController->GetReplay());
    levelView->actionButton->get_onClick()->Invoke();
}

void OnQueueButtonClick() {
    if (IsCurrentLevelInConfig())
        RemoveCurrentLevelFromConfig();
    else
        SaveCurrentLevelInConfig();
    viewController->UpdateUI();
}

void OnDeleteButtonClick() {
    if(!usingLocalReplays)
        return;
    try {
        std::filesystem::remove(viewController->GetReplay());
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR("Failed to delete replay: {}", e.what());
    }
    Manager::RefreshLevelReplays();
}

void OnSettingsButtonClick() {
    static SafePtrUnity<HMUI::FlowCoordinator> settings;
    if(!settings)
        settings = (HMUI::FlowCoordinator*) BeatSaberUI::CreateFlowCoordinator<ReplaySettings::ModSettings*>();
    auto flow = BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    flow->PresentFlowCoordinator(settings.ptr(), nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false, false);
}

void OnIncrementChanged(float value) {
    viewController->SelectReplay(value - 1);
}

custom_types::Helpers::Coroutine MatchRequirementsCoroutine(UnityEngine::UI::Button* replayButton) {
    co_yield nullptr;

    bool interactable = levelView->actionButton->get_interactable() || levelView->practiceButton->get_interactable();
    replayButton->set_interactable(interactable && replayButton->get_interactable());
    co_return;
}

namespace Menu {
    void EnsureSetup(StandardLevelDetailView* view) {
        static ConstString canvasName("ReplayButtonCanvas");
        levelView = view;

        auto parent = view->practiceButton->get_transform()->GetParent();
        auto canvasTransform = (UnityEngine::RectTransform*) parent->Find(canvasName);

        if(!canvasTransform) {
            LOG_INFO("Making button canvas");
            canvas = BeatSaberUI::CreateCanvas();
            canvasTransform = (UnityEngine::RectTransform*) canvas->get_transform();
            canvasTransform->set_name(canvasName);
            canvasTransform->SetParent(parent, false);
            canvasTransform->set_localScale({1, 1, 1});
            canvasTransform->set_sizeDelta({10, 10});
            canvasTransform->set_anchoredPosition({0, 0});
            canvasTransform->SetAsLastSibling();
            auto canvasLayout = canvas->AddComponent<UnityEngine::UI::LayoutElement*>();
            canvasLayout->set_preferredWidth(10);

            auto replayButton = BeatSaberUI::CreateUIButton(canvasTransform, "", OnReplayButtonClick);
            replayButton->get_gameObject()->SetActive(true);
            auto buttonTransform = (UnityEngine::RectTransform*) replayButton->get_transform();
            UnityEngine::Object::Destroy(buttonTransform->Find("Content")->GetComponent<UnityEngine::UI::LayoutElement*>());
            auto icon = BeatSaberUI::CreateImage(buttonTransform, GetReplayIcon());
            icon->get_transform()->set_localScale({0.6, 0.6, 0.6});
            icon->set_preserveAspect(true);
            buttonTransform->set_anchoredPosition({0, 0});
            buttonTransform->set_sizeDelta({10, 10});

            ((UnityEngine::RectTransform*) parent)->set_anchoredPosition({4.2, -55});

            viewController = BeatSaberUI::CreateViewController<ReplayViewController*>();
            viewController->set_name("ReplayViewController");
        }

        auto replayButton = canvasTransform->GetComponentInChildren<UnityEngine::UI::Button*>();
        SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(MatchRequirementsCoroutine(replayButton)));
        if(auto hint = replayButton->GetComponent<HMUI::HoverHint*>())
            hint->set_text("");
    }

    void SetButtonEnabled(bool enabled) {
        canvas->SetActive(enabled);
        canvas->GetComponentInChildren<UnityEngine::UI::Button*>()->set_interactable(true);
        float xpos = enabled ? 4.2 : -1.8;
        ((UnityEngine::RectTransform*) canvas->get_transform()->GetParent())->set_anchoredPosition({xpos, -55});
    }

    void SetButtonUninteractable(std::string_view reason) {
        auto replayButton = canvas->GetComponentInChildren<UnityEngine::UI::Button*>();
        replayButton->set_interactable(false);
        if(auto hint = replayButton->GetComponent<HMUI::HoverHint*>())
            hint->set_text(reason);
        else
            BeatSaberUI::AddHoverHint(replayButton, reason);
    }

    void CheckMultiplayer() {
        auto flowCoordinator = BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        auto klass = il2cpp_functions::object_get_class((Il2CppObject*) flowCoordinator);
        if(il2cpp_functions::class_is_assignable_from(classof(GlobalNamespace::MultiplayerLevelSelectionFlowCoordinator*), klass))
            SetButtonEnabled(false);
    }

    void SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replays, bool external) {
        usingLocalReplays = !external;
        viewController->SetReplays(replays);
    }

    void PresentMenu() {
        auto flowCoordinator = BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        flowCoordinator->showBackButton = true;
        flowCoordinator->PresentViewController(viewController, nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false);
        viewController->UpdateUI();
    }
    void DismissMenu() {
        if(viewController->get_isInViewControllerHierarchy() && !viewController->childViewController) {
            auto flowCoordinator = BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
            flowCoordinator->DismissViewController(viewController, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
        }
    }

    bool AreReplaysLocal() {
        return usingLocalReplays;
    }
}

void SetPreferred(auto* object, std::optional<float> width, std::optional<float> height) {
    UnityEngine::UI::LayoutElement* layout = object->template GetComponent<UnityEngine::UI::LayoutElement*>();
    if(!layout)
        layout = object->get_gameObject()->template AddComponent<UnityEngine::UI::LayoutElement*>();
    if(width.has_value())
        layout->set_preferredWidth(*width);
    if(height.has_value())
        layout->set_preferredHeight(*height);
}

TMPro::TextMeshProUGUI* CreateCenteredText(UnityEngine::UI::HorizontalLayoutGroup* parent) {
    auto text = BeatSaberUI::CreateText(parent->get_transform(), "");
    text->set_fontSize(4.5);
    text->set_alignment(TMPro::TextAlignmentOptions::Center);
    text->set_lineSpacing(-35);
    SetPreferred(text, 40, 15);
    return text;
}

std::string GetLayeredText(const std::string& label, const std::string& text, bool newline = true) {
    return fmt::format("<i><uppercase><color=#bdbdbd>{}</color></uppercase>{}{}</i>", label, newline ? "\n" : "", text);
}

inline UnityEngine::UI::Toggle* AddConfigValueToggle(UnityEngine::Transform* parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
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

void Menu::ReplayViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation) {
        cameraDropdown->SelectCellWithIdx(getConfig().CamMode.GetValue());
        return;
    }

    using namespace UnityEngine;

    auto mainLayout = BeatSaberUI::CreateVerticalLayoutGroup(get_transform());
    mainLayout->set_childControlWidth(true);
    mainLayout->set_childForceExpandWidth(true);
    mainLayout->set_childControlHeight(false);
    mainLayout->set_childForceExpandHeight(false);
    SetPreferred(mainLayout, 80, std::nullopt);

    auto levelBarTemplate = Resources::FindObjectsOfTypeAll<LevelBar*>().First([](LevelBar* x) {
        return x->get_transform()->GetParent()->get_name() == "PracticeViewController";
    });
    levelBar = Object::Instantiate(levelBarTemplate->get_gameObject(), mainLayout->get_transform())->GetComponent<LevelBar*>();
    levelBar->set_name("ReplayLevelBarSimple");
    SetPreferred(levelBar, std::nullopt, 20);

    sourceText = BeatSaberUI::CreateText(mainLayout, "");
    sourceText->set_fontSize(4.5);
    sourceText->set_alignment(TMPro::TextAlignmentOptions::Center);

    auto horizontal1 = BeatSaberUI::CreateHorizontalLayoutGroup(mainLayout);

    dateText = CreateCenteredText(horizontal1);
    modifiersText = CreateCenteredText(horizontal1);

    auto horizontal2 = BeatSaberUI::CreateHorizontalLayoutGroup(mainLayout);

    scoreText = CreateCenteredText(horizontal2);
    failText = CreateCenteredText(horizontal2);

    auto horizontal3 = BeatSaberUI::CreateHorizontalLayoutGroup(mainLayout);
    horizontal3->set_spacing(5);

    watchButton = BeatSaberUI::CreateUIButton(horizontal3, "Watch Replay", "ActionButton", Vector2(), Vector2(0, 10), OnWatchButtonClick);
    renderButton = BeatSaberUI::CreateUIButton(horizontal3,  "Render Replay", Vector2(), Vector2(0, 10), OnRenderButtonClick);

    auto horizontal4 = BeatSaberUI::CreateHorizontalLayoutGroup(mainLayout);
    horizontal4->set_spacing(5);

    cameraDropdown = AddConfigValueDropdownEnum(horizontal4, getConfig().CamMode, cameraModes);
    cameraDropdown->get_transform()->GetParent()->Find("Label")->GetComponent<TMPro::TextMeshProUGUI*>()->SetText("");
    SetPreferred(cameraDropdown->get_transform()->GetParent(), 30, 8);

    auto container = UnityEngine::GameObject::New_ctor("SmallerButtonContainer");
    container->get_transform()->SetParent(horizontal4->get_transform(), false);
    SetPreferred(container, 30, 8);
    auto queueText = IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue";
    queueButton = BeatSaberUI::CreateUIButton(container, queueText, Vector2(-2.8, 0), Vector2(33, 8), OnQueueButtonClick);
    queueButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_fontStyle(TMPro::FontStyles::Italic);
    queueButton->set_interactable(usingLocalReplays);
    static ConstString contentName("Content");
    UnityEngine::Object::Destroy(queueButton->get_transform()->Find(contentName)->template GetComponent<UnityEngine::UI::LayoutElement*>());

    auto settingsButton = BeatSaberUI::CreateUIButton(get_transform(), "", Vector2(-48, -22), Vector2(10, 10), OnSettingsButtonClick);
    UnityEngine::Object::Destroy(settingsButton->get_transform()->Find("Content")->GetComponent<UI::LayoutElement*>());
    auto settigsIcon = BeatSaberUI::CreateImage(settingsButton, GetSettingsIcon());
    settigsIcon->get_transform()->set_localScale({0.8, 0.8, 0.8});
    settigsIcon->set_preserveAspect(true);

    deleteButton = BeatSaberUI::CreateUIButton(get_transform(), "", Vector2(48, -22), Vector2(10, 10), [this]() {
        confirmModal->Show(true, true, nullptr);
    });
    UnityEngine::Object::Destroy(deleteButton->get_transform()->Find("Content")->GetComponent<UI::LayoutElement*>());
    auto deleteIcon = BeatSaberUI::CreateImage(deleteButton, GetDeleteIcon());
    deleteIcon->get_transform()->set_localScale({0.8, 0.8, 0.8});
    deleteIcon->set_preserveAspect(true);
    deleteButton->get_gameObject()->SetActive(usingLocalReplays);

    increment = BeatSaberUI::CreateIncrementSetting(mainLayout, "", 0, 1, getConfig().LastReplayIdx.GetValue() + 1, 1, replays.size(), OnIncrementChanged);
    Object::Destroy(increment->GetComponent<UI::HorizontalLayoutGroup*>());
    ((RectTransform*) increment->get_transform()->GetChild(1))->set_anchoredPosition({-20, 0});

    confirmModal = BeatSaberUI::CreateModal(get_transform(), {58, 24}, nullptr);

    static ConstString dialogText("Are you sure you would like to delete this\nreplay? This cannot be undone.");

    auto removeText = BeatSaberUI::CreateText(confirmModal->get_transform(), dialogText, false, {0, 5});
    removeText->set_alignment(TMPro::TextAlignmentOptions::Center);

    auto confirmButton = BeatSaberUI::CreateUIButton(confirmModal->get_transform(), "Delete", "ActionButton", {11.5, -6}, {20, 10}, [this] {
        confirmModal->Hide(true, nullptr);
        OnDeleteButtonClick();
    });
    Object::Destroy(confirmButton->get_transform()->Find(contentName)->GetComponent<UI::LayoutElement*>());
    auto cancelButton = BeatSaberUI::CreateUIButton(confirmModal->get_transform(), "Cancel", {-11.5, -6}, {20, 10}, [this] {
        confirmModal->Hide(true, nullptr);
    });
    Object::Destroy(cancelButton->get_transform()->Find(contentName)->GetComponent<UI::LayoutElement*>());
}

void Menu::ReplayViewController::OnEnable() {
    if(queueButton) {
        queueButton->set_interactable(usingLocalReplays);
        BeatSaberUI::SetButtonText(queueButton, IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue");
    }
}

void Menu::ReplayViewController::SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replayInfos) {
    replays = replayInfos;
    if(getConfig().LastReplayIdx.GetValue() >= replayInfos.size())
        getConfig().LastReplayIdx.SetValue(replayInfos.size() - 1);
    auto lastBeatmap = beatmap;
    beatmap = levelView->selectedDifficultyBeatmap;
    if(lastBeatmap != beatmap) {
        beatmapData = nullptr;
        GetBeatmapData(beatmap, [this, replayInfos](IReadonlyBeatmapData* data) {
            beatmapData = data;
            if(replays == replayInfos && increment) {
                UpdateUI();
            }
        });
    }
    if(increment) {
        increment->MaxValue = replayInfos.size();
        increment->CurrentValue = getConfig().LastReplayIdx.GetValue() + 1;
        increment->UpdateValue();
    }
}

void Menu::ReplayViewController::SelectReplay(int index) {
    getConfig().LastReplayIdx.SetValue(index);
    UpdateUI();
}

std::string& Menu::ReplayViewController::GetReplay() {
    return replays[getConfig().LastReplayIdx.GetValue()].first;
}

void Menu::ReplayViewController::UpdateUI() {
    auto beatmap = levelView->get_selectedDifficultyBeatmap();
    auto level = (IPreviewBeatmapLevel*) beatmap->get_level();
    auto characteristic = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic();
    auto difficulty = beatmap->get_difficulty();
    levelBar->Setup(level, characteristic, difficulty);
    float songLength = level->get_songDuration();

    auto info = replays[getConfig().LastReplayIdx.GetValue()].second;

    sourceText->set_text(GetLayeredText("Replay Source:  ", info->source, false));
    std::string date = GetStringForTimeSinceNow(info->timestamp);
    std::string modifiers = GetModifierString(info->modifiers, info->reached0Energy);
    std::string score = std::to_string(info->score);
    if(beatmapData) {
        float percent = info->score * 100.0f / ScoreModel::ComputeMaxMultipliedScoreForBeatmap(beatmapData);
        score = fmt::format("{} <size=80%>(<color=#1dbcd1>{:.2f}%</color>)</size>", info->score, percent);
    }
    std::string fail = info->failed ? "<color=#cc1818>True</color>" : "<color=#2adb44>False</color>";
    if(info->failed && info->failTime > 0.001)
        fail = fmt::format("<color=#cc1818>{}</color> / {}", SecondsToString(info->failTime), SecondsToString(songLength));

    dateText->set_text(GetLayeredText("Date Played", date));
    modifiersText->set_text(GetLayeredText("Modifiers", modifiers));
    scoreText->set_text(GetLayeredText("Score", score));
    failText->set_text(GetLayeredText("Failed", fail));

    queueButton->set_interactable(usingLocalReplays);
    BeatSaberUI::SetButtonText(queueButton, IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue");

    deleteButton->get_gameObject()->SetActive(usingLocalReplays);

    auto buttons = increment->get_transform()->GetChild(1)->GetComponentsInChildren<UnityEngine::UI::Button*>();
    buttons.First()->set_interactable(getConfig().LastReplayIdx.GetValue() + 1 != increment->MinValue);
    buttons.Last()->set_interactable(getConfig().LastReplayIdx.GetValue() + 1 != increment->MaxValue);

    increment->get_gameObject()->SetActive(increment->MinValue != increment->MaxValue);
}
