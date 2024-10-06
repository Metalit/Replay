#pragma once

#include "GlobalNamespace/BeatmapKey.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/LevelBar.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/SimpleTextDropdown.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "Utils.hpp"
#include "custom-types/shared/macros.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/IncrementSetting.hpp"

struct ReplayInfo;

namespace Menu {
    void EnsureSetup(GlobalNamespace::StandardLevelDetailView* view);

    void SetButtonEnabled(bool enabled);
    void SetButtonUninteractable(std::string_view reason);

    void CheckMultiplayer();

    void SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replays, bool external = false);

    void PresentMenu();
    void DismissMenu();

    bool AreReplaysLocal();
}

DECLARE_CLASS_CODEGEN(Menu, ReplayViewController, HMUI::ViewController,

    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);

    DECLARE_INSTANCE_METHOD(void, UpdateUI, bool getData);
    DECLARE_INSTANCE_METHOD(void, OnEnable);

   public:
    void SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replays);
    void SelectReplay(int index);
    std::string& GetReplay();
   private:
    GlobalNamespace::LevelBar* levelBar;
    TMPro::TextMeshProUGUI* sourceText;
    TMPro::TextMeshProUGUI* dateText;
    TMPro::TextMeshProUGUI* modifiersText;
    TMPro::TextMeshProUGUI* scoreText;
    TMPro::TextMeshProUGUI* failText;
    UnityEngine::UI::Button* watchButton;
    UnityEngine::UI::Button* renderButton;
    UnityEngine::UI::Button* queueButton;
    UnityEngine::UI::Button* deleteButton;
    BSML::DropdownListSetting* cameraDropdown;
    BSML::IncrementSetting* increment;
    HMUI::ModalView* confirmModal;

    std::vector<std::pair<std::string, ReplayInfo*>> replays;
    DifficultyBeatmap beatmap;
    GlobalNamespace::IReadonlyBeatmapData* beatmapData;
)
