#pragma once

#include "beatsaber-hook/shared/utils/logging.hpp"

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

#include "sombrero/shared/FastQuaternion.hpp"
#include "sombrero/shared/FastVector3.hpp"

using Vector3 = Sombrero::FastVector3;
using Quaternion = Sombrero::FastQuaternion;

extern modloader::ModInfo modInfo;

constexpr auto RendersFolder = "/sdcard/Renders";

void SelectLevelOnNextSongRefresh(bool render = true, int idx = 0);

extern bool recorderInstalled;
