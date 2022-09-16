#pragma once

#include "custom-types/shared/macros.hpp"

#include "GlobalNamespace/ScoringElement.hpp"
#include "GlobalNamespace/ScoreMultiplierCounter_MultiplierEventType.hpp"

#define METHOD(...) il2cpp_utils::il2cpp_type_check::MetadataGetter<&GlobalNamespace::__VA_ARGS__>::get()

DECLARE_CLASS_CODEGEN(ReplayHelpers, ScoringElement, GlobalNamespace::ScoringElement,
    public:
    using MultiplierType = GlobalNamespace::ScoreMultiplierCounter::MultiplierEventType;

    int cutScore, executionOrder;
    MultiplierType multiplierType, bestMultiplierType;

    DECLARE_OVERRIDE_METHOD(int, get_cutScore, METHOD(ScoringElement::get_cutScore));

    DECLARE_OVERRIDE_METHOD(MultiplierType, get_multiplierEventType, METHOD(ScoringElement::get_multiplierEventType));
    DECLARE_OVERRIDE_METHOD(MultiplierType, get_wouldBeCorrectCutBestPossibleMultiplierEventType, METHOD(ScoringElement::get_wouldBeCorrectCutBestPossibleMultiplierEventType));

    DECLARE_OVERRIDE_METHOD(int, get_executionOrder, METHOD(ScoringElement::get_executionOrder));
)

#undef METHOD

GlobalNamespace::ScoringElement* MakeFakeScoringElement(const class NoteEvent& noteEvent);
