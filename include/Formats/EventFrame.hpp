#pragma once

#include "Formats/EventReplay.hpp"
#include "Formats/FrameReplay.hpp"

struct EventFrame : public virtual EventReplay, public virtual FrameReplay {};

ReplayWrapper ReadScoresaber(std::string const& path);
