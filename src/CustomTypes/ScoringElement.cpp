#include "CustomTypes/ScoringElement.hpp"

#include "Formats/EventReplay.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "Main.hpp"
#include "Utils.hpp"

DEFINE_TYPE(ReplayHelpers, ScoringElement);

using Element = ReplayHelpers::ScoringElement;
using namespace GlobalNamespace;

int Element::get_cutScore() {
    return cutScore;
}

Element::MultiplierType Element::get_multiplierEventType() {
    return multiplierType;
}

Element::MultiplierType Element::get_wouldBeCorrectCutBestPossibleMultiplierEventType() {
    return bestMultiplierType;
}

int Element::get_executionOrder() {
    return executionOrder;
}

ScoringElement* MakeFakeScoringElement(NoteEvent const& noteEvent) {
    auto noteData = CRASH_UNLESS(il2cpp_utils::New<NoteData*>());
    if (noteEvent.info.eventType == NoteEventInfo::Type::BOMB)
        noteData->gameplayType = NoteData::GameplayType::Bomb;
    else {
        switch ((NoteData::ScoringType) noteEvent.info.scoringType) {
            case NoteData::ScoringType::BurstSliderHead:
                noteData->gameplayType = NoteData::GameplayType::BurstSliderHead;
                break;
            case NoteData::ScoringType::BurstSliderElement:
                noteData->gameplayType = NoteData::GameplayType::BurstSliderElement;
                break;
            default:
                noteData->gameplayType = NoteData::GameplayType::Normal;
                break;
        }
    }
    noteData->scoringType = noteEvent.info.scoringType;
    if ((int) noteData->scoringType == -2)
        noteData->scoringType = NoteData::ScoringType::Normal;
    noteData->lineIndex = noteEvent.info.lineIndex;
    noteData->noteLineLayer = noteEvent.info.lineLayer;
    noteData->colorType = noteEvent.info.colorType;
    noteData->cutDirection = noteEvent.info.cutDirection;
    noteData->_time_k__BackingField = noteEvent.time;

    auto scoringElement = CRASH_UNLESS(il2cpp_utils::New<Element*>());
    scoringElement->noteData = noteData;
    scoringElement->isFinished = true;
    switch (noteEvent.info.eventType) {
        case NoteEventInfo::Type::GOOD:
            scoringElement->cutScore = ScoreForNote(noteEvent);
            scoringElement->executionOrder = scoringElement->get_maxPossibleCutScore();
            scoringElement->multiplierType = Element::MultiplierType::Positive;
            scoringElement->bestMultiplierType = Element::MultiplierType::Positive;
            break;
        case NoteEventInfo::Type::BOMB:
            scoringElement->cutScore = 0;
            scoringElement->executionOrder = 100000;
            scoringElement->multiplierType = Element::MultiplierType::Negative;
            scoringElement->bestMultiplierType = Element::MultiplierType::Neutral;
            break;
        default:
            scoringElement->cutScore = 0;
            scoringElement->executionOrder = 100000;
            scoringElement->multiplierType = Element::MultiplierType::Negative;
            scoringElement->bestMultiplierType = Element::MultiplierType::Positive;
            break;
    }
    return (ScoringElement*) scoringElement;
}
