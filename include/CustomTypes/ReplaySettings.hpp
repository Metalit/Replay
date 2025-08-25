#pragma once

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/InputFieldView.hpp"
#include "HMUI/ModalView.hpp"
#include "HMUI/ViewController.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "bsml/shared/BSML/Components/Settings/DropdownListSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/IncrementSetting.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(Replay, MainSettings, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling
    );

    DECLARE_STATIC_METHOD(MainSettings*, GetInstance);

   private:
    static MainSettings* instance;
};

DECLARE_CLASS_CODEGEN(Replay, RenderSettings, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling
    );
    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);

    DECLARE_INSTANCE_METHOD(void, GetCover, GlobalNamespace::BeatmapLevel* level);
    DECLARE_INSTANCE_METHOD(void, UpdateCover, GlobalNamespace::BeatmapLevel* level, UnityEngine::Sprite* cover);

    DECLARE_STATIC_METHOD(RenderSettings*, GetInstance);

   private:
    UnityEngine::UI::Button* beginQueueButton;
    UnityEngine::UI::Button* clearQueueButton;
    BSML::CustomListTableData* queueList;
    custom_types::Helpers::Coroutine GetCoverCoro(GlobalNamespace::BeatmapLevel * level);

    static RenderSettings* instance;
};

DECLARE_CLASS_CODEGEN(Replay, InputSettings, HMUI::ViewController) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling
    );
    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);

    DECLARE_STATIC_METHOD(InputSettings*, GetInstance);

   private:
    std::array<BSML::IncrementSetting*, 3> positionSettings;
    std::array<BSML::IncrementSetting*, 3> rotationSettings;
    BSML::DropdownListSetting* presetDropdown;
    UnityEngine::UI::Button* removePresetButton;
    HMUI::ModalView* nameModal;
    HMUI::InputFieldView* nameInput;

    static InputSettings* instance;
};

DECLARE_CLASS_CODEGEN(Replay, ModSettings, HMUI::FlowCoordinator) {
    DECLARE_DEFAULT_CTOR();

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, DidActivate, &HMUI::FlowCoordinator::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling
    );
    DECLARE_OVERRIDE_METHOD_MATCH(void, BackButtonWasPressed, &HMUI::FlowCoordinator::BackButtonWasPressed, HMUI::ViewController* topViewController);
};
