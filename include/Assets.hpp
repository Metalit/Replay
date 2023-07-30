#define DECLARE_FILE(name, prefix) extern "C" uint8_t _binary_##name##_start[]; extern "C" uint8_t _binary_##name##_end[]; struct prefix##name { static size_t getLength() { return _binary_##name##_end - _binary_##name##_start; } static uint8_t* getData() { return _binary_##name##_start; } };
DECLARE_FILE(Delete_png,)
DECLARE_FILE(Ding_wav,)
DECLARE_FILE(Replay_png,)
DECLARE_FILE(Settings_png,)
DECLARE_FILE(White_png,)
