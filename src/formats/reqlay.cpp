#include "math.hpp"
#include "metacore/shared/unity.hpp"
#include "parsing.hpp"

// loading code for henwill's old replay versions

struct V1Modifiers {
    bool oneLife;
    bool fourLives;
    bool ghostNotes;
    bool disappearingArrows;
    bool fasterSong;
    bool noFail;
    bool noBombs;
    bool noObstacles;
    bool noArrows;
    bool slowerSong;
    bool leftHanded;
};

struct V2Modifiers {
    bool fourLives;
    bool disappearingArrows;
    bool noObstacles;
    bool noBombs;
    bool noArrows;
    bool slowerSong;
    bool noFail;
    bool oneLife;
    bool ghostNotes;
    bool fasterSong;
    bool leftHanded;
};

typedef Replay::Modifiers V6Modifiers;

struct EulerTransform {
    Vector3 position;
    Vector3 rotation;
};

struct V1KeyFrame {
    EulerTransform rightSaber;
    EulerTransform leftSaber;
    EulerTransform head;

    int score;
    float percent;
    int rank;
    int combo;
    float time;
};

struct V2KeyFrame {
    EulerTransform rightSaber;
    EulerTransform leftSaber;
    EulerTransform head;

    int score;
    float percent;
    int rank;
    int combo;
    float time;
    float jumpYOffset = -1;
};

struct V5KeyFrame {
    EulerTransform rightSaber;
    EulerTransform leftSaber;
    EulerTransform head;

    int score;
    float percent;
    int rank;
    int combo;
    float time;
    float jumpYOffset = -1;
    float energy = -1;
};

template <class T>
Replay::Modifiers ConvertModifiers(T const& modifiers) {
    Replay::Modifiers ret;
    ret.disappearingArrows = modifiers.disappearingArrows;
    ret.fasterSong = modifiers.fasterSong;
    ret.slowerSong = modifiers.slowerSong;
    ret.ghostNotes = modifiers.ghostNotes;
    ret.noArrows = modifiers.noArrows;
    ret.noBombs = modifiers.noBombs;
    ret.noObstacles = modifiers.noObstacles;
    ret.noFail = modifiers.noFail;
    ret.oneLife = modifiers.oneLife;
    ret.fourLives = modifiers.fourLives;
    ret.leftHanded = modifiers.leftHanded;
    ret.superFastSong = false;
    ret.strictAngles = false;
    ret.proMode = false;
    ret.smallNotes = false;
    return ret;
}

Replay::Transform ConvertTransform(EulerTransform& euler) {
    Replay::Transform ret(euler.position, {});
    ret.rotation = UnityEngine::Quaternion::Euler(euler.rotation);
    return ret;
}

std::shared_ptr<Replay::Data> ReadFromV1(std::ifstream& input) {
    auto replay = std::make_shared<Replay::Data>();
    replay->frames.emplace();

    V1Modifiers modifiers;
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    replay->info.reached0Energy = modifiers.noFail;
    replay->info.failed = false;

    V1KeyFrame frame;
    while (READ_TO(frame)) {
        frame.head.rotation = frame.head.rotation * 90;
        replay->frames->scores.emplace_back(frame.time, frame.score, frame.percent, frame.combo, -1, -1, -1, -1);
        replay->poses.emplace_back(ConvertTransform(frame.head), ConvertTransform(frame.leftSaber), ConvertTransform(frame.rightSaber));
    }

    replay->info.hasYOffset = false;
    replay->info.score = frame.score;

    return replay;
}

// changed modifier order, added version header, added jump offset to keyframes
std::shared_ptr<Replay::Data> ReadFromV2(std::ifstream& input) {
    auto replay = std::make_shared<Replay::Data>();
    replay->frames.emplace();

    V2Modifiers modifiers;
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    replay->info.reached0Energy = modifiers.noFail;
    replay->info.failed = false;

    V2KeyFrame frame;
    while (READ_TO(frame)) {
        replay->frames->scores.emplace_back(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset, -1, -1);
        replay->poses.emplace_back(ConvertTransform(frame.head), ConvertTransform(frame.leftSaber), ConvertTransform(frame.rightSaber));
    }

    replay->info.hasYOffset = true;
    replay->info.score = frame.score;

    return replay;
}

// added info for fails in replays (different from reaching 0 energy with no fail)
std::shared_ptr<Replay::Data> ReadFromV3(std::ifstream& input) {
    auto replay = std::make_shared<Replay::Data>();
    replay->frames.emplace();

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);

    V2Modifiers modifiers;
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    replay->info.reached0Energy = modifiers.noFail;

    V2KeyFrame frame;
    while (READ_TO(frame)) {
        replay->frames->scores.emplace_back(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset, -1, -1);
        replay->poses.emplace_back(ConvertTransform(frame.head), ConvertTransform(frame.leftSaber), ConvertTransform(frame.rightSaber));
    }

    replay->info.hasYOffset = true;
    replay->info.score = frame.score;

    return replay;
}

// explicitly added reached 0 energy bool and time to the replay
std::shared_ptr<Replay::Data> ReadFromV4(std::ifstream& input) {
    auto replay = std::make_shared<Replay::Data>();
    replay->frames.emplace();

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);

    V2Modifiers modifiers;
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);

    READ_TO(replay->info.reached0Energy);
    READ_TO(replay->info.reached0Time);

    V2KeyFrame frame;
    while (READ_TO(frame)) {
        replay->frames->scores.emplace_back(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset, -1, -1);
        replay->poses.emplace_back(ConvertTransform(frame.head), ConvertTransform(frame.leftSaber), ConvertTransform(frame.rightSaber));
    }

    replay->info.hasYOffset = true;
    replay->info.score = frame.score;

    return replay;
}

// added energy to keyframes
std::shared_ptr<Replay::Data> ReadFromV5(std::ifstream& input) {
    auto replay = std::make_shared<Replay::Data>();
    replay->frames.emplace();

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);

    V2Modifiers modifiers;
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);

    READ_TO(replay->info.reached0Energy);
    READ_TO(replay->info.reached0Time);

    V5KeyFrame frame;
    while (READ_TO(frame)) {
        replay->frames->scores.emplace_back(frame.time, frame.score, frame.percent, frame.combo, frame.energy, frame.jumpYOffset, -1, -1);
        replay->poses.emplace_back(ConvertTransform(frame.head), ConvertTransform(frame.leftSaber), ConvertTransform(frame.rightSaber));
    }

    replay->info.hasYOffset = true;
    replay->info.score = frame.score;

    return replay;
}

// reordered modifiers again and added the new ones
std::shared_ptr<Replay::Data> ReadFromV6(std::ifstream& input) {
    auto replay = std::make_shared<Replay::Data>();
    replay->frames.emplace();

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);

    V6Modifiers modifiers;
    READ_TO(modifiers);
    replay->info.modifiers = modifiers;

    READ_TO(replay->info.reached0Energy);
    READ_TO(replay->info.reached0Time);

    V5KeyFrame frame;
    while (READ_TO(frame)) {
        replay->frames->scores.emplace_back(frame.time, frame.score, frame.percent, frame.combo, frame.energy, frame.jumpYOffset, -1, -1);
        replay->poses.emplace_back(ConvertTransform(frame.head), ConvertTransform(frame.leftSaber), ConvertTransform(frame.rightSaber));
    }

    replay->info.hasYOffset = true;
    replay->info.score = frame.score;

    return replay;
}

unsigned char fileHeader[3] = {0xa1, 0xd2, 0x45};

std::shared_ptr<Replay::Data> ReadVersionedReqlay(std::string const& path) {
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);

    if (!input.is_open())
        throw Parsing::Exception("Failure opening file");

    unsigned char headerBytes[3];
    for (int i = 0; i < 3; i++) {
        READ_TO(headerBytes[i]);
        if (headerBytes[i] != fileHeader[i]) {
            input.seekg(0);
            logger.info("Reading reqlay file with version 1");
            return ReadFromV1(input);
        }
    }

    int version;
    READ_TO(version);
    logger.info("Reading reqlay file with version {}", version);

    switch (version) {
        case 2:
            return ReadFromV2(input);
        case 3:
            return ReadFromV3(input);
        case 4:
            return ReadFromV4(input);
        case 5:
            return ReadFromV5(input);
        case 6:
            return ReadFromV6(input);
        default:
            throw Parsing::Exception(fmt::format("Unsupported version {}", version));
    }
}

std::shared_ptr<Replay::Data> Parsing::ReadReqlay(std::string const& path) {
    auto replay = ReadVersionedReqlay(path);

    auto modified = std::filesystem::last_write_time(path);
    replay->info.timestamp = std::chrono::duration_cast<std::chrono::seconds>(modified.time_since_epoch()).count();
    replay->info.source = "Replay Mod (Legacy)";
    replay->info.positionsAreLocal = false;

    bool hasRotation = path.find("Degree") != std::string::npos || path.find("degree") != std::string::npos;
    MetaCore::Engine::QuaternionAverage averageCalc(Quaternion::identity(), hasRotation);

    for (auto& pose : replay->poses)
        averageCalc.AddRotation(pose.head.rotation);

    replay->info.averageOffset = UnityEngine::Quaternion::Inverse(averageCalc.GetAverage());

    PreProcess(*replay);
    return replay;
}
