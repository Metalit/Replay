#include "Main.hpp"
#include "CustomTypes/MovementData.hpp"

#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/SaberSwingRatingCounter.hpp"

DEFINE_TYPE(ReplayHelpers, MovementData);

using namespace ReplayHelpers;
using namespace GlobalNamespace;

BladeMovementDataElement MovementData::get_lastAddedData() {
    return baseData->get_lastAddedData();
}

void MovementData::AddDataProcessor(ISaberMovementDataProcessor* dataProcessor) {
    this->dataProcessor = dataProcessor;
}

void MovementData::RemoveDataProcessor(ISaberMovementDataProcessor* dataProcessor) {
    this->dataProcessor = nullptr;
}

void MovementData::RequestLastDataProcessing(ISaberMovementDataProcessor* dataProcessor) {
    // set notePlaneWasCut and other values so AfterCutStepRating is calculated properly
    // that way overswing counting will "work" (only if the replay stores the overswing, of course)
    auto baseElement = get_lastAddedData();
    auto counter = (SaberSwingRatingCounter*) dataProcessor;
    counter->notePlaneWasCut = true;
    counter->cutPlaneNormal = baseElement.segmentNormal;
    baseElement.segmentAngle = afterCutRating * 60;
    counter->ProcessNewData(baseElement, baseElement, true);
    // register as a data processor and wait for the next data so that the counter can finish properly
    baseData->AddDataProcessor((ISaberMovementDataProcessor*) this);
}

float MovementData::ComputeSwingRating(float overrideSegmentAngle) {
    return beforeCutRating;
}

float MovementData::ComputeSwingRatingOverload() {
    return beforeCutRating;
}

void MovementData::ProcessNewData(BladeMovementDataElement newData, BladeMovementDataElement prevData, bool prevDataAreValid) {
    auto counter = (SaberSwingRatingCounter*) dataProcessor;
    if(counter) {
        if(counter->beforeCutRating > 1)
            counter->beforeCutRating = 1;
        counter->Finish();
    }
    baseData->RemoveDataProcessor((ISaberMovementDataProcessor*) this);
}

ISaberMovementData* MakeFakeMovementData(ISaberMovementData* baseData, float beforeCutRating, float afterCutRating) {
    auto movementData = CRASH_UNLESS(il2cpp_utils::New<MovementData*>());
    movementData->baseData = baseData;
    movementData->beforeCutRating = beforeCutRating;
    movementData->afterCutRating = afterCutRating;
    movementData->dataProcessor = nullptr;
    return (ISaberMovementData*) movementData;
}
