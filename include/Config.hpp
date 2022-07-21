#pragma once

#define HAS_CODEGEN
#include "config-utils/shared/config-utils.hpp"

const std::vector<std::pair<int, int>> resolutions = {
    {640, 480},
    {1280, 720},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160}
};

DECLARE_CONFIG(Config,
    CONFIG_VALUE(CameraMode, int, "Camera Mode", 0)
    CONFIG_VALUE(Smoothing, float, "Smoothing", 1, "The amount to smooth the camera by in smooth camera mode")
    CONFIG_VALUE(Correction, bool, "Correct Camera", true, "Whether to adjust the camera rotation to remove tilt")
    CONFIG_VALUE(Offset, UnityEngine::Vector3, "Position Offset", {0, 0, -0.5}, "The offset of the camera in smooth camera mode")
    CONFIG_VALUE(Relative, bool, "Relative Offset", true, "Whether the offset is dependent on the camera rotation (recommended for 360 levels)")
    CONFIG_VALUE(Walls, bool, "PC Walls", true, "Whether to use PC walls when rendering")
    CONFIG_VALUE(Shockwaves, bool, "Shockwaves", true, "Whether to enable shockwaves when rendering")
    CONFIG_VALUE(Resolution, int, "Render Resolution", 2)
    CONFIG_VALUE(Bitrate, int, "Render Bitrate", 15000)
    CONFIG_VALUE(FPS, int, "Render FPS", 60)
    CONFIG_VALUE(FOV, float, "Render FOV", 80)
    CONFIG_INIT_FUNCTION(
        CONFIG_INIT_VALUE(CameraMode)
        CONFIG_INIT_VALUE(Smoothing)
        CONFIG_INIT_VALUE(Correction)
        CONFIG_INIT_VALUE(Offset)
        CONFIG_INIT_VALUE(Relative)
        CONFIG_INIT_VALUE(Walls)
        CONFIG_INIT_VALUE(Shockwaves)
        CONFIG_INIT_VALUE(Resolution)
        CONFIG_INIT_VALUE(Bitrate)
        CONFIG_INIT_VALUE(FPS)
        CONFIG_INIT_VALUE(FOV)
    )
)

#include "HMUI/ViewController.hpp"

void SettingsDidActivate(HMUI::ViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling);
