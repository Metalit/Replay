#pragma once

#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "GlobalNamespace/LevelBar.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/ViewController.hpp"
#include "TMPro/TextMeshProUGUI.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/IncrementSetting.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(Menu, ReplayViewController, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling
    );
    DECLARE_INSTANCE_METHOD(void, UpdateUI, bool getData);
    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDestroy);

    DECLARE_STATIC_METHOD(void, CreateShortcut, GlobalNamespace::StandardLevelDetailView* view);
    DECLARE_STATIC_METHOD(void, SetEnabled, bool value);
    DECLARE_STATIC_METHOD(void, DisableWithRecordingHint);
    DECLARE_STATIC_METHOD(void, CheckMultiplayer);
    DECLARE_STATIC_METHOD(void, Present);
    DECLARE_STATIC_METHOD(void, Dismiss);
    DECLARE_STATIC_METHOD(ReplayViewController*, GetInstance);

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

    GlobalNamespace::IReadonlyBeatmapData* beatmapData;

    static ReplayViewController* instance;
};
