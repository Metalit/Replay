#include "CustomTypes/MovementData.hpp"

#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/ISaberSwingRatingCounterDidChangeReceiver.hpp"
#include "GlobalNamespace/LazyCopyHashSet_1.hpp"
#include "GlobalNamespace/SaberSwingRatingCounter.hpp"
#include "Main.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"

DEFINE_TYPE(ReplayHelpers, MovementData);

using namespace ReplayHelpers;
using namespace GlobalNamespace;

BladeMovementDataElement MovementData::get_lastAddedData() {
    return baseData->lastAddedData;
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
    counter->_notePlaneWasCut = true;
    counter->_cutPlaneNormal = baseElement.segmentNormal;
    baseElement.segmentAngle = afterCutRating * 60;
    // needs to try to calculate after cut rating so that it doesn't immediately finish
    counter->_rateAfterCut = true;
    counter->_afterCutRating = 0;
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
    if (counter) {
        if (counter->_beforeCutRating > 1)
            counter->_beforeCutRating = 1;
        // run change events because the receivers aren't registered at the time of RequestLastDataProcessing
        ListW<ISaberSwingRatingCounterDidChangeReceiver*> changeEvents = counter->_didChangeReceivers->items;
        for (auto& changeEvent : changeEvents)
            changeEvent->HandleSaberSwingRatingCounterDidChange(counter->i___GlobalNamespace__ISaberSwingRatingCounter(), counter->afterCutRating);
        // thanks beatgames
        // (because they changed the score spawning to wait a frame, if the counter finishes instantly,
        // it's possible for it to be reused and have a different note cut info before the score is spawned)
        BSML::MainThreadScheduler::ScheduleNextFrame([counter]() {
            counter->Finish();  // TODO: this likely causes finishes to be in an arbitrary order, causing at least part of the scoring issue
        });
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
