#include "Main.hpp"
#include "ReplayMenu.hpp"

#include "Formats/FrameReplay.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "Config.hpp"

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

const std::vector<std::string> dropdownStrings = {
    "Normal",
    "Smooth Camera"
};

void OnReplayButtonClick() {
    if(!usingLocalReplays)
        Manager::RefreshLevelReplays();
    Menu::PresentMenu();
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

void OnWatchButtonClick() {
    Manager::Camera::rendering = false;
    Manager::ReplayStarted(viewController->GetReplay());
    levelView->actionButton->get_onClick()->Invoke();
}

void OnCameraModeSet(StringW value) {
    for(int i = 0; i < dropdownStrings.size(); i++) {
        if(dropdownStrings[i] == value) {
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

void OnIncrementChanged(float value) {
    viewController->SelectReplay(value - 1);
}

custom_types::Helpers::Coroutine MatchRequirementsCoroutine(UnityEngine::UI::Button* replayButton) {
    co_yield nullptr;
    
    bool interactable = levelView->actionButton->get_interactable();
    replayButton->set_interactable(interactable);
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
    }

    void SetButtonEnabled(bool enabled) {
        canvas->SetActive(enabled);
        float xpos = enabled ? 4.2 : -1.8;
        ((UnityEngine::RectTransform*) canvas->get_transform()->GetParent())->set_anchoredPosition({xpos, -55});
    }

    void CheckMultiplayer() {
        auto flowCoordinator = BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
        auto klass = il2cpp_functions::object_get_class((Il2CppObject*) flowCoordinator);
        if(il2cpp_functions::class_is_assignable_from(classof(GlobalNamespace::MultiplayerLevelSelectionFlowCoordinator*), klass))
            SetButtonEnabled(false);
    }

    void SetReplays(std::vector<ReplayInfo*> replayInfos, std::vector<std::string> replayPaths, bool external) {
        usingLocalReplays = !external;
        viewController->SetReplays(replayInfos, replayPaths);
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

TMPro::TextMeshProUGUI* CreateCenteredText(UnityEngine::UI::VerticalLayoutGroup* parent) {
    auto text = BeatSaberUI::CreateText(parent->get_transform(), "");
    text->set_fontSize(4.5);
    text->set_alignment(TMPro::TextAlignmentOptions::Center);
    text->set_lineSpacing(-35);
    return text;
}

std::string GetLayeredText(const std::string& label, const std::string& text, bool newline = true) {
    return fmt::format("<i><uppercase><color=#bdbdbd>{}</color></uppercase>{}{}</i>", label, newline ? "\n" : "", text);
}

void Menu::ReplayViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;
        
    auto levelBarTemplate = UnityEngine::Resources::FindObjectsOfTypeAll<LevelBar*>().First([](LevelBar* x) {
        return x->get_transform()->GetParent()->get_name() == "PracticeViewController";
    });
    levelBar = UnityEngine::Object::Instantiate(levelBarTemplate->get_gameObject(), get_transform())->GetComponent<LevelBar*>();
    levelBar->set_name("ReplayLevelBarSimple");
    levelBar->GetComponent<UnityEngine::RectTransform*>()->set_anchoredPosition({0, -2});

    sourceText = BeatSaberUI::CreateText(get_transform(), "", {0, 19});
    sourceText->set_fontSize(4.5);
    sourceText->set_alignment(TMPro::TextAlignmentOptions::Center);

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(get_transform());
    horizontal->set_spacing(5);
    horizontal->set_childControlWidth(false);
    horizontal->set_childForceExpandWidth(false);
    horizontal->get_rectTransform()->set_anchoredPosition({38.5, -10});
    
    auto layout1 = BeatSaberUI::CreateVerticalLayoutGroup(horizontal);
    layout1->set_spacing(3);
    layout1->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(40);
    
    auto layout2 = BeatSaberUI::CreateVerticalLayoutGroup(horizontal);
    layout2->set_spacing(3);
    layout2->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(40);

    dateText = CreateCenteredText(layout1);
    modifiersText = CreateCenteredText(layout2);
    scoreText = CreateCenteredText(layout1);
    failText = CreateCenteredText(layout2);

    auto layout3 = BeatSaberUI::CreateVerticalLayoutGroup(layout1);
    layout3->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(40);
    auto layout4 = BeatSaberUI::CreateVerticalLayoutGroup(layout2);
    layout4->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(40);

    watchButton = BeatSaberUI::CreateUIButton(layout3, "Watch Replay", "ActionButton", UnityEngine::Vector2(), UnityEngine::Vector2(0, 10), OnWatchButtonClick);
    renderButton = BeatSaberUI::CreateUIButton(layout4, "Render Replay", UnityEngine::Vector2(), UnityEngine::Vector2(0, 10), OnRenderButtonClick);
    std::vector<StringW> dropdownWs; for(auto str : dropdownStrings) dropdownWs.emplace_back(str);
    BeatSaberUI::CreateDropdown(layout3, "", dropdownWs[getConfig().CamMode.GetValue()], dropdownWs, OnCameraModeSet)
        ->get_transform()->get_parent()->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredHeight(10);
    deleteButton = BeatSaberUI::CreateUIButton(layout4, "Delete Replay", UnityEngine::Vector2(), UnityEngine::Vector2(0, 10), OnDeleteButtonClick);

    increment = BeatSaberUI::CreateIncrementSetting(get_transform(), "", 0, 1, currentReplay + 1, 1, replayInfos.size(), {-60, -74}, OnIncrementChanged);
}

void Menu::ReplayViewController::SetReplays(std::vector<ReplayInfo*> infos, std::vector<std::string> paths) {
    replayInfos = infos;
    replayPaths = paths;
    currentReplay = 0;
    auto lastBeatmap = beatmap;
    beatmap = levelView->selectedDifficultyBeatmap;
    if(lastBeatmap != beatmap) {
        beatmapData = nullptr;
        GetBeatmapData(beatmap, [this, infos](IReadonlyBeatmapData* data) {
            beatmapData = data;
            if(replayInfos == infos && increment) {
                UpdateUI();
            }
        });
    }
    if(increment) {
        increment->MaxValue = infos.size();
        increment->CurrentValue = 0;
        increment->UpdateValue();
    }
}

void Menu::ReplayViewController::SelectReplay(int index) {
    currentReplay = index;
    UpdateUI();
}

std::string& Menu::ReplayViewController::GetReplay() {
    return replayPaths[currentReplay];
}

void Menu::ReplayViewController::UpdateUI() {
    auto beatmap = levelView->get_selectedDifficultyBeatmap();
    auto level = (IPreviewBeatmapLevel*) beatmap->get_level();
    auto characteristic = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic();
    auto difficulty = beatmap->get_difficulty();
    levelBar->Setup(level, characteristic, difficulty);
    float songLength = level->get_songDuration();

    auto info = replayInfos[currentReplay];

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

    deleteButton->set_interactable(usingLocalReplays);

    auto buttons = increment->get_transform()->GetChild(1)->GetComponentsInChildren<UnityEngine::UI::Button*>();
    buttons.First()->set_interactable(currentReplay + 1 != increment->MinValue);
    buttons.Last()->set_interactable(currentReplay + 1 != increment->MaxValue);

    increment->get_gameObject()->SetActive(increment->MinValue != increment->MaxValue);
}
