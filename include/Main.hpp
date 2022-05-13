#pragma once

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "paper/shared/logger.hpp"

#define LOG_INFO(string, ...) Paper::Logger::fmtLogTag<Paper::LogLevel::INF>(string, "Replay" __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR(string, ...) Paper::Logger::fmtLogTag<Paper::LogLevel::ERR>(string, "Replay" __VA_OPT__(,) __VA_ARGS__)
