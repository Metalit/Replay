#pragma once

#include "GlobalNamespace/BeatmapKey.hpp"
#include "GlobalNamespace/IReadonlyBeatmapData.hpp"
#include "replay.hpp"

namespace Parsing {
    struct Exception : std::exception {
        std::string message;
        explicit Exception(std::string const& message) : message(message) {}
        char const* what() const noexcept override { return message.c_str(); }
    };

    std::shared_ptr<Replay::Data> ReadReqlay(std::string const& path);
    std::shared_ptr<Replay::Data> ReadScoresaber(std::string const& path);
    std::shared_ptr<Replay::Data> ReadBSOR(std::string const& path);

    std::string GetFullHash(std::istream& input);

    std::string ReadString(std::istream& input);
    std::string ReadStringUTF16(std::istream& input);

    void CheckErrorState(std::istream& input, std::string hint = "unspecified");

    std::vector<std::pair<std::string, std::shared_ptr<Replay::Data>>> GetReplays(GlobalNamespace::BeatmapKey beatmap);

    void PreProcess(Replay::Data& replay);
    void CheckForQuit(Replay::Info& info, float songLength);
    void RecalculateNotes(Replay::Data& replay, GlobalNamespace::IReadonlyBeatmapData* beatmapData);
}

#define READ_TO(name)                                                   \
    input.read(reinterpret_cast<char*>(&name), sizeof(decltype(name))); \
    Parsing::CheckErrorState(input, #name)

#define READ_STRING(name)              \
    name = Parsing::ReadString(input); \
    Parsing::CheckErrorState(input, #name)

#define READ_UTF16(name)                    \
    name = Parsing::ReadStringUTF16(input); \
    Parsing::CheckErrorState(input, #name)
