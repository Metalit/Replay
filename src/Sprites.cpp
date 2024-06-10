#include "Assets.hpp"
#include "Main.hpp"
#include "bsml/shared/Helpers/utilities.hpp"

#define SPRITE_FUNC(name) \
UnityEngine::Sprite* Get##name##Icon() { \
    auto arr = ArrayW<uint8_t>(name##_png::getLength()); \
    memcpy(arr->_values, name##_png::getData(), name##_png::getLength()); \
    return BSML::Utilities::LoadSpriteRaw(arr); \
}

SPRITE_FUNC(Replay)
SPRITE_FUNC(Delete)
SPRITE_FUNC(Settings)
SPRITE_FUNC(White)
