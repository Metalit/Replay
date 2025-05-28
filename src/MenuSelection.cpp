#include "MenuSelection.hpp"

#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"
#include "GlobalNamespace/BeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapLevelPack.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/LevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/MenuTransitionsHelper.hpp"
#include "GlobalNamespace/PlayerData.hpp"
#include "GlobalNamespace/PlayerDataModel.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "HMUI/ViewController.hpp"
#include "UnityEngine/Resources.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "bsml/shared/Helpers/getters.hpp"
#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/delegates.hpp"
#include "metacore/shared/game.hpp"
#include "metacore/shared/songs.hpp"
#include "utils.hpp"

using namespace GlobalNamespace;
using namespace HMUI;

SinglePlayerLevelSelectionFlowCoordinator* GetLevelSelectionFlowCoordinator() {
    auto flowCoordinator = (HMUI::FlowCoordinator*) BSML::Helpers::GetMainFlowCoordinator();
    std::optional<SinglePlayerLevelSelectionFlowCoordinator*> opt = std::nullopt;
    do {
        opt = il2cpp_utils::try_cast<SinglePlayerLevelSelectionFlowCoordinator>(flowCoordinator);
        if (opt)
            break;
    } while ((flowCoordinator = flowCoordinator->childFlowCoordinator));
    return opt ? *opt : nullptr;
}

void RenderAfterLoaded() {
    auto levelSelection = GetLevelSelectionFlowCoordinator();
    if (!levelSelection)
        return;
    BSML::MainThreadScheduler::ScheduleUntil(
        [levelSelection]() { return levelSelection->selectedBeatmapLevel && Manager::Camera::muxingFinished; }, []() { RenderCurrentLevel(); }
    );
}

void SelectRenderHelper(bool render, int idx, bool remove) {
    LOG_DEBUG("Selecting level, render: {}, idx: {}", render, idx);
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    if (levelsVec.size() <= idx || idx < 0)
        return;
    auto levelToSelect = levelsVec[idx];

    auto mainCoordinator = BSML::Helpers::GetMainFlowCoordinator();

    if (remove) {
        levelsVec.erase(levelsVec.begin() + idx);
        getConfig().LevelsToSelect.SetValue(levelsVec);
    }

    auto pack = mainCoordinator->_beatmapLevelsModel->GetLevelPack(levelToSelect.PackID);
    if (!pack)
        pack = mainCoordinator->_beatmapLevelsModel->GetLevelPackForLevelId(levelToSelect.ID);
    auto level = mainCoordinator->_beatmapLevelsModel->GetBeatmapLevel(levelToSelect.ID);

    LOG_DEBUG("level id {}, pack id {}", levelToSelect.ID, levelToSelect.PackID);
    LOG_DEBUG("found level {} ({}) in pack {} ({})", fmt::ptr(level), level ? level->songName : "", fmt::ptr(pack), pack ? pack->packName : "");

    if (!level) {
        // if we removed the level (at the moment means we're going through the queue), go ahead and try to do the next one
        if (remove)
            SelectRenderHelper(render, idx, remove);
        return;
    }

    MetaCore::Songs::SelectLevel(BeatmapKey(GetCharacteristic(levelToSelect.Characteristic), levelToSelect.Difficulty, levelToSelect.ID), pack);

    getConfig().LastReplayIdx.SetValue(levelToSelect.ReplayIndex);
    if (render)
        RenderAfterLoaded();
}

void AwaitMainFlowCoordinator(std::function<void()> afterPresent) {
    auto fc = BSML::Helpers::GetMainFlowCoordinator();
    if (fc && fc->_screenSystem)
        afterPresent();
    else {
        BSML::MainThreadScheduler::ScheduleUntil(
            []() {
                auto fc = BSML::Helpers::GetMainFlowCoordinator();
                return fc && fc->_screenSystem;
            },
            afterPresent
        );
    }
}

void SelectLevelInConfig(int idx, bool remove) {
    AwaitMainFlowCoordinator([idx, remove]() { SelectRenderHelper(false, idx, remove); });
}

void RenderLevelInConfig() {
    MetaCore::Game::SetCameraFadeOut(MOD_ID, true);
    AwaitMainFlowCoordinator([]() { SelectRenderHelper(true, 0, true); });
}

std::optional<LevelSelection> GetCurrentLevel() {
    std::optional<LevelSelection> ret = std::nullopt;

    auto levelSelection = GetLevelSelectionFlowCoordinator();
    if (!levelSelection || !levelSelection->selectedBeatmapLevel)
        return ret;
    auto map = levelSelection->selectedBeatmapKey;
    if (!map.IsValid())
        return ret;

    ret.emplace();
    ret->ID = (std::string) map.levelId;
    ret->Difficulty = (int) map.difficulty;
    ret->Characteristic = (std::string) map.beatmapCharacteristic->serializedName;

    auto pack = levelSelection->selectedBeatmapLevelPack;
    if (pack)
        ret->PackID = (std::string) pack->packID;
    ret->Category = (int) levelSelection->selectedLevelCategory;

    ret->ReplayIndex = getConfig().LastReplayIdx.GetValue();
    return ret;
}

void SaveCurrentLevelInConfig() {
    auto newLevel = GetCurrentLevel();
    if (!newLevel)
        return;
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    levelsVec.emplace_back(*newLevel);
    getConfig().LevelsToSelect.SetValue(levelsVec);
}

void RemoveCurrentLevelFromConfig() {
    auto currentLevel = GetCurrentLevel();
    if (!currentLevel)
        return;
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    for (auto iter = levelsVec.begin(); iter != levelsVec.end(); iter++) {
        if (iter->ID == currentLevel->ID && iter->Characteristic == currentLevel->Characteristic && iter->Difficulty == currentLevel->Difficulty &&
            iter->ReplayIndex == currentLevel->ReplayIndex) {
            levelsVec.erase(iter);
            break;
        }
    }
    getConfig().LevelsToSelect.SetValue(levelsVec);
}

bool IsCurrentLevelInConfig() {
    auto currentLevel = GetCurrentLevel();
    if (!currentLevel)
        return false;
    auto levelsVec = getConfig().LevelsToSelect.GetValue();
    for (auto level : levelsVec) {
        if (level.ID == currentLevel->ID && level.Characteristic == currentLevel->Characteristic && level.Difficulty == currentLevel->Difficulty &&
            level.ReplayIndex == currentLevel->ReplayIndex)
            return true;
    }
    return false;
}

void RenderCurrentLevel(bool currentReplay) {
    auto levelSelection = GetLevelSelectionFlowCoordinator();
    if (!levelSelection) {
        LOG_ERROR("Failed to get LevelSelectionFlowCoordinator, not rendering");
        return;
    }
    auto map = levelSelection->selectedBeatmapKey;
    if (!map.IsValid()) {
        LOG_ERROR("Failed to get selected beatmap, not rendering");
        return;
    }
    Manager::Camera::rendering = true;
    if (!currentReplay) {
        auto replays = GetReplays(map);
        if (replays.empty()) {
            LOG_ERROR("Failed to get beatmap replays, not rendering");
            return;
        }
        int idx = getConfig().LastReplayIdx.GetValue();
        if (idx >= replays.size())
            idx = replays.size() - 1;
        Manager::ReplayStarted(replays[idx].second);
    } else
        Manager::ReplayStarted(Manager::currentReplay);
    levelSelection->StartLevel(nullptr, false);
}

void RestartGame() {
    LOG_ERROR("Not implemented :(");
}
