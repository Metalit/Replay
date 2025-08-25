#include "CustomTypes/ReplayMenu.hpp"

#include "CustomTypes/ReplaySettings.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "GlobalNamespace/MultiplayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/ContentSizeFitter.hpp"
#include "VRUIControls/VRGraphicRaycaster.hpp"
#include "assets.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/creation.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/strings.hpp"
#include "metacore/shared/unity.hpp"
#include "parsing.hpp"
#include "utils.hpp"

DEFINE_TYPE(Replay, MenuView);

using namespace GlobalNamespace;
using namespace UnityEngine;

Replay::MenuView* Replay::MenuView::instance = nullptr;

static ConstString RecordingHint = "Install BeatLeader or ScoreSaber to record replays";

GameObject* canvas;
StandardLevelDetailView* levelView;

static StringW QueueButtonText() {
    return Manager::IsCurrentLevelInConfig() ? "Remove From Queue" : "Add To Render Queue";
}

static void OnReplayButtonClick() {
    Replay::MenuView::Present();
}

static void OnWatchButtonClick() {
    Manager::StartReplay(false);
    levelView->actionButton->onClick->Invoke();
}

static void OnRenderButtonClick() {
    Manager::StartReplay(true);
    levelView->actionButton->onClick->Invoke();
}

static void OnQueueButtonClick() {
    if (Manager::IsCurrentLevelInConfig())
        Manager::RemoveCurrentLevelFromConfig();
    else
        Manager::SaveCurrentLevelInConfig();
    Replay::MenuView::GetInstance()->OnEnable();
}

static void OnDeleteButtonClick() {
    if (!Manager::AreReplaysLocal())
        return;
    Manager::DeleteCurrentReplay();
}

static void OnSettingsButtonClick() {
    static UnityW<HMUI::FlowCoordinator> settings;
    if (!settings)
        settings = (HMUI::FlowCoordinator*) BSML::Helpers::CreateFlowCoordinator<Replay::ModSettings*>();
    auto flow = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    flow->PresentFlowCoordinator(settings, nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false, false);
}

static void OnIncrementChanged(float value) {
    Manager::SelectReplay(value - 1);
    Replay::MenuView::GetInstance()->UpdateUI(false);
}

static void SetPreferred(Component* object, std::optional<float> width, std::optional<float> height) {
    UI::LayoutElement* layout = object->GetComponent<UI::LayoutElement*>();
    if (!layout)
        layout = object->gameObject->AddComponent<UI::LayoutElement*>();
    if (width.has_value())
        layout->preferredWidth = *width;
    if (height.has_value())
        layout->preferredHeight = *height;
}

static void RemoveFit(Component* object, bool horizontal = true, bool vertical = true) {
    UI::ContentSizeFitter* layout = object->GetComponent<UI::ContentSizeFitter*>();
    if (!layout)
        return;
    if (horizontal)
        layout->horizontalFit = UI::ContentSizeFitter::FitMode::Unconstrained;
    if (vertical)
        layout->verticalFit = UI::ContentSizeFitter::FitMode::Unconstrained;
}

static TMPro::TextMeshProUGUI* CreateCenteredText(UI::HorizontalLayoutGroup* parent) {
    auto text = BSML::Lite::CreateText(parent->transform, "");
    text->fontSize = 4.5;
    text->alignment = TMPro::TextAlignmentOptions::Center;
    text->lineSpacing = -35;
    SetPreferred(text, 40, 15);
    return text;
}

static std::string GetLayeredText(std::string const& label, std::string const& text, bool newline = true) {
    return fmt::format("<i><uppercase><color=#bdbdbd>{}</color></uppercase>{}{}</i>", label, newline ? "\n" : "", text);
}

static UI::Toggle* AddConfigValueToggle(Transform* parent, ConfigUtils::ConfigValue<bool>& configValue, auto callback) {
    auto object = BSML::Lite::CreateToggle(parent, configValue.GetName(), configValue.GetValue(), [&configValue, callback](bool value) {
        configValue.SetValue(value);
        callback(value);
    });
    if (!configValue.GetHoverHint().empty())
        BSML::Lite::AddHoverHint(object, configValue.GetHoverHint());
    return object;
}

void Replay::MenuView::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if (!firstActivation) {
        cameraDropdown->dropdown->SelectCellWithIdx(getConfig().CamMode.GetValue());
        return;
    }

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
    statusText = CreateCenteredText(horizontal2);

    auto horizontal3 = BSML::Lite::CreateHorizontalLayoutGroup(mainLayout);
    horizontal3->spacing = 5;

    watchButton = BSML::Lite::CreateUIButton(horizontal3, "Watch Replay", "ActionButton", {}, {0, 10}, OnWatchButtonClick);
    RemoveFit(watchButton);
    renderButton = BSML::Lite::CreateUIButton(horizontal3, "Render Replay", Vector2(), {0, 10}, OnRenderButtonClick);
    RemoveFit(renderButton);

    auto horizontal4 = BSML::Lite::CreateHorizontalLayoutGroup(mainLayout);
    horizontal4->spacing = 5;

    cameraDropdown = AddConfigValueDropdownEnum(horizontal4, getConfig().CamMode, CameraModes);
    cameraDropdown->transform->parent->Find("Label")->GetComponent<TMPro::TextMeshProUGUI*>()->text = "";
    SetPreferred(cameraDropdown->transform->parent, 30, 8);

    auto queueText = QueueButtonText();
    queueButton = BSML::Lite::CreateUIButton(horizontal4, queueText, Vector2(), {33, 8}, OnQueueButtonClick);
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
    deleteButton->gameObject->SetActive(Manager::AreReplaysLocal());
    SetPreferred(deleteIcon, 8, 8);

    increment = BSML::Lite::CreateIncrementSetting(mainLayout, "", 0, 1, 1, 1, Manager::GetReplaysCount(), OnIncrementChanged);
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

void Replay::MenuView::UpdateUI(bool getData) {
    if (!wasActivatedBefore)
        return;

    auto beatmap = MetaCore::Songs::GetSelectedKey();
    auto level = MetaCore::Songs::GetSelectedLevel();

    if (getData) {
        beatmapData = nullptr;
        MetaCore::Songs::GetBeatmapData(beatmap, [this, forMap = beatmap](IReadonlyBeatmapData* data) {
            if (!MetaCore::Songs::GetSelectedKey().Equals(forMap))
                return;
            beatmapData = data;
            UpdateUI(false);
        });
    }

    levelBar->Setup(level, beatmap.difficulty, beatmap.beatmapCharacteristic);
    float songLength = level->songDuration;

    auto& info = Manager::GetCurrentInfo();
    Parsing::CheckForQuit(info, songLength);

    sourceText->text = GetLayeredText("Replay Source:  ", info.source, false);
    std::string date = MetaCore::Strings::TimeAgoString(info.timestamp);
    std::string modifiers = Utils::GetModifierString(info.modifiers, info.reached0Energy);
    std::string percent = "...";
    if (beatmapData) {
        float num = info.score * 100.0 / ScoreModel::ComputeMaxMultipliedScoreForBeatmap(beatmapData);
        percent = fmt::format("{:.2f}", num);
    }
    std::string score = fmt::format("{} <size=80%>(<color=#1dbcd1>{}%</color>)</size>", info.score, percent);
    std::string status = Utils::GetStatusString(info, true, songLength);

    dateText->text = GetLayeredText("Date Played", date);
    modifiersText->text = GetLayeredText("Modifiers", modifiers);
    scoreText->text = GetLayeredText("Score", score);
    statusText->text = GetLayeredText("Status", status);

    BSML::Lite::SetButtonText(queueButton, QueueButtonText());

    deleteButton->gameObject->SetActive(Manager::AreReplaysLocal());

    int selectedReplay = Manager::GetSelectedIndex() + 1;
    increment->set_Value(selectedReplay);
    increment->maxValue = Manager::GetReplaysCount();
    auto buttons = increment->transform->GetChild(1)->GetComponentsInChildren<UI::Button*>();
    buttons->First()->interactable = selectedReplay != increment->minValue;
    buttons->Last()->interactable = selectedReplay != increment->maxValue;

    increment->gameObject->SetActive(increment->minValue != increment->maxValue);
}

void Replay::MenuView::OnEnable() {
    if (queueButton)
        BSML::Lite::SetButtonText(queueButton, QueueButtonText());
}

static void MatchRequirements(UI::Button* replayButton) {
    bool interactable = levelView->actionButton->interactable || levelView->practiceButton->interactable;
    replayButton->interactable = interactable && replayButton->interactable;
}

void Replay::MenuView::CreateShortcut() {
    if (!canvas) {
        logger.info("Making button canvas");

        levelView = UnityEngine::Object::FindObjectOfType<StandardLevelDetailView*>(true);

        auto parent = levelView->practiceButton->transform->parent->GetComponent<RectTransform*>();
        canvas = BSML::Lite::CreateCanvas();
        MetaCore::Engine::SetOnDestroy(canvas, []() { canvas = nullptr; });

        auto canvasTransform = canvas->GetComponent<RectTransform*>();
        canvasTransform->SetParent(parent, false);
        canvasTransform->name = "ReplayButtonCanvas";
        canvasTransform->localScale = {1, 1, 1};
        canvasTransform->sizeDelta = {12, 10};
        canvasTransform->SetAsLastSibling();

        auto canvasComponent = canvas->GetComponent<Canvas*>();
        canvasComponent->overrideSorting = true;
        canvasComponent->sortingOrder = 5;

        auto canvasLayout = canvas->AddComponent<UI::LayoutElement*>();
        canvasLayout->preferredWidth = 12;

        auto replayButton = BSML::Lite::CreateUIButton(canvasTransform, "", OnReplayButtonClick);
        auto buttonTransform = replayButton->GetComponent<RectTransform*>();
        auto icon = BSML::Lite::CreateImage(buttonTransform, PNG_SPRITE(Replay));
        icon->transform->localScale = {0.6, 0.6, 0.6};
        icon->preserveAspect = true;
        auto iconDims = icon->GetComponent<UI::LayoutElement*>();
        iconDims->preferredWidth = 8;
        iconDims->preferredHeight = 8;

        replayButton->GetComponent<UI::LayoutElement*>()->preferredWidth = -1;
        buttonTransform->anchoredPosition = {5, -5};

        parent->anchoredPosition = {5.2, -55};
    }

    auto replayButton = canvas->GetComponentInChildren<UI::Button*>();
    BSML::MainThreadScheduler::ScheduleNextFrame([replayButton]() { MatchRequirements(replayButton); });
    if (auto hint = replayButton->GetComponent<HMUI::HoverHint*>())
        hint->text = "";
}

void Replay::MenuView::SetEnabled(bool value) {
    if (!canvas)
        return;
    canvas->active = value;
    canvas->GetComponentInChildren<UI::Button*>()->interactable = true;
    float xpos = value ? 5.2 : -1.8;
    canvas->transform->parent->GetComponent<RectTransform*>()->anchoredPosition = {xpos, -55};
}

void Replay::MenuView::DisableWithRecordingHint() {
    if (!canvas)
        return;
    auto replayButton = canvas->GetComponentInChildren<UI::Button*>();
    replayButton->interactable = false;
    if (auto hint = replayButton->GetComponent<HMUI::HoverHint*>())
        hint->text = RecordingHint;
    else
        BSML::Lite::AddHoverHint(replayButton, RecordingHint);
}

void Replay::MenuView::CheckMultiplayer() {
    auto flowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    if (flowCoordinator.try_cast<MultiplayerLevelSelectionFlowCoordinator>())
        SetEnabled(false);
}

void Replay::MenuView::Present() {
    if (Presented())
        return;
    auto flowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    flowCoordinator->showBackButton = true;
    flowCoordinator->PresentViewController(GetInstance(), nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false);
    GetInstance()->UpdateUI(true);
}

void Replay::MenuView::Dismiss() {
    if (Presented() && !GetInstance()->childViewController) {
        auto flowCoordinator = BSML::Helpers::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        flowCoordinator->DismissViewController(GetInstance(), HMUI::ViewController::AnimationDirection::Horizontal, nullptr, false);
    }
}

bool Replay::MenuView::Presented() {
    return instance && instance->isInViewControllerHierarchy;
}

Replay::MenuView* Replay::MenuView::GetInstance() {
    if (!instance) {
        instance = BSML::Helpers::CreateViewController<Replay::MenuView*>();
        instance->name = "ReplayMenuViewController";
        MetaCore::Engine::SetOnDestroy(instance, []() { instance = nullptr; });
    }
    return instance;
}
