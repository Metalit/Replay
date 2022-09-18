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

enum struct CameraMode {
    Headset,
    Smooth,
    ThirdPerson
};

DECLARE_CONFIG(Config,
    CONFIG_VALUE(CamMode, int, "Camera Mode", 0)
    CONFIG_VALUE(Smoothing, float, "Smoothing", 1, "The amount to smooth the camera by in smooth camera mode")
    CONFIG_VALUE(Correction, bool, "Correct Camera", true, "Whether to adjust the camera rotation to remove tilt")
    CONFIG_VALUE(Offset, UnityEngine::Vector3, "Position Offset", {0, 0, -0.5}, "The offset of the camera in smooth camera mode")
    CONFIG_VALUE(Relative, bool, "Relative Offset", true, "Whether the offset is dependent on the camera rotation (recommended for 360 levels)")
    CONFIG_VALUE(HideText, bool, "Hide Player Text", true, "Whether to hide the REPLAY player text for locally saved replays")
    CONFIG_VALUE(TextHeight, float, "Player Text Height", 7, "The height of the REPLAY player text when visible")
    CONFIG_VALUE(Walls, bool, "PC Walls", true, "Whether to use PC walls when rendering")
    CONFIG_VALUE(Mirrors, int, "PC Mirrors", 3, "PC Mirrors level to use when rendering")
    CONFIG_VALUE(Bloom, int, "Bloom", 3, "PC bloom level to use when rendering")
    CONFIG_VALUE(Shockwaves, int, "Shockwaves", 2, "Shockwave level to use when rendering")
    CONFIG_VALUE(Resolution, int, "Resolution", 2)
    CONFIG_VALUE(Bitrate, int, "Bitrate", 20000)
    CONFIG_VALUE(FPS, int, "FPS", 60)
    CONFIG_VALUE(FOV, float, "FOV", 80)
    CONFIG_VALUE(Pauses, bool, "Allow Pauses", false, "Whether to allow the game to pause while rendering")
    CONFIG_INIT_FUNCTION(
        CONFIG_INIT_VALUE(CamMode)
        CONFIG_INIT_VALUE(Smoothing)
        CONFIG_INIT_VALUE(Correction)
        CONFIG_INIT_VALUE(Offset)
        CONFIG_INIT_VALUE(Relative)
        CONFIG_INIT_VALUE(HideText)
        CONFIG_INIT_VALUE(TextHeight)
        CONFIG_INIT_VALUE(Walls)
        CONFIG_INIT_VALUE(Mirrors)
        CONFIG_INIT_VALUE(Bloom)
        CONFIG_INIT_VALUE(Shockwaves)
        CONFIG_INIT_VALUE(Resolution)
        CONFIG_INIT_VALUE(Bitrate)
        CONFIG_INIT_VALUE(FPS)
        CONFIG_INIT_VALUE(FOV)
        CONFIG_INIT_VALUE(Pauses)
    )
)
