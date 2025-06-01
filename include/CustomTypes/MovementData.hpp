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

    DECLARE_OVERRIDE_METHOD_MATCH(DataElement, get_lastAddedData, &IData::get_lastAddedData);

    DECLARE_OVERRIDE_METHOD_MATCH(void, AddDataProcessor, &IData::AddDataProcessor, IDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, RemoveDataProcessor, &IData::RemoveDataProcessor, IDataProcessor* dataProcessor);
    DECLARE_OVERRIDE_METHOD_MATCH(void, RequestLastDataProcessing, &IData::RequestLastDataProcessing, IDataProcessor* dataProcessor);

    DECLARE_OVERRIDE_METHOD_MATCH(
        float, ComputeSwingRating, static_cast<float (IData::*)(float)>(&IData::ComputeSwingRating), float overrideSegmentAngle
    );
    DECLARE_OVERRIDE_METHOD_MATCH(float, ComputeSwingRatingOverload, static_cast<float (IData::*)()>(&IData::ComputeSwingRating));

    DECLARE_OVERRIDE_METHOD_MATCH(
        void, ProcessNewData, &IDataProcessor::ProcessNewData, DataElement newData, DataElement prevData, bool prevDataAreValid
    );

    DECLARE_INSTANCE_FIELD(IData*, baseData);
    DECLARE_INSTANCE_FIELD(float, beforeCutRating);
    DECLARE_INSTANCE_FIELD(float, afterCutRating);
    DECLARE_INSTANCE_FIELD(IDataProcessor*, dataProcessor);

    DECLARE_STATIC_METHOD(GlobalNamespace::ISaberMovementData*, Create, GlobalNamespace::ISaberMovementData * base, float before, float after);
};
