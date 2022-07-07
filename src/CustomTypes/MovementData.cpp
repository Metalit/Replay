#include "Main.hpp"
#include "CustomTypes/MovementData.hpp"

#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/SaberSwingRatingCounter.hpp"

DEFINE_TYPE(ReplayHelpers, MovementData);

using namespace ReplayHelpers;
using namespace GlobalNamespace;

BladeMovementDataElement MovementData::get_lastAddedData() {
    return baseElement;
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
    auto counter = (SaberSwingRatingCounter*) dataProcessor;
    counter->notePlaneWasCut = true;
    counter->cutPlaneNormal = baseElement.segmentNormal;
    baseElement.segmentAngle = afterCutRating * 60;
    counter->ProcessNewData(baseElement, baseElement, true);
    counter->Finish();
}

float MovementData::ComputeSwingRating(float overrideSegmentAngle) {
    return beforeCutRating;
}

float MovementData::ComputeSwingRatingOverload() {
    return beforeCutRating;
}

ISaberMovementData* MakeFakeMovementData(ISaberMovementData* baseData, float beforeCutRating, float afterCutRating) {
    auto movementData = CRASH_UNLESS(il2cpp_utils::New<MovementData*>());
    movementData->baseElement = baseData->get_lastAddedData();
    movementData->beforeCutRating = beforeCutRating;
    movementData->afterCutRating = afterCutRating;
    return (ISaberMovementData*) movementData;
}
