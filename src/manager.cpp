#include "manager.hpp"

#include "CustomTypes/ReplayMenu.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/MenuLightsManager.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/game.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/songs.hpp"
#include "parsing.hpp"
#include "playback.hpp"
#include "utils.hpp"

static bool replaying = false;
static bool rendering = false;
static bool paused = false;

static std::vector<std::pair<std::string, Replay::Replay>> replays;
static bool local = true;

std::map<std::string, std::vector<std::function<void(char const*, size_t)>>> Manager::customDataCallbacks;

static void SelectFromConfig(int index, bool render) {
    logger.debug("Selecting level, render: {}, idx: {}", render, index);

    auto levels = getConfig().LevelsToSelect.GetValue();
    if (index >= levels.size() || index < 0)
        return;
    auto levelToSelect = levels[index];

    if (render) {
        levels.erase(levels.begin() + index);
        getConfig().LevelsToSelect.SetValue(levels);
    }

    auto main = MetaCore::Game::GetMainFlowCoordinator();
    auto pack = main->_beatmapLevelsModel->GetLevelPack(levelToSelect.PackID);
    if (!pack)
        pack = main->_beatmapLevelsModel->GetLevelPackForLevelId(levelToSelect.ID);
    auto level = main->_beatmapLevelsModel->GetBeatmapLevel(levelToSelect.ID);

    logger.debug("level id {}, pack id {}", levelToSelect.ID, levelToSelect.PackID);
    logger.debug("found level {} ({}) in pack {} ({})", fmt::ptr(level), level ? level->songName : "", fmt::ptr(pack), pack ? pack->packName : "");

    if (!level) {
        // if we're going through the render queue, go ahead and try to do the next one
        if (render)
            SelectFromConfig(index, render);
        return;
    }

    MetaCore::Songs::SelectLevel({Utils::GetCharacteristic(levelToSelect.Characteristic), levelToSelect.Difficulty, levelToSelect.ID}, pack);

    getConfig().LastReplayIdx.SetValue(levelToSelect.ReplayIndex);
    if (render) {
        Manager::StartReplay(true);
        main->_soloFreePlayFlowCoordinator->StartLevel(nullptr, false);
    } else
        Menu::ReplayViewController::Present();
}

void Manager::SelectLevelInConfig(int index) {
    SelectFromConfig(index, false);
}

void Manager::BeginQueue() {
    SelectFromConfig(0, true);
}

void Manager::SaveCurrentLevelInConfig() {}

void Manager::RemoveCurrentLevelFromConfig() {}

bool Manager::IsCurrentLevelInConfig() {
    auto selection = MetaCore::Songs::GetSelectedKey();
    if (!selection.IsValid())
        return false;
    for (auto level : getConfig().LevelsToSelect.GetValue()) {
        if (level.ID == selection.levelId && level.Characteristic == selection.beatmapCharacteristic->_serializedName &&
            level.Difficulty == (int) selection.difficulty && level.ReplayIndex == getConfig().LastReplayIdx.GetValue())
            return true;
    }
    return false;
}

void Manager::SetExternalReplay(std::string path, Replay::Replay replay) {
    replays = {{path, replay}};
    local = false;
    Menu::ReplayViewController::GetInstance()->UpdateUI(false);
}

bool Manager::AreReplaysLocal() {
    return local;
}

std::vector<std::pair<std::string, Replay::Replay>>& Manager::GetReplays() {
    return replays;
}

void Manager::StartReplay(bool render) {
    if (replays.empty())
        return;
    auto& replay = GetCurrentReplay();

    replaying = true;
    rendering = render;
    paused = false;
    MetaCore::Game::DisableScoreSubmissionOnce(MOD_ID);

    for (auto& pair : customDataCallbacks) {
        std::string const& key = pair.first;
        auto& callbacks = pair.second;

        auto found = replay.customData.find(key);
        if (found == replay.customData.end())
            continue;

        for (auto& callback : callbacks)
            callback(found->second.data(), found->second.size());
    }
}

void Manager::CameraFinished() {
    if (!rendering)
        return;
    if (getConfig().LevelsToSelect.GetValue().empty()) {
        MetaCore::Game::SetCameraFadeOut(MOD_ID, false);
        Utils::PlayDing();
    } else
        SelectFromConfig(0, true);
}

static int GetCurrentIndex() {
    // should never be called with empty replays vector
    int idx = getConfig().LastReplayIdx.GetValue();
    if (idx >= replays.size()) {
        idx = replays.size() - 1;
        if (local)
            getConfig().LastReplayIdx.SetValue(idx);
    }
    return idx;
}

Replay::Replay& Manager::GetCurrentReplay() {
    return replays[GetCurrentIndex()].second;
}

void Manager::DeleteCurrentReplay() {
    auto replay = replays.begin() + GetCurrentIndex();
    try {
        std::filesystem::remove(replay->first);
    } catch (std::exception const& e) {
        logger.error("Failed to delete replay: {}", e.what());
        return;
    }
    replays.erase(replays.begin());
    Menu::ReplayViewController::GetInstance()->UpdateUI(false);
}

Replay::Info& Manager::GetCurrentInfo() {
    return GetCurrentReplay().info;
}

bool Manager::Replaying() {
    return replaying;
}

bool Manager::Rendering() {
    return replaying && rendering;
}

bool Manager::Paused() {
    return paused;
}

ON_EVENT(MetaCore::Events::Update) {
    if (!replaying)
        return;
    Camera::SetMoving(Utils::IsButtonDown(getConfig().MoveButton.GetValue()));
    Camera::Travel(Utils::IsButtonDown(getConfig().TravelButton.GetValue()));
}

ON_EVENT(MetaCore::Events::MapSelected) {
    replays = Parsing::GetReplays(MetaCore::Songs::GetSelectedKey());
    local = true;
    Menu::ReplayViewController::SetEnabled(!replays.empty());
    if (!replays.empty())
        Menu::ReplayViewController::GetInstance()->UpdateUI(true);
}

ON_EVENT(MetaCore::Events::MapStarted) {
    if (replaying)
        Parsing::RecalculateNotes(Manager::GetCurrentReplay(), MetaCore::Internals::beatmapData()->i___GlobalNamespace__IReadonlyBeatmapData());
    Camera::SetupCamera();
}

ON_EVENT(MetaCore::Events::MapPaused) {
    paused = true;
    Camera::OnPause();
}

ON_EVENT(MetaCore::Events::MapUnpaused) {
    paused = false;
    Camera::OnUnpause();
}

ON_EVENT(MetaCore::Events::MapRestarted) {
    paused = false;
}

ON_EVENT(MetaCore::Events::MapEnded) {
    Camera::FinishReplay();
    replaying = false;
    paused = false;
}

ON_EVENT(MetaCore::Events::GameplaySceneEnded) {
    auto main = MetaCore::Game::GetMainFlowCoordinator();
    if (replaying && !MetaCore::Internals::mapWasQuit)
        main->_menuLightsManager->SetColorPreset(main->_defaultLightsPreset, false, 0);

    if (!replaying && !MetaCore::Internals::mapWasQuit && !recorderInstalled)
        Menu::ReplayViewController::DisableWithRecordingHint();
}
