#include "CustomTypes/ReplayMenu.hpp"
#include "Formats/EventReplay.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "ReplayManager.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "conditional-dependencies/shared/main.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

EXPOSE_API(PlayBSORFromFile, bool, std::string filePath) {
    auto replay = ReadBSOR(filePath);
    if (replay.IsValid()) {
        auto levelView = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::StandardLevelDetailView*>()->First();

        Menu::EnsureSetup(levelView);
        Manager::SetReplays({{filePath, std::move(replay)}}, true);
        Menu::PresentMenu();

        return true;
    }
    return false;
}

EXPOSE_API(PlayBSORFromFileForced, bool, std::string filePath) {
    auto replay = ReadBSOR(filePath);
    if (replay.IsValid()) {
        auto levelView = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::StandardLevelDetailView*>()->First();

        Manager::Camera::rendering = false;
        Manager::ReplayStarted(replay);
        levelView->actionButton->get_onClick()->Invoke();

        return true;
    }
    return false;
}

EXPOSE_API(IsInReplay, bool) {
    return Manager::replaying;
}

EXPOSE_API(IsInRender, bool) {
    return Manager::replaying && Manager::Camera::rendering;
}

// every added callback will be called when replay start, if we don't have that custom data, the first parameter will be nullptr
EXPOSE_API(AddReplayCustomDataCallback, void, std::string key, std::function<void(char const*, size_t)> callback) {
    if (Manager::customDataCallbacks.count(key) == 0) {
        Manager::customDataCallbacks.insert({key, {}});
    }
    Manager::customDataCallbacks[key].push_back(callback);
}
