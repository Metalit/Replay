#include "CustomTypes/ScoringElement.hpp"

#include "GlobalNamespace/NoteData.hpp"
#include "main.hpp"
#include "utils.hpp"

DEFINE_TYPE(Replay, ScoringElement);

using namespace GlobalNamespace;
using Element = Replay::ScoringElement;

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

ScoringElement* MakeFakeScoringElement(Replay::Events::Note const& noteEvent) {
    NoteData::GameplayType gameplayType;
    if (noteEvent.info.eventType == Replay::Events::NoteInfo::Type::BOMB)
        gameplayType = NoteData::GameplayType::Bomb;
    else {
        switch ((NoteData::ScoringType) noteEvent.info.scoringType) {
            case NoteData::ScoringType::ChainHead:
            case NoteData::ScoringType::ChainHeadArcTail:
                gameplayType = NoteData::GameplayType::BurstSliderHead;
                break;
            case NoteData::ScoringType::ChainLink:
            case NoteData::ScoringType::ChainLinkArcHead:
                gameplayType = NoteData::GameplayType::BurstSliderElement;
                break;
            default:
                gameplayType = NoteData::GameplayType::Normal;
                break;
        }
    }
    NoteData::ScoringType scoringType = noteEvent.info.scoringType;
    if ((int) scoringType == -2)
        scoringType = NoteData::ScoringType::Normal;
    int lineIndex = noteEvent.info.lineIndex;
    int noteLineLayer = noteEvent.info.lineLayer;
    ColorType colorType = noteEvent.info.colorType;
    NoteCutDirection cutDirection = noteEvent.info.cutDirection;
    float time = noteEvent.time;

    // hopefully nothing needs the uninitialized fields
    auto noteData =
        NoteData::New_ctor(time, 0, 0, lineIndex, noteLineLayer, noteLineLayer, gameplayType, scoringType, colorType, cutDirection, 0, 0, 0, 0, 0, 0);

    auto scoringElement = il2cpp_utils::New<Element*>().value();
    scoringElement->noteData = noteData;
    scoringElement->isFinished = true;
    switch (noteEvent.info.eventType) {
        case Replay::Events::NoteInfo::Type::GOOD:
            scoringElement->cutScore = Utils::ScoreForNote(noteEvent);
            scoringElement->executionOrder = scoringElement->maxPossibleCutScore;
            scoringElement->multiplierType = Element::MultiplierType::Positive;
            scoringElement->bestMultiplierType = Element::MultiplierType::Positive;
            break;
        case Replay::Events::NoteInfo::Type::BOMB:
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
