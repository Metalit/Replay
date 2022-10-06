#include "Main.hpp"
#include "Formats/FrameReplay.hpp"
#include "MathUtils.hpp"

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

Transform ConvertEulerTransform(EulerTransform& trans) {
    Transform ret(trans.position, {});
    ret.rotation = UnityEngine::Quaternion::Euler(trans.rotation);
    return ret;
}

#define READ_TO(name) input.read(reinterpret_cast<char*>(&name), sizeof(decltype(name)))

ReplayWrapper ReadFromV1(std::ifstream& input) {
    auto replay = new FrameReplay();
    ReplayWrapper ret(ReplayType::Frame, replay);

    auto modifiers = V1Modifiers();
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    replay->info.reached0Energy = modifiers.noFail;
    replay->info.failed = false;

    replay->info.hasYOffset = false;
    auto frame = V1KeyFrame();
    while(READ_TO(frame)) {
        frame.head.rotation = frame.head.rotation * 90;
        replay->scoreFrames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, 0));
        replay->frames.emplace_back(Frame(ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    replay->info.score = frame.score;

    return ret;
}

// changed modifier order, added version header, added jump offset to keyframes
ReplayWrapper ReadFromV2(std::ifstream& input) {
    auto replay = new FrameReplay();
    ReplayWrapper ret(ReplayType::Frame, replay);

    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    replay->info.reached0Energy = modifiers.noFail;
    replay->info.failed = false;

    replay->info.hasYOffset = true;
    auto frame = V2KeyFrame();
    while(READ_TO(frame)) {
        replay->scoreFrames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset));
        replay->frames.emplace_back(Frame(ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    replay->info.score = frame.score;

    return ret;
}

// added info for fails in replays (different from reaching 0 energy with no fail)
ReplayWrapper ReadFromV3(std::ifstream& input) {
    auto replay = new FrameReplay();
    ReplayWrapper ret(ReplayType::Frame, replay);

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);
    
    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    replay->info.reached0Energy = modifiers.noFail;

    replay->info.hasYOffset = true;
    auto frame = V2KeyFrame();
    while(READ_TO(frame)) {
        replay->scoreFrames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset));
        replay->frames.emplace_back(Frame(ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    replay->info.score = frame.score;

    return ret;
}

// explicitly added reached 0 energy bool and time to the replay
ReplayWrapper ReadFromV4(std::ifstream& input) {
    auto replay = new FrameReplay();
    ReplayWrapper ret(ReplayType::Frame, replay);

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);
    
    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    
    READ_TO(replay->info.reached0Energy);
    READ_TO(replay->info.reached0Time);

    replay->info.hasYOffset = true;
    auto frame = V2KeyFrame();
    while(READ_TO(frame)) {
        replay->scoreFrames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, -1, frame.jumpYOffset));
        replay->frames.emplace_back(Frame(ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    replay->info.score = frame.score;

    return ret;
}

// added energy to keyframes
ReplayWrapper ReadFromV5(std::ifstream& input) {
    auto replay = new FrameReplay();
    ReplayWrapper ret(ReplayType::Frame, replay);

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);
    
    auto modifiers = V2Modifiers();
    READ_TO(modifiers);
    replay->info.modifiers = ConvertModifiers(modifiers);
    
    READ_TO(replay->info.reached0Energy);
    READ_TO(replay->info.reached0Time);

    replay->info.hasYOffset = true;
    auto frame = V5KeyFrame();
    while(READ_TO(frame)) {
        replay->scoreFrames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, frame.energy, frame.jumpYOffset));
        replay->frames.emplace_back(Frame(ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    replay->info.score = frame.score;

    return ret;
}

// reordered modifiers again and added the new ones
ReplayWrapper ReadFromV6(std::ifstream& input) {
    auto replay = new FrameReplay();
    ReplayWrapper ret(ReplayType::Frame, replay);

    READ_TO(replay->info.failed);
    READ_TO(replay->info.failTime);
    
    auto modifiers = V6Modifiers();
    READ_TO(modifiers);
    replay->info.modifiers = modifiers;
    
    READ_TO(replay->info.reached0Energy);
    READ_TO(replay->info.reached0Time);

    replay->info.hasYOffset = true;
    auto frame = V5KeyFrame();
    while(READ_TO(frame)) {
        replay->scoreFrames.emplace_back(ScoreFrame(frame.time, frame.score, frame.percent, frame.combo, frame.energy, frame.jumpYOffset));
        replay->frames.emplace_back(Frame(ConvertEulerTransform(frame.head), ConvertEulerTransform(frame.leftSaber), ConvertEulerTransform(frame.rightSaber)));
    }
    replay->info.score = frame.score;

    return ret;
}

unsigned char fileHeader[3] = { 0xa1, 0xd2, 0x45 };

ReplayWrapper _ReadReqlay(const std::string& path) {
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

ReplayWrapper ReadReqlay(const std::string& path) {
    ReplayWrapper ret = _ReadReqlay(path);
    
    auto modified = std::filesystem::last_write_time(path);
    ret.replay->info.timestamp = std::filesystem::file_time_type::clock::to_time_t(modified);
    ret.replay->info.source = "Replay Mod (Old)";

    QuaternionAverage averageCalc(UnityEngine::Quaternion::Euler({0, 0, 0}));
    for(auto& frame : ret.replay->frames) {
        averageCalc.AddRotation(frame.head.rotation);
    }
    ret.replay->info.averageOffset = UnityEngine::Quaternion::Inverse(averageCalc.GetAverage());
    if(path.find("Degree") != std::string::npos || path.find("degree") != std::string::npos) {
        auto euler = ret.replay->info.averageOffset.get_eulerAngles();
        euler.y = 0;
        ret.replay->info.averageOffset = UnityEngine::Quaternion::Euler(euler);
    }

    return ret;
}
