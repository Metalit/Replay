#pragma once

#include "custom-types/shared/macros.hpp"

#include "GlobalNamespace/ISaberMovementData.hpp"
#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/ISaberMovementDataProcessor.hpp"

#define METHOD(...) il2cpp_utils::il2cpp_type_check::MetadataGetter<&GlobalNamespace::ISaberMovementData::__VA_ARGS__>::get()

DECLARE_CLASS_CODEGEN_INTERFACES(ReplayHelpers, MovementData, Il2CppObject, classof(GlobalNamespace::ISaberMovementData*),

    GlobalNamespace::BladeMovementDataElement baseElement;
    float beforeCutRating, afterCutRating;
    GlobalNamespace::ISaberMovementDataProcessor* dataProcessor;
    
    DECLARE_OVERRIDE_METHOD(GlobalNamespace::BladeMovementDataElement, get_lastAddedData, METHOD(get_lastAddedData));
    
    DECLARE_OVERRIDE_METHOD(void, AddDataProcessor, METHOD(AddDataProcessor), GlobalNamespace::ISaberMovementDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD(void, RemoveDataProcessor, METHOD(RemoveDataProcessor), GlobalNamespace::ISaberMovementDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD(void, RequestLastDataProcessing, METHOD(RequestLastDataProcessing), GlobalNamespace::ISaberMovementDataProcessor* dataProcessor);

    DECLARE_OVERRIDE_METHOD(float, ComputeSwingRating, il2cpp_utils::FindMethodUnsafe("", "ISaberMovementData", "ComputeSwingRating", 1), float overrideSegmentAngle);
    DECLARE_OVERRIDE_METHOD(float, ComputeSwingRatingOverload, il2cpp_utils::FindMethodUnsafe("", "ISaberMovementData", "ComputeSwingRating", 0));
)

GlobalNamespace::ISaberMovementData* MakeFakeMovementData(GlobalNamespace::ISaberMovementData* baseData, float beforeCutRating, float afterCutRating);
