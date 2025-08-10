#pragma once

#include "GlobalNamespace/BeatmapKey.hpp"
#include "replay.hpp"

namespace Manager {
    extern std::map<std::string, std::vector<std::function<void(char const*, size_t)>>> customDataCallbacks;

    void SelectLevelInConfig(int index);
    void BeginQueue();

    void SaveCurrentLevelInConfig();
    void RemoveCurrentLevelFromConfig();
    bool IsCurrentLevelInConfig();

    void SetExternalReplay(std::string path, std::shared_ptr<Replay::Data> replay);
    bool AreReplaysLocal();
    int GetReplaysCount();

    void StartReplay(bool render);
    void CameraFinished();

    Replay::Data& GetCurrentReplay();
    Replay::Info& GetCurrentInfo();
    void DeleteCurrentReplay();

    bool Replaying();
    bool Rendering();
    bool Paused();

    bool HasRotations();
    bool CancelPresentation();
}
