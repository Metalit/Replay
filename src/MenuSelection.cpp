#include "Main.hpp"
#include "Config.hpp"
#include "ReplayManager.hpp"
#include "Utils.hpp"
#include "MenuSelection.hpp"
#include "JNIUtils.hpp"

#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "UnityEngine/WaitForSeconds.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator_State.hpp"
#include "GlobalNamespace/SharedCoroutineStarter.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/PlayerData.hpp"
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

#include "custom-types/shared/coroutine.hpp"

using namespace GlobalNamespace;
using namespace HMUI;

SinglePlayerLevelSelectionFlowCoordinator* GetLevelSelectionFlowCoordinator() {
    auto flowCoordinator = (HMUI::FlowCoordinator*) QuestUI::BeatSaberUI::GetMainFlowCoordinator();
    std::optional<SinglePlayerLevelSelectionFlowCoordinator*> opt = std::nullopt;
    do {
        opt = il2cpp_utils::try_cast<SinglePlayerLevelSelectionFlowCoordinator>(flowCoordinator);
        if(opt)
            break;
    } while((flowCoordinator = flowCoordinator->get_childFlowCoordinator()));
    return opt ? *opt : nullptr;
}

custom_types::Helpers::Coroutine RenderAfterLoaded() {
    auto levelSelection = GetLevelSelectionFlowCoordinator();
    if(!levelSelection)
        co_return;
    while(!levelSelection->get_selectedBeatmapLevel() || !Manager::Camera::muxingFinished)
        co_yield (System::Collections::IEnumerator*) UnityEngine::WaitForSeconds::New_ctor(0.2);
    co_yield nullptr;
    RenderCurrentLevel();
    co_return;
}

void RenderLevelInConfig() {
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    if(levelsVec.empty())
        return;
    LOG_INFO("Selecting level from config");
    auto levelToSelect = levelsVec[0];
    auto mainCoordinator = QuestUI::BeatSaberUI::GetMainFlowCoordinator();
    auto flowCoordinator = mainCoordinator->YoungestChildFlowCoordinatorOrSelf();
    if(flowCoordinator != (HMUI::FlowCoordinator*) mainCoordinator) {
        if(!il2cpp_utils::try_cast<SinglePlayerLevelSelectionFlowCoordinator>(flowCoordinator)) {
            RenderLevelOnNextSongRefresh();
            UnityEngine::Resources::FindObjectsOfTypeAll<MenuTransitionsHelper*>().First()->RestartGame(nullptr);
            return;
        } else {
            auto views = flowCoordinator->mainScreenViewControllers;
            while(views->get_Count() > 1)
                flowCoordinator->DismissViewController(views->get_Item(views->get_Count() - 1), HMUI::ViewController::AnimationDirection::Horizontal, nullptr, true);
            mainCoordinator->DismissFlowCoordinator(flowCoordinator, HMUI::ViewController::AnimationDirection::Horizontal, nullptr, true);
        }
    }
    levelsVec.erase(levelsVec.begin());
    getConfig().LevelsToSelect.SetValue(levelsVec);
    auto pack = mainCoordinator->beatmapLevelsModel->GetLevelPack(levelToSelect.PackID);
    if(!pack)
        pack = mainCoordinator->beatmapLevelsModel->GetLevelPackForLevelId(levelToSelect.ID);
    auto level = mainCoordinator->beatmapLevelsModel->GetLevelPreviewForLevelId(levelToSelect.ID);
    if(!level)
        return;
    auto chars = UnityEngine::Resources::FindObjectsOfTypeAll<BeatmapCharacteristicCollectionSO*>().First();
    auto characteristic = chars->GetBeatmapCharacteristicBySerializedName(levelToSelect.Characteristic);
    mainCoordinator->playerDataModel->playerData->SetLastSelectedBeatmapCharacteristic(characteristic);
    mainCoordinator->playerDataModel->playerData->SetLastSelectedBeatmapDifficulty(levelToSelect.Difficulty);
    auto state = LevelSelectionFlowCoordinator::State::New_ctor(pack, level);
    state->levelCategory.value = levelToSelect.Category;
    state->levelCategory.has_value = true;
    mainCoordinator->soloFreePlayFlowCoordinator->Setup(state);
    mainCoordinator->PresentFlowCoordinator(mainCoordinator->soloFreePlayFlowCoordinator, nullptr, ViewController::AnimationDirection::Horizontal, true, false);
    getConfig().LastReplayIdx.SetValue(levelToSelect.ReplayIndex);
    SharedCoroutineStarter::get_instance()->StartCoroutine(custom_types::Helpers::CoroutineHelper::New(RenderAfterLoaded()));
}

std::optional<LevelSelection> GetCurrentLevel() {
    std::optional<LevelSelection> ret = std::nullopt;
    auto levelSelection = GetLevelSelectionFlowCoordinator();
    if(!levelSelection)
        return ret;
    auto map = levelSelection->get_selectedDifficultyBeatmap();
    if(!map)
        return ret;
    ret.emplace();
    ret->ID = (std::string) map->get_level()->i_IPreviewBeatmapLevel()->get_levelID();
    ret->Difficulty = map->get_difficulty();
    ret->Characteristic = (std::string) map->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    auto pack = levelSelection->get_selectedBeatmapLevelPack();
    if(pack)
        ret->PackID = (std::string) pack->get_packID();
    ret->Category = levelSelection->get_selectedLevelCategory();
    ret->ReplayIndex = getConfig().LastReplayIdx.GetValue();
    return ret;
}

void SaveCurrentLevelInConfig() {
    auto newLevel = GetCurrentLevel();
    if(!newLevel)
        return;
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    levelsVec.emplace_back(*newLevel);
    getConfig().LevelsToSelect.SetValue(levelsVec);
}

bool IsCurrentLevelInConfig() {
    auto currentLevel = GetCurrentLevel();
    if(!currentLevel)
        return false;
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    for(auto level : levelsVec) {
        if(level.ID == currentLevel->ID
            && level.Characteristic == currentLevel->Characteristic
            && level.Difficulty == currentLevel->Difficulty
            && level.ReplayIndex == currentLevel->ReplayIndex
        )
            return true;
    }
    return false;
}

void RenderCurrentLevel(bool currentReplay) {
    auto levelSelection = GetLevelSelectionFlowCoordinator();
    if(!levelSelection) {
        LOG_ERROR("Failed to get LevelSelectionFlowCoordinator, not rendering");
        return;
    }
    auto map = levelSelection->get_selectedDifficultyBeatmap();
    if(!map) {
        LOG_ERROR("Failed to get selected beatmap, not rendering");
        return;
    }
    Manager::Camera::rendering = true;
    if(!currentReplay) {
        auto replays = GetReplays(map);
        if(replays.empty()) {
            LOG_ERROR("Failed to get beatmap replays, not rendering");
            return;
        }
        int idx = getConfig().LastReplayIdx.GetValue();
        if(idx >= replays.size())
            idx = replays.size() - 1;
        Manager::ReplayStarted(replays[idx].second);
    } else
        Manager::ReplayStarted(Manager::currentReplay);
    levelSelection->StartLevel(nullptr, false);
}

void RestartGame() {
    auto env = JNIUtils::GetJNIEnv();
    JNIUtils::LaunchApp(env, "com.example.norestore");
    JNIUtils::KillApp(env);
}
