#include "CustomTypes/ReplayMenu.hpp"
#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "camera.hpp"
#include "conditional-dependencies/shared/main.hpp"
#include "manager.hpp"
#include "parsing.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

EXPOSE_API(PlayBSORFromFile, bool, std::string path) {
    try {
        auto replay = Parsing::ReadBSOR(path);
        Manager::SetExternalReplay(path, replay);

        Replay::MenuView::Present();
        return true;
    } catch (std::exception const& e) {
        logger.error("Error parsing BSOR from {}: {}", path, e.what());
        return false;
    }
}

EXPOSE_API(PlayBSORFromFileForced, bool, std::string path) {
    try {
        auto replay = Parsing::ReadBSOR(path);
        Manager::SetExternalReplay(path, replay);

        if (auto levelView = UnityEngine::Object::FindObjectOfType<GlobalNamespace::StandardLevelDetailView*>(true)) {
            Manager::StartReplay(false);
            levelView->actionButton->onClick->Invoke();
            return true;
        }

        logger.error("failed to find StandardLevelDetailView");
        return false;
    } catch (std::exception const& e) {
        logger.error("Error parsing BSOR from {}: {}", path, e.what());
        return false;
    }
}

EXPOSE_API(IsInReplay, bool) {
    return Manager::Replaying();
}

EXPOSE_API(IsInRender, bool) {
    return Manager::Rendering();
}

// every added callback will be called when replay start, if we don't have that custom data, the first parameter will be nullptr
EXPOSE_API(AddReplayCustomDataCallback, void, std::string key, std::function<void(char const*, size_t)> callback) {
    if (Manager::customDataCallbacks.count(key) == 0) {
        Manager::customDataCallbacks.insert({key, {}});
    }
    Manager::customDataCallbacks[key].push_back(callback);
}
