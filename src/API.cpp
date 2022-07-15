#include "GlobalNamespace/StandardLevelDetailView.hpp"

#include "UnityEngine/Resources.hpp"
#include "UnityEngine/UI/Button.hpp"
#include "UnityEngine/UI/Button_ButtonClickedEvent.hpp"

#define ID MOD_ID
#include "conditional-dependencies/shared/main.hpp"

#include "Formats/EventReplay.hpp"
#include "ReplayManager.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"

EXPOSE_API(PlayBSORFromFile, bool, std::string filePath) {
    auto replay = ReadBSOR(filePath);
    if (replay.replay) {
        auto levelView = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::StandardLevelDetailView*>().First();

        Manager::ReplayStarted(replay);
        levelView->actionButton->get_onClick()->Invoke();

        return true;
    }
    return false;
}

#pragma GCC diagnostic pop