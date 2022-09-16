#pragma once

#include "custom-types/shared/macros.hpp"

#include "GlobalNamespace/ISaberMovementData.hpp"
#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/ISaberMovementDataProcessor.hpp"

#define METHOD(...) il2cpp_utils::il2cpp_type_check::MetadataGetter<&GlobalNamespace::__VA_ARGS__>::get()

#define INTERFACES std::vector<Il2CppClass*>({ classof(GlobalNamespace::ISaberMovementData*), classof(GlobalNamespace::ISaberMovementDataProcessor*) })

DECLARE_CLASS_CODEGEN_INTERFACES(ReplayHelpers, MovementData, Il2CppObject, INTERFACES,

    GlobalNamespace::ISaberMovementData* baseData;
    float beforeCutRating, afterCutRating;
    GlobalNamespace::ISaberMovementDataProcessor* dataProcessor;
    
    DECLARE_OVERRIDE_METHOD(GlobalNamespace::BladeMovementDataElement, get_lastAddedData, METHOD(ISaberMovementData::get_lastAddedData));
    
    DECLARE_OVERRIDE_METHOD(void, AddDataProcessor, METHOD(ISaberMovementData::AddDataProcessor), GlobalNamespace::ISaberMovementDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD(void, RemoveDataProcessor, METHOD(ISaberMovementData::RemoveDataProcessor), GlobalNamespace::ISaberMovementDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD(void, RequestLastDataProcessing, METHOD(ISaberMovementData::RequestLastDataProcessing), GlobalNamespace::ISaberMovementDataProcessor* dataProcessor);

    DECLARE_OVERRIDE_METHOD(float, ComputeSwingRating, il2cpp_utils::FindMethodUnsafe("", "ISaberMovementData", "ComputeSwingRating", 1), float overrideSegmentAngle);
    DECLARE_OVERRIDE_METHOD(float, ComputeSwingRatingOverload, il2cpp_utils::FindMethodUnsafe("", "ISaberMovementData", "ComputeSwingRating", 0));

    using DataElement = GlobalNamespace::BladeMovementDataElement;
    DECLARE_OVERRIDE_METHOD(void, ProcessNewData, METHOD(ISaberMovementDataProcessor::ProcessNewData), DataElement newData, DataElement prevData, bool prevDataAreValid);
)

#undef METHOD
#undef INTERFACES

GlobalNamespace::ISaberMovementData* MakeFakeMovementData(GlobalNamespace::ISaberMovementData* baseData, float beforeCutRating, float afterCutRating);
