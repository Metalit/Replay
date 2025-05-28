#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#define LOG_INFO(...) logger.info(__VA_ARGS__)
#define LOG_DEBUG(...) logger.debug(__VA_ARGS__)
#define LOG_ERROR(...) logger.error(__VA_ARGS__)

extern modloader::ModInfo modInfo;

inline auto RendersFolder = "/sdcard/Renders";

void SelectLevelOnNextSongRefresh(bool render = true, int idx = 0);

extern bool recorderInstalled;
