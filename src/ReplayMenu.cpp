#include "Main.hpp"
#include "ReplayMenu.hpp"

#include "Formats/FrameReplay.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"

#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
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

void OnReplayButtonClick() {
    auto flowCoordinator = UnityEngine::Resources::FindObjectsOfTypeAll<SinglePlayerLevelSelectionFlowCoordinator*>().First();
    flowCoordinator->showBackButton = true;
    flowCoordinator->PresentViewController(viewController, nullptr, HMUI::ViewController::AnimationDirection::Horizontal, false);
    viewController->UpdateUI();
}

void OnDeleteButtonClick() {

}

void OnWatchButtonClick() {
    levelView->actionButton->get_onClick()->Invoke();
    Manager::ReplayStarted(viewController->GetReplay());
}

void OnIncrementChanged(float value) {
    viewController->SelectReplay(value - 1);
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
    }

    void SetButtonEnabled(bool enabled) {
        canvas->SetActive(enabled);
        float xpos = enabled ? 4.2 : -1.8;
        ((UnityEngine::RectTransform*) canvas->get_transform()->GetParent())->set_anchoredPosition({xpos, -55});
    }

    void SetReplays(std::vector<ReplayInfo*> replayInfos, std::vector<std::string> replayPaths) {
        viewController->SetReplays(replayInfos, replayPaths);
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
    levelBar->GetComponent<UnityEngine::RectTransform*>()->set_anchoredPosition({0, -5});

    sourceText = BeatSaberUI::CreateText(get_transform(), "", {0, 14});
    sourceText->set_fontSize(4.5);
    sourceText->set_alignment(TMPro::TextAlignmentOptions::Center);

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(get_transform());
    horizontal->set_spacing(5);
    horizontal->set_childControlWidth(false);
    horizontal->set_childForceExpandWidth(false);
    horizontal->get_rectTransform()->set_anchoredPosition({38.5, -12});
    
    auto layout1 = BeatSaberUI::CreateVerticalLayoutGroup(horizontal);
    layout1->set_spacing(4);
    layout1->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(40);
    
    auto layout2 = BeatSaberUI::CreateVerticalLayoutGroup(horizontal);
    layout2->set_spacing(4);
    layout2->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(40);

    dateText = CreateCenteredText(layout1);
    modifiersText = CreateCenteredText(layout2);
    scoreText = CreateCenteredText(layout1);
    failText = CreateCenteredText(layout2);

    BeatSaberUI::CreateUIButton(layout1, "Delete Replay", UnityEngine::Vector2(), UnityEngine::Vector2(0, 10), OnDeleteButtonClick);
    BeatSaberUI::CreateUIButton(layout2, "Watch Replay", "ActionButton", UnityEngine::Vector2(), UnityEngine::Vector2(0, 10), OnWatchButtonClick);

    increment = BeatSaberUI::CreateIncrementSetting(get_transform(), "", 0, 1, currentReplay + 1, 1, replayInfos.size(), {-60, -74}, OnIncrementChanged);
}

void Menu::ReplayViewController::SetReplays(std::vector<ReplayInfo*> infos, std::vector<std::string> paths) {
    replayInfos = infos;
    replayPaths = paths;
    currentReplay = 0;
    beatmapData = nullptr;
    GetBeatmapData(levelView->selectedDifficultyBeatmap, [this, infos](IReadonlyBeatmapData* data) {
        beatmapData = data;
        if(replayInfos == infos && increment) {
            UpdateUI();
        }
    });
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
}
