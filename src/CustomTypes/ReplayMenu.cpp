#include "CustomTypes/ReplayMenu.hpp"

#include "Config.hpp"
#include "CustomTypes/ReplaySettings.hpp"
#include "Formats/FrameReplay.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "GlobalNamespace/MultiplayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "Main.hpp"
#include "MenuSelection.hpp"
#include "ReplayManager.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "Utils.hpp"
#include "VRUIControls/VRGraphicRaycaster.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/strings.hpp"

DEFINE_TYPE(Menu, ReplayViewController);

using namespace GlobalNamespace;

UnityEngine::GameObject* canvas;
Menu::ReplayViewController* viewController;
StandardLevelDetailView* levelView;
bool usingLocalReplays = true;

void OnReplayButtonClick() {
    if (!usingLocalReplays)
        Manager::RefreshLevelReplays();
    Menu::PresentMenu();
}

void OnWatchButtonClick() {
    Manager::Camera::rendering = false;
    Manager::ReplayStarted(viewController->GetReplay());
    levelView->actionButton->onClick->Invoke();
}

void OnCameraModeSet(StringW value) {
    for (int i = 0; i < cameraModes.size(); i++) {
        if (cameraModes[i] == value) {
            getConfig().CamMode.SetValue(i);
            return;
        }
    }
}

void OnRenderButtonClick() {
    Manager::Camera::rendering = true;
    Manager::ReplayStarted(viewController->GetReplay());
    levelView->actionButton->onClick->Invoke();
}

void OnQueueButtonClick() {
    if (IsCurrentLevelInConfig())
        RemoveCurrentLevelFromConfig();
    else
        SaveCurrentLevelInConfig();
    viewController->UpdateUI(false);
}

void OnDeleteButtonClick() {
    if (!usingLocalReplays)
        return;
    try {
        std::filesystem::remove(viewController->GetReplay());
    } catch (std::filesystem::filesystem_error const& e) {
        LOG_ERROR("Failed to delete replay: {}", e.what());
    }
    Manager::RefreshLevelReplays();
}

void OnSettingsButtonClick() {
    static UnityW<HMUI::FlowCoordinator> settings;
    if (!settings)
        settings = (HMUI::FlowCoordinator*) BSML::Helpers::CreateFlowCoordinator<ReplaySettings::ModSettings*>();
    auto flow = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    flow->PresentFlowCoordinator(settings, nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false, false);
}

void OnIncrementChanged(float value) {
    viewController->SelectReplay(value - 1);
}

void MatchRequirements(UnityEngine::UI::Button* replayButton) {
    bool interactable = levelView->actionButton->interactable || levelView->practiceButton->interactable;
    replayButton->interactable = interactable && replayButton->interactable;
}

namespace Menu {
    void EnsureSetup(StandardLevelDetailView* view) {
        static ConstString canvasName("ReplayButtonCanvas");
        levelView = view;

        auto parent = view->practiceButton->transform->parent->GetComponent<UnityEngine::RectTransform*>();
        auto canvasTransform = (UnityEngine::RectTransform*) parent->Find(canvasName).unsafePtr();

        if (!canvasTransform) {
            LOG_INFO("Making button canvas");
            canvas = BSML::Lite::CreateCanvas();
            canvasTransform = canvas->GetComponent<UnityEngine::RectTransform*>();
            canvasTransform->SetParent(parent, false);
            canvasTransform->name = canvasName;
            canvasTransform->localScale = {1, 1, 1};
            canvasTransform->sizeDelta = {12, 10};
            canvasTransform->SetAsLastSibling();
            auto canvasComponent = canvas->GetComponent<UnityEngine::Canvas*>();
            canvasComponent->overrideSorting = true;
            canvasComponent->sortingOrder = 5;
            auto canvasLayout = canvas->AddComponent<UnityEngine::UI::LayoutElement*>();
            canvasLayout->preferredWidth = 12;

            auto replayButton = BSML::Lite::CreateUIButton(canvasTransform, "", OnReplayButtonClick);
            auto buttonTransform = replayButton->GetComponent<UnityEngine::RectTransform*>();
            auto icon = BSML::Lite::CreateImage(buttonTransform, PNG_SPRITE(Replay));
            icon->transform->localScale = {0.6, 0.6, 0.6};
            icon->preserveAspect = true;
            auto iconDims = icon->GetComponent<UnityEngine::UI::LayoutElement*>();
            iconDims->preferredWidth = 8;
            iconDims->preferredHeight = 8;
            replayButton->GetComponent<UnityEngine::UI::LayoutElement*>()->preferredWidth = -1;
            buttonTransform->anchoredPosition = {5, -5};

            parent->anchoredPosition = {5.2, -55};

            viewController = BSML::Helpers::CreateViewController<ReplayViewController*>();
            viewController->name = "ReplayViewController";
        }

        auto replayButton = canvasTransform->GetComponentInChildren<UnityEngine::UI::Button*>();
        BSML::MainThreadScheduler::ScheduleNextFrame([replayButton]() { MatchRequirements(replayButton); });
        if (auto hint = replayButton->GetComponent<HMUI::HoverHint*>())
            hint->text = "";
    }

    void SetButtonEnabled(bool enabled) {
        LOG_DEBUG("enabling button {}", enabled);
        canvas->active = enabled;
        canvas->GetComponentInChildren<UnityEngine::UI::Button*>()->interactable = true;
        float xpos = enabled ? 5.2 : -1.8;
        canvas->transform->parent->GetComponent<UnityEngine::RectTransform*>()->anchoredPosition = {xpos, -55};
    }

    void SetButtonUninteractable(std::string_view reason) {
        auto replayButton = canvas->GetComponentInChildren<UnityEngine::UI::Button*>();
        replayButton->interactable = false;
        if (auto hint = replayButton->GetComponent<HMUI::HoverHint*>())
            hint->text = reason;
        else
            BSML::Lite::AddHoverHint(replayButton, reason);
    }

    void CheckMultiplayer() {
        auto flowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        if (flowCoordinator.try_cast<MultiplayerLevelSelectionFlowCoordinator>())
            SetButtonEnabled(false);
    }

    void SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replays, bool external) {
        usingLocalReplays = !external;
        viewController->SetReplays(replays);
    }

    void PresentMenu() {
        auto flowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        flowCoordinator->showBackButton = true;
        flowCoordinator->PresentViewController(viewController, nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false);
        viewController->UpdateUI(true);
    }
    void DismissMenu() {
        if (viewController->isInViewControllerHierarchy && !viewController->childViewController) {
            auto flowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
            flowCoordinator->DismissViewController(viewController, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
        }
    }

    bool AreReplaysLocal() {
        return usingLocalReplays;
    }
}

void SetPreferred(auto object, std::optional<float> width, std::optional<float> height) {
    UnityEngine::UI::LayoutElement* layout = object->template GetComponent<UnityEngine::UI::LayoutElement*>();
    if (!layout)
        layout = object->gameObject->template AddComponent<UnityEngine::UI::LayoutElement*>();
    if (width.has_value())
        layout->preferredWidth = *width;
    if (height.has_value())
        layout->preferredHeight = *height;
}

void RemoveFit(auto object, bool horizontal = true, bool vertical = true) {
    UnityEngine::UI::ContentSizeFitter* layout = object->template GetComponent<UnityEngine::UI::ContentSizeFitter*>();
    if (!layout)
        return;
    if (horizontal)
        layout->horizontalFit = UnityEngine::UI::ContentSizeFitter::FitMode::Unconstrained;
    if (vertical)
        layout->verticalFit = UnityEngine::UI::ContentSizeFitter::FitMode::Unconstrained;
}

TMPro::TextMeshProUGUI* CreateCenteredText(UnityEngine::UI::HorizontalLayoutGroup* parent) {
    auto text = BSML::Lite::CreateText(parent->transform, "");
    text->fontSize = 4.5;
    text->alignment = TMPro::TextAlignmentOptions::Center;
    text->lineSpacing = -35;
    SetPreferred(text, 40, 15);
    return text;
}

std::string GetLayeredText(std::string const& label, std::string const& text, bool newline = true) {
    return fmt::format("<i><uppercase><color=#bdbdbd>{}</color></uppercase>{}{}</i>", label, newline ? "\n" : "", text);
}

inline UnityEngine::UI::Toggle* AddConfigValueToggle(UnityEngine::Transform* parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = BSML::Lite::CreateToggle(parent, configValue.GetName(), configValue.GetValue(), [&configValue, callback](bool value) {
        configValue.SetValue(value);
        callback(value);
    });
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object, configValue.GetHoverHint());
    return object;
}

void Menu::ReplayViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation) {
        cameraDropdown->dropdown->SelectCellWithIdx(getConfig().CamMode.GetValue());
        return;
    }

    using namespace UnityEngine;

    auto mainLayout = BSML::Lite::CreateVerticalLayoutGroup(transform);
    mainLayout->childControlWidth = true;
    mainLayout->childForceExpandWidth = true;
    mainLayout->childControlHeight = false;
    mainLayout->childForceExpandHeight = false;
    SetPreferred(mainLayout, 80, std::nullopt);

    auto levelBarTemplate =
        Resources::FindObjectsOfTypeAll<LevelBar*>()->First([](LevelBar* x) { return x->transform->parent->name == "PracticeViewController"; });
    levelBar = Object::Instantiate(levelBarTemplate, mainLayout->transform);
    levelBar->name = "ReplayLevelBarSimple";
    SetPreferred(levelBar, std::nullopt, 20);

    sourceText = BSML::Lite::CreateText(mainLayout, "");
    sourceText->fontSize = 4.5;
    sourceText->alignment = TMPro::TextAlignmentOptions::Center;
    sourceText->rectTransform->sizeDelta = {80, 8};

    auto horizontal1 = BSML::Lite::CreateHorizontalLayoutGroup(mainLayout);

    dateText = CreateCenteredText(horizontal1);
    modifiersText = CreateCenteredText(horizontal1);

    auto horizontal2 = BSML::Lite::CreateHorizontalLayoutGroup(mainLayout);

    scoreText = CreateCenteredText(horizontal2);
    failText = CreateCenteredText(horizontal2);

    auto horizontal3 = BSML::Lite::CreateHorizontalLayoutGroup(mainLayout);
    horizontal3->spacing = 5;

    watchButton = BSML::Lite::CreateUIButton(horizontal3, "Watch Replay", "ActionButton", {}, {0, 10}, OnWatchButtonClick);
    RemoveFit(watchButton);
    renderButton = BSML::Lite::CreateUIButton(horizontal3, "Render Replay", Vector2(), {0, 10}, OnRenderButtonClick);
    RemoveFit(renderButton);

    auto horizontal4 = BSML::Lite::CreateHorizontalLayoutGroup(mainLayout);
    horizontal4->spacing = 5;

    cameraDropdown = AddConfigValueDropdownEnum(horizontal4, getConfig().CamMode, cameraModes);
    cameraDropdown->transform->parent->Find("Label")->GetComponent<TMPro::TextMeshProUGUI*>()->text = "";
    SetPreferred(cameraDropdown->transform->parent, 30, 8);

    auto queueText = IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue";
    queueButton = BSML::Lite::CreateUIButton(horizontal4, queueText, Vector2(), {33, 8}, OnQueueButtonClick);
    queueButton->interactable = usingLocalReplays;
    RemoveFit(queueButton);

    auto settingsButton = BSML::Lite::CreateUIButton(transform, "", {32, -62}, {10, 10}, OnSettingsButtonClick);
    SetPreferred(settingsButton, -1, std::nullopt);
    auto settigsIcon = BSML::Lite::CreateImage(settingsButton, PNG_SPRITE(Settings));
    settigsIcon->transform->localScale = {0.8, 0.8, 0.8};
    settigsIcon->preserveAspect = true;
    SetPreferred(settigsIcon, 8, 8);

    deleteButton = BSML::Lite::CreateUIButton(transform, "", {128, -62}, {10, 10}, [this]() { confirmModal->Show(true, true, nullptr); });
    SetPreferred(deleteButton, -1, std::nullopt);
    auto deleteIcon = BSML::Lite::CreateImage(deleteButton, PNG_SPRITE(Delete));
    deleteIcon->transform->localScale = {0.8, 0.8, 0.8};
    deleteIcon->preserveAspect = true;
    deleteButton->gameObject->SetActive(usingLocalReplays);
    SetPreferred(deleteIcon, 8, 8);

    increment =
        BSML::Lite::CreateIncrementSetting(mainLayout, "", 0, 1, getConfig().LastReplayIdx.GetValue() + 1, 1, replays.size(), OnIncrementChanged);
    Object::Destroy(increment->GetComponent<UI::HorizontalLayoutGroup*>());
    increment->transform->GetChild(1)->GetComponent<RectTransform*>()->anchoredPosition = {-20, 0};

    confirmModal = BSML::Lite::CreateModal(transform, {58, 24}, nullptr);

    static ConstString dialogText("Are you sure you would like to delete this\nreplay? This cannot be undone.");

    auto removeText = BSML::Lite::CreateText(confirmModal, dialogText, TMPro::FontStyles::Normal, {0, 5});
    removeText->alignment = TMPro::TextAlignmentOptions::Center;

    auto confirmButton = BSML::Lite::CreateUIButton(confirmModal->transform, "Delete", "ActionButton", {40.5, -18}, {20, 10}, [this] {
        confirmModal->Hide(true, nullptr);
        OnDeleteButtonClick();
    });
    SetPreferred(confirmButton, 20, 10);
    auto cancelButton =
        BSML::Lite::CreateUIButton(confirmModal->transform, "Cancel", {17.5, -18}, {20, 10}, [this] { confirmModal->Hide(true, nullptr); });
    SetPreferred(cancelButton, 20, 10);
}

void Menu::ReplayViewController::OnEnable() {
    if (queueButton) {
        queueButton->interactable = usingLocalReplays;
        BSML::Lite::SetButtonText(queueButton, IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue");
    }
}

void Menu::ReplayViewController::SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replayInfos) {
    replays = replayInfos;
    if (getConfig().LastReplayIdx.GetValue() >= replayInfos.size())
        getConfig().LastReplayIdx.SetValue(replayInfos.size() - 1);
    auto lastBeatmap = beatmap;
    beatmap = {levelView->beatmapKey, levelView->_beatmapLevel};
    if (lastBeatmap != beatmap)
        beatmapData = nullptr;
    if (increment) {
        increment->maxValue = replayInfos.size();
        increment->set_Value(getConfig().LastReplayIdx.GetValue() + 1);
        increment->ApplyValue();
    }
}

void Menu::ReplayViewController::SelectReplay(int index) {
    getConfig().LastReplayIdx.SetValue(index);
    UpdateUI(false);
}

std::string& Menu::ReplayViewController::GetReplay() {
    return replays[getConfig().LastReplayIdx.GetValue()].first;
}

void Menu::ReplayViewController::UpdateUI(bool getData) {
    if (getData && !beatmapData) {
        MetaCore::Songs::GetBeatmapData(beatmap.difficulty, [this, forMap = beatmap](IReadonlyBeatmapData* data) {
            beatmapData = data;
            if (beatmap == forMap && increment)
                UpdateUI(false);
        });
    }
    auto beatmap = levelView->beatmapKey;
    auto level = levelView->_beatmapLevel;
    levelBar->Setup(level, beatmap.difficulty, beatmap.beatmapCharacteristic);
    float songLength = level->songDuration;

    auto info = replays[getConfig().LastReplayIdx.GetValue()].second;

    sourceText->text = GetLayeredText("Replay Source:  ", info->source, false);
    std::string date = MetaCore::Strings::TimeAgoString(info->timestamp);
    std::string modifiers = GetModifierString(info->modifiers, info->reached0Energy);
    std::string percent = "...";
    if (beatmapData) {
        float num = info->score * 100.0 / ScoreModel::ComputeMaxMultipliedScoreForBeatmap(beatmapData);
        percent = fmt::format("{:.2f}", num);
    }
    std::string score = fmt::format("{} <size=80%>(<color=#1dbcd1>{}%</color>)</size>", info->score, percent);
    std::string fail = info->failed ? "<color=#cc1818>True</color>" : "<color=#2adb44>False</color>";
    if (info->failed && info->failTime > 0.001)
        fail = fmt::format(
            "<color=#cc1818>{}</color> / {}", MetaCore::Strings::SecondsToString(info->failTime), MetaCore::Strings::SecondsToString(songLength)
        );

    dateText->text = GetLayeredText("Date Played", date);
    modifiersText->text = GetLayeredText("Modifiers", modifiers);
    scoreText->text = GetLayeredText("Score", score);
    failText->text = GetLayeredText("Failed", fail);

    queueButton->interactable = usingLocalReplays;
    BSML::Lite::SetButtonText(queueButton, IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue");

    deleteButton->gameObject->SetActive(usingLocalReplays);

    auto buttons = increment->transform->GetChild(1)->GetComponentsInChildren<UnityEngine::UI::Button*>();
    buttons->First()->interactable = getConfig().LastReplayIdx.GetValue() + 1 != increment->minValue;
    buttons->Last()->interactable = getConfig().LastReplayIdx.GetValue() + 1 != increment->maxValue;

    increment->gameObject->SetActive(increment->minValue != increment->maxValue);
}
