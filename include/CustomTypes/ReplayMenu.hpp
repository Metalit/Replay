#pragma once

#include "GlobalNamespace/LevelBar.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "HMUI/ViewController.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/SimpleTextDropdown.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"

#include "questui/shared/CustomTypes/Components/Settings/IncrementSetting.hpp"

#include "custom-types/shared/macros.hpp"

struct ReplayInfo;

namespace Menu {
    void EnsureSetup(GlobalNamespace::StandardLevelDetailView* view);

    void SetButtonEnabled(bool enabled);

    void CheckMultiplayer();

    void SetReplays(std::vector<std::pair<std::string, ReplayInfo*>> replays, bool external = false);

    void PresentMenu();
    void DismissMenu();

    bool AreReplaysLocal();
}

DECLARE_CLASS_CODEGEN(Menu, ReplayViewController, HMUI::ViewController,

    DECLARE_OVERRIDE_METHOD(void, DidActivate,
        il2cpp_utils::il2cpp_type_check::MetadataGetter<&HMUI::ViewController::DidActivate>::get(),
        bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);

    DECLARE_INSTANCE_METHOD(void, UpdateUI);

    DECLARE_DEFAULT_CTOR();
    DECLARE_DTOR(dtor);

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
        UnityEngine::UI::Button* deleteButton;
        HMUI::SimpleTextDropdown* cameraDropdown;
        QuestUI::IncrementSetting* increment;
        HMUI::ModalView* confirmModal;

        std::vector<std::pair<std::string, ReplayInfo*>> replays;
        GlobalNamespace::IDifficultyBeatmap* beatmap;
        GlobalNamespace::IReadonlyBeatmapData* beatmapData;

        static inline UnityEngine::UI::Button* queueButton;
        static inline bool addedEvent = false;
)
