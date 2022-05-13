#include "Main.hpp"
#include "ReplayMenu.hpp"

#include "Formats/FrameReplay.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"

#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"
#include "VRUIControls/VRGraphicRaycaster.hpp"

#include "questui/shared/BeatSaberUI.hpp"

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
            icon->get_transform()->set_localScale({0.5, 0.5, 0.5});
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
}

TMPro::TextMeshProUGUI* CreateCenteredText(UnityEngine::UI::VerticalLayoutGroup* parent) {
    auto text = BeatSaberUI::CreateText(parent->get_transform(), "");
    text->set_fontSize(4.5);
    text->set_alignment(TMPro::TextAlignmentOptions::Center);
    text->set_lineSpacing(-35);
    return text;
}

std::string GetLayeredText(const std::string& label, const std::string& text) {
    return "<i><uppercase><color=#bdbdbd>" + label + "</color></uppercase>\n" + text + "</i>";
}

void Menu::ReplayViewController::DidActivate(bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    if(!firstActivation)
        return;
        
    get_gameObject()->GetComponent<VRUIControls::VRGraphicRaycaster*>()->physicsRaycaster = BeatSaberUI::GetPhysicsRaycasterWithCache();
    
    levelBar = UnityEngine::Resources::FindObjectsOfTypeAll<LevelBar*>().First([](LevelBar* x) {
        return x->get_transform()->GetParent()->get_name() == "PracticeViewController";
    });

    levelBar->set_name("ReplayLevelBarSimple");
    levelBar->get_transform()->SetParent(get_transform(), false);
    levelBar->GetComponent<UnityEngine::RectTransform*>()->set_anchoredPosition({0, -5});

    auto horizontal = BeatSaberUI::CreateHorizontalLayoutGroup(get_transform());
    horizontal->set_spacing(0);
    horizontal->set_childControlWidth(false);
    horizontal->set_childForceExpandWidth(false);
    horizontal->get_rectTransform()->set_anchoredPosition({34.0f, -1});
    
    auto layout1 = BeatSaberUI::CreateVerticalLayoutGroup(horizontal);
    layout1->set_spacing(2.5);
    layout1->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(43);
    
    auto layout2 = BeatSaberUI::CreateVerticalLayoutGroup(horizontal);
    layout2->set_spacing(2.5);
    layout2->GetComponent<UnityEngine::UI::LayoutElement*>()->set_preferredWidth(43);

    dateText = CreateCenteredText(layout1);
    scoreText = CreateCenteredText(layout2);
    modifiersText = CreateCenteredText(layout1);
    failText = CreateCenteredText(layout2);
    UpdateUI();
}

void Menu::ReplayViewController::SetReplays(std::vector<ReplayInfo*> replayInfos, std::vector<std::string> replayPaths) {

}

void Menu::ReplayViewController::UpdateUI() {
    auto beatmap = levelView->get_selectedDifficultyBeatmap();
    auto level = (IPreviewBeatmapLevel*) beatmap->get_level();
    auto characteristic = beatmap->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic();
    auto difficulty = beatmap->get_difficulty();
    levelBar->Setup(level, characteristic, difficulty);

    dateText->set_text(GetLayeredText("Date Played", "test date"));
    scoreText->set_text(GetLayeredText("Score", "infinity (1%)"));
    modifiersText->set_text(GetLayeredText("Modifiers", "ESFS"));
    failText->set_text(GetLayeredText("Failed", "of course not"));
}
