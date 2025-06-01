#pragma once

#include "GlobalNamespace/ScoreMultiplierCounter.hpp"
#include "GlobalNamespace/ScoringElement.hpp"
#include "custom-types/shared/macros.hpp"
#include "replay.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, ScoringElement, GlobalNamespace::ScoringElement) {
    DECLARE_DEFAULT_CTOR();

   public:
    using MultiplierType = GlobalNamespace::ScoreMultiplierCounter::MultiplierEventType;

    DECLARE_OVERRIDE_METHOD_MATCH(int, get_cutScore, &GlobalNamespace::ScoringElement::get_cutScore);
    DECLARE_OVERRIDE_METHOD_MATCH(MultiplierType, get_multiplierEventType, &GlobalNamespace::ScoringElement::get_multiplierEventType);
    DECLARE_OVERRIDE_METHOD_MATCH(
        MultiplierType,
        get_wouldBeCorrectCutBestPossibleMultiplierEventType,
        &GlobalNamespace::ScoringElement::get_wouldBeCorrectCutBestPossibleMultiplierEventType
    );
    DECLARE_OVERRIDE_METHOD_MATCH(int, get_executionOrder, &GlobalNamespace::ScoringElement::get_executionOrder);

    DECLARE_INSTANCE_FIELD(int, cutScore);
    DECLARE_INSTANCE_FIELD(int, executionOrder);
    DECLARE_INSTANCE_FIELD(MultiplierType, multiplierType);
    DECLARE_INSTANCE_FIELD(MultiplierType, bestMultiplierType);
};

GlobalNamespace::ScoringElement* MakeFakeScoringElement(Replay::Events::Note& noteEvent);
