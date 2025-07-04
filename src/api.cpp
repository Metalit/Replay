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

// callback may be given (nullptr, 0) if a replay is started without any custom data matching the key
EXPOSE_API(AddReplayCustomDataCallback, void, std::string key, std::function<void(char const*, size_t)> callback) {
    Manager::customDataCallbacks[key].emplace_back(std::move(callback));
}
