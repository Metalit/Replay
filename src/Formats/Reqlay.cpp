#include "Main.hpp"
#include "Formats/FrameReplay.hpp"

#include <fstream>

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

typedef ReplayModifiers V6Modifiers;

struct FailInfo {
    bool failed;
    float time = 0;
};

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

template<class T>
ReplayModifiers ConvertModifiers(const T& modifiers) {
    ReplayModifiers ret;
    ret.disappearingArrows = modifiers.disappearingArrows;
    ret.fasterSong = modifiers.fasterSong;
    ret.slowerSong = modifiers.slowerSong;
    if constexpr(std::is_same_v<T, V6Modifiers>) {
        ret.superFastSong = modifiers.superFastSong;
        ret.strictAngles = modifiers.strictAngles;
        ret.proMode = modifiers.proMode;
        ret.smallNotes = modifiers.smallNotes;
    } else {
        ret.superFastSong = false;
        ret.strictAngles = false;
        ret.proMode = false;
        ret.smallNotes = false;
    }
    ret.ghostNotes = modifiers.ghostNotes;
    ret.noArrows = modifiers.noArrows;
    ret.noBombs = modifiers.noBombs;
    ret.noObstacles = modifiers.noObstacles;
    ret.noFail = modifiers.noFail;
    return ret;
}

Transform ConvertEulerTransform(EulerTransform& trans) {
    Transform ret(trans.position, {});
    ret.rotation = UnityEngine::Quaternion::Euler(trans.rotation);
    return ret;
}

#define READ_TO(name) input.read(reinterpret_cast<char*>(&name), sizeof(decltype(name)))

unsigned char fileHeader[3] = { 0xa1, 0xd2, 0x45 };

FrameReplay ReadFromV1(std::ifstream& input) {
    FrameReplay ret;

    auto modifiers = V1Modifiers();
    READ_TO(modifiers);
    ret.info.modifiers = ConvertModifiers(modifiers);
    ret.info.reached0Energy = modifiers.noFail;
    ret.info.failed = false;

    ret.info.hasYOffset = false;
    auto frame = V1KeyFrame();
    while(READ_TO(frame)) {
        frame.head.rotation = frame.head.rotation * 90;
        ret.frames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, 0,
            ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    ret.info.score = frame.score;

    return ret;
}

// changed modifier order, added version header, added jump offset to keyframes
FrameReplay ReadFromV2(std::ifstream& input) {
    FrameReplay ret;

    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    ret.info.modifiers = ConvertModifiers(modifiers);
    ret.info.reached0Energy = modifiers.noFail;
    ret.info.failed = false;

    ret.info.hasYOffset = true;
    auto frame = V2KeyFrame();
    while(READ_TO(frame)) {
        ret.frames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset,
            ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    ret.info.score = frame.score;

    return ret;
}

// added info for fails in replays (different from reaching 0 energy with no fail)
FrameReplay ReadFromV3(std::ifstream& input) {
    FrameReplay ret;

    FailInfo fail;
    READ_TO(fail);
    ret.info.failed = fail.failed;
    ret.info.failTime = fail.time;
    
    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    ret.info.modifiers = ConvertModifiers(modifiers);
    ret.info.reached0Energy = modifiers.noFail;

    ret.info.hasYOffset = true;
    auto frame = V2KeyFrame();
    while(READ_TO(frame)) {
        ret.frames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset,
            ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    ret.info.score = frame.score;

    return ret;
}

// explicitly added reached 0 energy bool and time to the replay
FrameReplay ReadFromV4(std::ifstream& input) {
    FrameReplay ret;

    FailInfo fail;
    READ_TO(fail);
    ret.info.failed = fail.failed;
    ret.info.failTime = fail.time;
    
    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    ret.info.modifiers = ConvertModifiers(modifiers);
    
    FailInfo reached0;
    READ_TO(reached0);
    ret.info.reached0Energy = reached0.failed;
    ret.info.reached0Time = reached0.time;

    ret.info.hasYOffset = true;
    auto frame = V2KeyFrame();
    while(READ_TO(frame)) {
        ret.frames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset,
            ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    ret.info.score = frame.score;

    return ret;
}

// added energy to keyframes
FrameReplay ReadFromV5(std::ifstream& input) {
    FrameReplay ret;

    FailInfo fail;
    READ_TO(fail);
    ret.info.failed = fail.failed;
    ret.info.failTime = fail.time;
    
    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    ret.info.modifiers = ConvertModifiers(modifiers);
    
    FailInfo reached0;
    READ_TO(reached0);
    ret.info.reached0Energy = reached0.failed;
    ret.info.reached0Time = reached0.time;

    ret.info.hasYOffset = true;
    auto frame = V5KeyFrame();
    while(READ_TO(frame)) {
        ret.frames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, frame.energy, frame.jumpYOffset,
            ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    ret.info.score = frame.score;

    return ret;
}

// reordered modifiers again and added the new ones
FrameReplay ReadFromV6(std::ifstream& input) {
    FrameReplay ret;

    FailInfo fail;
    READ_TO(fail);
    ret.info.failed = fail.failed;
    ret.info.failTime = fail.time;
    
    auto modifiers = V6Modifiers();
    READ_TO(modifiers);
    ret.info.modifiers = ConvertModifiers(modifiers);
    
    FailInfo reached0;
    READ_TO(reached0);
    ret.info.reached0Energy = reached0.failed;
    ret.info.reached0Time = reached0.time;

    ret.info.hasYOffset = true;
    auto frame = V5KeyFrame();
    while(READ_TO(frame)) {
        ret.frames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, frame.energy, frame.jumpYOffset,
            ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    ret.info.score = frame.score;

    return ret;
}

FrameReplay _ReadReqlay(const std::string& path) {
    std::ifstream input(path, std::ios::binary);

    if(!input.is_open()) {
        LOG_ERROR("Failure opening file {}", path);
        return {};
    }

    unsigned char headerBytes[3];
    for(int i = 0; i < 3; i++) {
        READ_TO(headerBytes[i]);
        if(headerBytes[i] != fileHeader[i]) {
            input.seekg(0);
            LOG_INFO("Reading reqlay file with version 1");
            return ReadFromV1(input);
        }
    }

    int version;
    READ_TO(version);
    LOG_INFO("Reading reqlay file with version {}", version);
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
        LOG_ERROR("Unsupported version! Found version {} in file {}", version, path);
        return {};
    }
}

FrameReplay ReadReqlay(const std::string& path) {
    FrameReplay ret = _ReadReqlay(path);
    
    auto modified = std::filesystem::last_write_time(path);
    ret.info.timestamp = std::filesystem::file_time_type::clock::to_time_t(modified);
    ret.info.source = "Replay Mod (Old)";

    return ret;
}
