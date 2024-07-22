#pragma once

#include "GlobalNamespace/BeatmapLevel.hpp"
#include "HMUI/FlowCoordinator.hpp"
#include "HMUI/ViewController.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "bsml/shared/BSML/Components/CustomListTableData.hpp"
#include "bsml/shared/BSML/Components/Settings/IncrementSetting.hpp"
#include "custom-types/shared/coroutine.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplaySettings, MainSettings, HMUI::ViewController,
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_DEFAULT_CTOR();
)

DECLARE_CLASS_CODEGEN(ReplaySettings, RenderSettings, HMUI::ViewController,
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_INSTANCE_METHOD(void, OnDisable);
    DECLARE_INSTANCE_METHOD(void, GetCover, GlobalNamespace::BeatmapLevel* level);
    DECLARE_INSTANCE_METHOD(void, UpdateCover, GlobalNamespace::BeatmapLevel* level, UnityEngine::Sprite* cover);
    DECLARE_DEFAULT_CTOR();
   private:
    UnityEngine::UI::Button* beginQueueButton;
    UnityEngine::UI::Button* clearQueueButton;
    BSML::CustomListTableData* queueList;
    custom_types::Helpers::Coroutine GetCoverCoro(GlobalNamespace::BeatmapLevel* level);
)

DECLARE_CLASS_CODEGEN(ReplaySettings, InputSettings, HMUI::ViewController,
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::ViewController::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_INSTANCE_METHOD(void, OnEnable);
    DECLARE_DEFAULT_CTOR();
   private:
    std::array<BSML::IncrementSetting*, 3> positionSettings{};
    std::array<BSML::IncrementSetting*, 3> rotationSettings{};
)

DECLARE_CLASS_CODEGEN(ReplaySettings, ModSettings, HMUI::FlowCoordinator,
    DECLARE_OVERRIDE_METHOD_MATCH(void, DidActivate, &HMUI::FlowCoordinator::DidActivate, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
    DECLARE_OVERRIDE_METHOD_MATCH(void, BackButtonWasPressed, &HMUI::FlowCoordinator::BackButtonWasPressed, HMUI::ViewController* topViewController);
    DECLARE_DEFAULT_CTOR();
)

// defined in Config.cpp
