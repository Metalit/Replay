#include "Main.hpp"
#include "Config.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "MenuSelection.hpp"
#include "JNIUtils.hpp"

#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "GlobalNamespace/SelectLevelDestination.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/IBeatmapLevelPackCollection.hpp"
#include "GlobalNamespace/IBeatmapLevelPack.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "HMUI/ViewController_AnimationDirection.hpp"
#include "UnityEngine/Resources.hpp"

#include "questui/shared/BeatSaberUI.hpp"

using namespace GlobalNamespace;
using namespace HMUI;

void SelectLevelInConfig() {
    auto levelToSelect = getConfig().LevelToSelect.GetValue();
    if(levelToSelect.ID == "")
        return;
    getConfig().LevelToSelect.SetValue({});
    auto mainCoordinator = QuestUI::BeatSaberUI::GetMainFlowCoordinator();
    auto flowCoordinator = mainCoordinator->YoungestChildFlowCoordinatorOrSelf();
    if(!flowCoordinator->isActivated)
        return;
    auto currentFlowCoordinator = flowCoordinator;
    while((flowCoordinator = flowCoordinator->parentFlowCoordinator)) {
        flowCoordinator->DismissFlowCoordinator(currentFlowCoordinator, ViewController::AnimationDirection::Horizontal, nullptr, true);
        currentFlowCoordinator = flowCoordinator;
    }
    auto pack = mainCoordinator->beatmapLevelsModel->GetLevelPack(levelToSelect.PackID);
    if(!pack)
        pack = mainCoordinator->beatmapLevelsModel->GetLevelPackForLevelId(levelToSelect.ID);
    auto level = mainCoordinator->beatmapLevelsModel->GetLevelPreviewForLevelId(levelToSelect.ID);
    if(!level)
        return;
    auto chars = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapCharacteristicCollectionSO*>().First();
    auto characteristic = chars->GetBeatmapCharacteristicBySerializedName(levelToSelect.Characteristic);
    mainCoordinator->ProcessMenuDestinationRequest(SelectLevelDestination::New_ctor(pack, level, levelToSelect.Difficulty, characteristic));
    // this method should only be ran after restarting after a render
    bool wasTempAudio = getConfig().NextIsAudio.GetValue();
    if(wasTempAudio)
        getConfig().NextIsAudio.SetValue(false);
    if(getConfig().AutoAudio.GetValue() && !wasTempAudio) {
        getConfig().NextIsAudio.SetValue(true);
        RenderCurrentLevel();
    } else if(getConfig().Ding.GetValue())
        PlayDing();
}

void SaveCurrentLevelInConfig() {
    LevelSelection ret;
    auto flowCoordinator = QuestUI::BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    auto opt = il2cpp_utils::try_cast<SinglePlayerLevelSelectionFlowCoordinator>(flowCoordinator);
    if(!opt)
        return;
    auto levelSelection = *opt;
    auto map = levelSelection->get_selectedDifficultyBeatmap();
    if(!map)
        return;
    ret.ID = (std::string) map->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    ret.Difficulty = map->get_difficulty();
    ret.Characteristic = (std::string) map->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    auto pack = levelSelection->get_selectedBeatmapLevelPack();
    if(pack)
        ret.PackID = (std::string) pack->get_packID();
    getConfig().LevelToSelect.SetValue(ret);
}

void RenderCurrentLevel() {
    auto flowCoordinator = QuestUI::BeatSaberUI::GetMainFlowCoordinator()->YoungestChildFlowCoordinatorOrSelf();
    auto opt = il2cpp_utils::try_cast<SinglePlayerLevelSelectionFlowCoordinator>(flowCoordinator);
    if(!opt)
        return;
    auto levelSelection = *opt;
    auto map = levelSelection->get_selectedDifficultyBeatmap();
    if(!map)
        return;
    auto replays = GetReplays(map);
    Manager::Camera::rendering = true;
    Manager::ReplayStarted(replays.begin()->second);
    levelSelection->StartLevel(nullptr, false);
}

void RestartGame() {
    JNIUtils::RestartApp();
}
