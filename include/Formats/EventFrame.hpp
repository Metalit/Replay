#pragma once

#include "Formats/EventReplay.hpp"
#include "Formats/FrameReplay.hpp"

struct EventFrame : public virtual EventReplay, public virtual FrameReplay {};

ReplayWrapper ReadScoresaber(const std::string& path);
