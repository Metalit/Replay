#pragma once

#include "paper2_scotland2/shared/logger.hpp"

static constexpr auto logger = Paper::ConstLoggerContext(MOD_ID);

constexpr auto RendersFolder = "/sdcard/Renders";

#include "sombrero/shared/FastQuaternion.hpp"
#include "sombrero/shared/FastVector3.hpp"

using Vector3 = Sombrero::FastVector3;
using Quaternion = Sombrero::FastQuaternion;

extern bool recorderInstalled;
