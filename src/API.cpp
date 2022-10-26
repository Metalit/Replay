#include "GlobalNamespace/StandardLevelDetailView.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Button_ButtonClickedEvent.hpp"

#define ID MOD_ID
#include "conditional-dependencies/shared/main.hpp"

#include "Formats/EventReplay.hpp"
#include "ReplayManager.hpp"
#include "CustomTypes/ReplayMenu.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

EXPOSE_API(PlayBSORFromFile, bool, std::string filePath) {
    auto replay = ReadBSOR(filePath);
    if (replay.IsValid()) {
        auto levelView = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::StandardLevelDetailView*>().First();

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
        auto levelView = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::StandardLevelDetailView*>().First();

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

#pragma GCC diagnostic pop