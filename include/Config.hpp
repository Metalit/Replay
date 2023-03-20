#pragma once

#include "config-utils/shared/config-utils.hpp"

const std::vector<std::pair<int, int>> resolutions = {
    {640, 480},
    {1280, 720},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160}
};

const std::vector<std::string> cameraModes = {
    "Normal",
    "Smooth Camera",
    "Third Person"
};

enum struct CameraMode {
    Headset,
    Smooth,
    ThirdPerson
};

enum struct InputButton {
    None,
    SideTrigger,
    FrontTrigger,
    LowerButton,
    UpperButton,
    JoystickUp,
    JoystickDown,
    JoystickLeft,
    JoystickRight
};

enum struct InputController {
    None,
    Left,
    Right
};

DECLARE_JSON_CLASS(Button,
    VALUE(int, Button)
    VALUE(int, Controller)
)
DECLARE_JSON_CLASS(ButtonPair,
    VALUE(int, ForwardButton)
    VALUE(int, ForwardController)
    VALUE(int, BackButton)
    VALUE(int, BackController)
)

DECLARE_JSON_CLASS(LevelSelection,
    VALUE(std::string, ID)
    VALUE(std::string, PackID)
    VALUE(int, Difficulty)
    VALUE(std::string, Characteristic)
    VALUE(int, Category)
    VALUE(int, ReplayIndex)
)

DECLARE_CONFIG(Config,
    CONFIG_VALUE(CamMode, int, "Camera Mode", 0)
    CONFIG_VALUE(AudioMode, bool, "Audio Mode", false)
    CONFIG_VALUE(RenderLaunch, bool, "Render On Next Launch", false)
    CONFIG_VALUE(LevelsToSelect, std::vector<LevelSelection>, "Select Level On Start", {})
    CONFIG_VALUE(LastReplayIdx, int, "Last Selected Replay Index", 0)
    CONFIG_VALUE(SimMode, bool, "Simulation Mode", false, "Disables score overriding when watching replays, basing the score only off of the movements you made")

    CONFIG_VALUE(Smoothing, float, "Smoothing", 1, "The amount to smooth the camera by in smooth camera mode")
    CONFIG_VALUE(Correction, bool, "Correct Camera", true, "Whether to adjust the camera rotation to remove tilt")
    CONFIG_VALUE(Offset, UnityEngine::Vector3, "Position Offset", UnityEngine::Vector3(0, 0, -0.5), "The offset of the camera in smooth camera mode")
    CONFIG_VALUE(Relative, bool, "Relative Offset", true, "Whether the offset is dependent on the camera rotation (recommended for 360 levels)")
    CONFIG_VALUE(HideText, bool, "Hide Player Text", true, "Whether to hide the REPLAY player text for locally saved replays")
    CONFIG_VALUE(TextHeight, float, "Player Text Height", 7, "The height of the REPLAY player text when visible")
    CONFIG_VALUE(Avatar, bool, "Enable Avatar", true, "Shows avatar when in third person camera mode")

    CONFIG_VALUE(Walls, bool, "PC Walls", true, "Whether to use PC walls when rendering")
    CONFIG_VALUE(Mirrors, int, "PC Mirrors", 3, "PC Mirrors level to use when rendering")
    // CONFIG_VALUE(Bloom, int, "Bloom", 3, "PC bloom level to use when rendering")
    CONFIG_VALUE(ShockwavesOn, bool, "Shockwaves Enabled", true)
    CONFIG_VALUE(Shockwaves, int, "Shockwaves", 1, "Shockwave level to use when rendering")
    CONFIG_VALUE(Resolution, int, "Resolution", 2, "Warning: resolutions higher than 1080p may cause Beat Saber to run out of memory and crash")
    CONFIG_VALUE(Bitrate, int, "Bitrate", 50000)
    CONFIG_VALUE(FOV, float, "FOV", 70)
    CONFIG_VALUE(FPS, int, "FPS", 60)
    CONFIG_VALUE(CameraOff, bool, "Disable Camera", false, "Disables the main camera to speed up renders")

    CONFIG_VALUE(Pauses, bool, "Allow Pauses", false, "Whether to allow the game to pause while rendering")
    CONFIG_VALUE(Restart, bool, "Restart Game", true, "Restart game after finishing a render")
    CONFIG_VALUE(Ding, bool, "Ding", false, "Plays a sound when renders are finished")

    CONFIG_VALUE(TimeButton, ButtonPair, "Skip Forward|Skip Backward", {}, "Skips around in the time while watching a replay")
    CONFIG_VALUE(TimeSkip, int, "Time Skip Amount", 5, "Number of seconds to skip per button press")
    CONFIG_VALUE(SpeedButton, ButtonPair, "Speed Up|Slow Down", {}, "Changes playback speed while watching a replay")
    CONFIG_VALUE(MoveButton, Button, "Movement Button", {}, "Enables moving to a desired third person position when held")
    CONFIG_VALUE(TravelButton, ButtonPair, "Travel Forward|Travel Backward", {}, "Moves the environment around you in third person")
    CONFIG_VALUE(TravelSpeed, float, "Travel Speed", 1, "The speed multiplier for the travel function")

    CONFIG_VALUE(ThirdPerPos, UnityEngine::Vector3, "Third Person Position", UnityEngine::Vector3(0, 2, -0.5))
    CONFIG_VALUE(ThirdPerRot, UnityEngine::Vector3, "Third Person Rotation", {})
)
