#pragma once

#include "metacore/shared/assets.hpp"

#define DECLARE_ASSET(name, binary)       \
    const IncludedAsset name {            \
        Externs::_binary_##binary##_start, \
        Externs::_binary_##binary##_end    \
    };

#define DECLARE_ASSET_NS(namespaze, name, binary) \
    namespace namespaze { DECLARE_ASSET(name, binary) }

namespace IncludedAssets {
    namespace Externs {
        extern "C" uint8_t _binary_Delete_png_start[];
        extern "C" uint8_t _binary_Delete_png_end[];
        extern "C" uint8_t _binary_Ding_wav_start[];
        extern "C" uint8_t _binary_Ding_wav_end[];
        extern "C" uint8_t _binary_Replay_png_start[];
        extern "C" uint8_t _binary_Replay_png_end[];
        extern "C" uint8_t _binary_Settings_png_start[];
        extern "C" uint8_t _binary_Settings_png_end[];
    }

    // Delete.png
    DECLARE_ASSET(Delete_png, Delete_png);
    // Ding.wav
    DECLARE_ASSET(Ding_wav, Ding_wav);
    // Replay.png
    DECLARE_ASSET(Replay_png, Replay_png);
    // Settings.png
    DECLARE_ASSET(Settings_png, Settings_png);
}
