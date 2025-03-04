#pragma once

#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/ISaberMovementData.hpp"
#include "GlobalNamespace/ISaberMovementDataProcessor.hpp"
#include "custom-types/shared/macros.hpp"

DECLARE_CLASS_CODEGEN_INTERFACES(ReplayHelpers, MovementData, Il2CppObject, GlobalNamespace::ISaberMovementData*, GlobalNamespace::ISaberMovementDataProcessor*) {
    DECLARE_DEFAULT_CTOR();

   public:
    using DataElement = GlobalNamespace::BladeMovementDataElement;
    using IData = GlobalNamespace::ISaberMovementData;
    using IDataProcessor = GlobalNamespace::ISaberMovementDataProcessor;

    IData* baseData;
    float beforeCutRating, afterCutRating;
    IDataProcessor* dataProcessor;

    DECLARE_OVERRIDE_METHOD_MATCH(DataElement, get_lastAddedData, &IData::get_lastAddedData);

    DECLARE_OVERRIDE_METHOD_MATCH(void, AddDataProcessor, &IData::AddDataProcessor, IDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, RemoveDataProcessor, &IData::RemoveDataProcessor, IDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, RequestLastDataProcessing, &IData::RequestLastDataProcessing, IDataProcessor* dataProcessor);

    DECLARE_OVERRIDE_METHOD(
        float, ComputeSwingRating, il2cpp_utils::FindMethodUnsafe("", "ISaberMovementData", "ComputeSwingRating", 1), float overrideSegmentAngle
    );
    DECLARE_OVERRIDE_METHOD(float, ComputeSwingRatingOverload, il2cpp_utils::FindMethodUnsafe("", "ISaberMovementData", "ComputeSwingRating", 0));

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, ProcessNewData, &IDataProcessor::ProcessNewData, DataElement newData, DataElement prevData, bool prevDataAreValid
    );
};

GlobalNamespace::ISaberMovementData* MakeFakeMovementData(GlobalNamespace::ISaberMovementData* baseData, float beforeCutRating, float afterCutRating);
