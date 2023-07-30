#include "Main.hpp"
#include "Assets.hpp"

#include "questui/shared/BeatSaberUI.hpp"

#define SPRITE_FUNC(name) \
UnityEngine::Sprite* Get##name##Icon() { \
    auto arr = ArrayW<uint8_t>(name##_png::getLength()); \
    memcpy(arr->values, name##_png::getData(), name##_png::getLength()); \
    return QuestUI::BeatSaberUI::ArrayToSprite(arr); \
}

SPRITE_FUNC(Replay)
SPRITE_FUNC(Delete)
SPRITE_FUNC(Settings)
SPRITE_FUNC(White)
