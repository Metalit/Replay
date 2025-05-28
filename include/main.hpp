#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

extern modloader::ModInfo modInfo;

constexpr auto RendersFolder = "/sdcard/Renders";

void SelectLevelOnNextSongRefresh(bool render = true, int idx = 0);

extern bool recorderInstalled;
