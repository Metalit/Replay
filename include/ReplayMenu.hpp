#pragma once

#include "GlobalNamespace/LevelBar.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"

#include "questui/shared/CustomTypes/Components/Settings/IncrementSetting.hpp"

#include "custom-types/shared/macros.hpp"

struct ReplayInfo;

namespace Menu {
    void EnsureSetup(GlobalNamespace::StandardLevelDetailView* view);

    void SetButtonEnabled(bool enabled);

    void CheckMultiplayer();

    void SetReplays(std::vector<ReplayInfo*> replayInfos, std::vector<std::string> replayPaths);
}

DECLARE_CLASS_CODEGEN(Menu, ReplayViewController, HMUI::ViewController,
    
    DECLARE_OVERRIDE_METHOD(void, DidActivate,
        il2cpp_utils::il2cpp_type_check::MetadataGetter<&HMUI::ViewController::DidActivate>::get(),
        bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);

    DECLARE_INSTANCE_METHOD(void, UpdateUI);

    DECLARE_DEFAULT_CTOR();

    public:
        void SetReplays(std::vector<ReplayInfo*> replayInfos, std::vector<std::string> replayPaths);
        void SelectReplay(int index);
        std::string& GetReplay();
    private:
        GlobalNamespace::LevelBar* levelBar;
        TMPro::TextMeshProUGUI* sourceText;
        TMPro::TextMeshProUGUI* dateText;
        TMPro::TextMeshProUGUI* modifiersText;
        TMPro::TextMeshProUGUI* scoreText;
        TMPro::TextMeshProUGUI* failText;
        UnityEngine::UI::Button* deleteButton;
        UnityEngine::UI::Button* watchButton;
        QuestUI::IncrementSetting* increment;

        std::vector<ReplayInfo*> replayInfos;
        std::vector<std::string> replayPaths;
        int currentReplay;
        GlobalNamespace::IReadonlyBeatmapData* beatmapData;
)
