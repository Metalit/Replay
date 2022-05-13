#pragma once

#include "GlobalNamespace/LevelBar.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"

#include "custom-types/shared/macros.hpp"

struct ReplayInfo;

namespace Menu {
    void EnsureSetup(GlobalNamespace::StandardLevelDetailView* view);
}

DECLARE_CLASS_CODEGEN(Menu, ReplayViewController, HMUI::ViewController,
    
    DECLARE_OVERRIDE_METHOD(void, DidActivate,
        il2cpp_utils::il2cpp_type_check::MetadataGetter<&HMUI::ViewController::DidActivate>::get(),
        bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);

    DECLARE_INSTANCE_METHOD(void, UpdateUI);

    public:
        void SetReplays(std::vector<ReplayInfo*> replayInfos, std::vector<std::string> replayPaths);
    private:
        GlobalNamespace::LevelBar* levelBar;
        TMPro::TextMeshProUGUI* dateText;
        TMPro::TextMeshProUGUI* scoreText;
        TMPro::TextMeshProUGUI* modifiersText;
        TMPro::TextMeshProUGUI* failText;
        UnityEngine::UI::Button* deleteButton;
        UnityEngine::UI::Button* watchButton;

        std::vector<ReplayInfo*> replayInfos;
        std::vector<std::string> replayPaths;
        int currentReplay;
)
