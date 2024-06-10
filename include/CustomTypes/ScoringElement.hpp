#pragma once

#include "GlobalNamespace/ScoreMultiplierCounter.hpp"
#include "GlobalNamespace/ScoringElement.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN(ReplayHelpers, ScoringElement, GlobalNamespace::ScoringElement,
    using MultiplierType = GlobalNamespace::ScoreMultiplierCounter::MultiplierEventType;

   public:
    int cutScore, executionOrder;
    MultiplierType multiplierType, bestMultiplierType;

    DECLARE_OVERRIDE_METHOD_MATCH(int, get_cutScore, &GlobalNamespace::ScoringElement::get_cutScore);

    DECLARE_OVERRIDE_METHOD_MATCH(MultiplierType, get_multiplierEventType, &GlobalNamespace::ScoringElement::get_multiplierEventType);
    DECLARE_OVERRIDE_METHOD_MATCH(MultiplierType, get_wouldBeCorrectCutBestPossibleMultiplierEventType, &GlobalNamespace::ScoringElement::get_wouldBeCorrectCutBestPossibleMultiplierEventType);

    DECLARE_OVERRIDE_METHOD_MATCH(int, get_executionOrder, &GlobalNamespace::ScoringElement::get_executionOrder);
)

GlobalNamespace::ScoringElement* MakeFakeScoringElement(const class NoteEvent& noteEvent);
