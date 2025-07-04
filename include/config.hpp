#pragma once

#include "config-utils/shared/config-utils.hpp"

inline std::vector<std::pair<int, int>> Resolutions = {{640, 480}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
inline std::vector<std::string_view> CameraModes = {"Normal", "Smooth Camera", "Third Person"};

enum struct CameraMode { Headset, Smooth, ThirdPerson };
enum struct InputButton { None, SideTrigger, FrontTrigger, LowerButton, UpperButton, JoystickUp, JoystickDown, JoystickLeft, JoystickRight };
enum struct InputController { None, Left, Right };

DECLARE_JSON_STRUCT(Button) {
    VALUE(int, Button);
    VALUE(int, Controller);
};

DECLARE_JSON_STRUCT(ButtonPair) {
    VALUE(int, ForwardButton);
    VALUE(int, ForwardController);
    VALUE(int, BackButton);
    VALUE(int, BackController);
};

DECLARE_JSON_STRUCT(LevelSelection) {
    VALUE(std::string, ID);
    VALUE(int, Difficulty);
    VALUE(std::string, Characteristic);
    VALUE(int, ReplayIndex);
};

DECLARE_JSON_STRUCT(ThirdPerPreset) {
    VALUE(ConfigUtils::Vector3, Position);
    VALUE(ConfigUtils::Vector3, Rotation);
};

DECLARE_CONFIG(Config) {
    CONFIG_VALUE(Version, int, "Config Version", 1);
    CONFIG_VALUE(CamMode, int, "Camera Mode", 0);
    CONFIG_VALUE(LevelsToSelect, std::vector<LevelSelection>, "Select Level On Start", {});
    CONFIG_VALUE(LastReplayIdx, int, "Last Selected Replay Index", 0);
    CONFIG_VALUE(OverrideWidth, int, "Override Resolution Width", -1);
    CONFIG_VALUE(OverrideHeight, int, "Override Resolution Height", -1);

    CONFIG_VALUE(Smoothing, float, "Smoothing", 1, "The amount to smooth the camera by in smooth camera mode");
    CONFIG_VALUE(Correction, bool, "Correct Camera", true, "Whether to adjust the camera rotation to remove tilt");
    CONFIG_VALUE(TargetTilt, float, "Target Tilt", 10, "The degrees to tilt the camera (as if looking up/down) after correction");
    CONFIG_VALUE(Offset, UnityEngine::Vector3, "Position Offset", UnityEngine::Vector3(0, 0, -0.5), "The offset of the camera in smooth camera mode");
    CONFIG_VALUE(Relative, bool, "Relative Offset", true, "Whether the offset is dependent on the camera rotation (recommended for 360 levels)");
    CONFIG_VALUE(HideText, bool, "Hide Player Text", true, "Whether to hide the REPLAY player text for your own replays");
    CONFIG_VALUE(TextHeight, float, "Player Text Height", 7, "The height of the REPLAY player text when visible");
    CONFIG_VALUE(Avatar, bool, "Enable Avatar", true, "Shows avatar when in third person camera mode");

    CONFIG_VALUE(Walls, int, "Wall Style", 0, "What kind of walls to display when rendering");
    CONFIG_VALUE(Bloom, bool, "Bloom", true, "Whether to use PC bloom when rendering");
    CONFIG_VALUE(Mirrors, int, "PC Mirrors", 3, "PC mirrors level to use when rendering");
    CONFIG_VALUE(AntiAlias, int, "Anti-Aliasing", 3, "Anti-Aliasing level for the rendering output");
    CONFIG_VALUE(ShockwavesOn, bool, "Shockwaves Enabled", true);
    CONFIG_VALUE(Shockwaves, int, "Shockwaves", 1, "Whether to show shockwaves when rendering");
    CONFIG_VALUE(Resolution, int, "Resolution", 2, "The resolution to use when rendering");
    CONFIG_VALUE(Bitrate, int, "Bitrate", 10000, "The bitrate in kbps to use when rendering");
    CONFIG_VALUE(FOV, float, "FOV", 70, "Only applies during renders");
    CONFIG_VALUE(FPS, int, "FPS", 60, "Only applies during renders");
    CONFIG_VALUE(
        HEVC,
        bool,
        "HEVC Encoder",
        true,
        "Whether to use the HEVC/H265 encoder, instead of AVC/H264, for better video quality but potentially less support on old devices or platforms"
    );

    CONFIG_VALUE(Pauses, bool, "Allow Pauses", false, "Whether to allow the game to pause while rendering");
    CONFIG_VALUE(Ding, bool, "Ding", false, "Plays a sound when renders are finished");

    CONFIG_VALUE(TimeButton, ButtonPair, "Skip Forward|Skip Backward", {}, "Skips around in the time while watching a replay");
    CONFIG_VALUE(TimeSkip, int, "Time Skip Amount", 5, "Number of seconds to skip per button press");
    CONFIG_VALUE(SpeedButton, ButtonPair, "Speed Up|Slow Down", {}, "Changes playback speed while watching a replay");
    CONFIG_VALUE(MoveButton, Button, "Movement Button", {}, "Enables moving to a desired third person position when held");
    CONFIG_VALUE(TravelButton, ButtonPair, "Travel Forward|Travel Backward", {}, "Moves the environment around you in third person");
    CONFIG_VALUE(TravelSpeed, float, "Travel Speed", 1, "The speed multiplier for the travel function");

    CONFIG_VALUE(ThirdPerPos, UnityEngine::Vector3, "Third Person Position", UnityEngine::Vector3(1, 2, -1));
    CONFIG_VALUE(ThirdPerRot, UnityEngine::Vector3, "Third Person Rotation", UnityEngine::Vector3(14, 335, 0));
    CONFIG_VALUE(ThirdPerPresets, StringKeyedMap<ThirdPerPreset>, "Third Person Presets", {});
    CONFIG_VALUE(CurrentThirdPerPreset, std::string, "Third Person Preset", "Default");

    CONFIG_VALUE(
        Simulation,
        bool,
        "Simulation Mode",
        false,
        "Disables score overriding when watching replays, basing the score only off of the movements you made"
    );
    CONFIG_VALUE(CleanFiles, bool, "Remove Temp Files", true);
};
