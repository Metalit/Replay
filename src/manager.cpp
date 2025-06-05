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
#include "pause.hpp"
#include "playback.hpp"
#include "utils.hpp"

static bool replaying = false;
static bool rendering = false;
static bool paused = false;

static std::vector<std::pair<std::string, Replay::Data>> replays;
static bool local = true;

static bool restarting = false;
static bool cancelPresentation = false;

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
        Replay::MenuView::Present();
}

void Manager::SelectLevelInConfig(int index) {
    SelectFromConfig(index, false);
}

void Manager::BeginQueue() {
    SelectFromConfig(0, true);
}

static std::optional<LevelSelection> GetCurrentLevel() {
    std::optional<LevelSelection> ret = std::nullopt;

    auto map = MetaCore::Songs::GetSelectedKey();
    if (!map.IsValid())
        return ret;

    ret.emplace();
    ret->ID = (std::string) map.levelId;
    ret->Difficulty = (int) map.difficulty;
    ret->Characteristic = (std::string) map.beatmapCharacteristic->serializedName;

    auto pack = MetaCore::Songs::GetSelectedPlaylist();
    if (pack)
        ret->PackID = (std::string) pack->packID;

    ret->ReplayIndex = getConfig().LastReplayIdx.GetValue();
    return ret;
}

void Manager::SaveCurrentLevelInConfig() {
    auto map = MetaCore::Songs::GetSelectedKey();
    if (!map.IsValid())
        return;

    auto levels = getConfig().LevelsToSelect.GetValue();
    auto added = levels.emplace_back();

    added.ID = (std::string) map.levelId;
    added.Difficulty = (int) map.difficulty;
    added.Characteristic = (std::string) map.beatmapCharacteristic->serializedName;

    auto pack = MetaCore::Songs::GetSelectedPlaylist();
    if (pack)
        added.PackID = (std::string) pack->packID;

    added.ReplayIndex = getConfig().LastReplayIdx.GetValue();

    getConfig().LevelsToSelect.SetValue(levels);
}

static std::vector<LevelSelection>::iterator FindCurrentLevel(std::vector<LevelSelection>& levels) {
    auto current = MetaCore::Songs::GetSelectedKey();
    if (!current.IsValid())
        return levels.end();
    for (auto level = levels.begin(); level != levels.end(); level++) {
        if (level->ID == current.levelId && level->Characteristic == current.beatmapCharacteristic->_serializedName &&
            level->Difficulty == (int) current.difficulty && level->ReplayIndex == getConfig().LastReplayIdx.GetValue())
            return level;
    }
    return levels.end();
}

void Manager::RemoveCurrentLevelFromConfig() {
    auto levels = getConfig().LevelsToSelect.GetValue();
    auto level = FindCurrentLevel(levels);
    if (level == levels.end())
        return;
    levels.erase(level);
    getConfig().LevelsToSelect.SetValue(levels);
}

bool Manager::IsCurrentLevelInConfig() {
    auto levels = getConfig().LevelsToSelect.GetValue();
    return FindCurrentLevel(levels) != levels.end();
}

void Manager::SetExternalReplay(std::string path, Replay::Data replay) {
    replays = {{path, replay}};
    local = false;
    Replay::MenuView::GetInstance()->UpdateUI(false);
}

bool Manager::AreReplaysLocal() {
    return local;
}

std::vector<std::pair<std::string, Replay::Data>>& Manager::GetReplays() {
    return replays;
}

void Manager::StartReplay(bool render) {
    if (replays.empty())
        return;
    logger.info("Starting replay, rendering: {}", render);

    auto& replay = GetCurrentReplay();

    replaying = true;
    rendering = render;
    paused = false;
    cancelPresentation = true;
    MetaCore::Game::DisableScoreSubmissionOnce(MOD_ID);

    for (auto& pair : customDataCallbacks) {
        std::string const& key = pair.first;
        auto& callbacks = pair.second;

        auto found = replay.customData.find(key);
        logger.debug("calling {} callbacks for {} {}", callbacks.size(), key, found != replay.customData.end());
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

Replay::Data& Manager::GetCurrentReplay() {
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
    Replay::MenuView::GetInstance()->UpdateUI(false);
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

bool Manager::CancelPresentation() {
    bool ret = cancelPresentation;
    cancelPresentation = false;
    return ret;
}

ON_EVENT(MetaCore::Events::Update) {
    if (!replaying || paused)
        return;
    Playback::UpdateTime();
    Camera::SetMoving(Utils::IsButtonDown(getConfig().MoveButton.GetValue()));
    Camera::Travel(Utils::IsButtonDown(getConfig().TravelButton.GetValue()));
}

ON_EVENT(MetaCore::Events::MapSelected) {
    replays = Parsing::GetReplays(MetaCore::Songs::GetSelectedKey());
    local = true;
    Replay::MenuView::CreateShortcut();
    Replay::MenuView::SetEnabled(!replays.empty());
}

ON_EVENT(MetaCore::Events::MapStarted) {
    if (!replaying)
        return;
    logger.debug("replay started");
    Parsing::RecalculateNotes(Manager::GetCurrentReplay(), MetaCore::Internals::beatmapData()->i___GlobalNamespace__IReadonlyBeatmapData());
    Camera::SetupCamera();
}

ON_EVENT(MetaCore::Events::MapPaused) {
    if (!replaying)
        return;
    logger.debug("replay paused");
    paused = true;
    Camera::OnPause();
    Pause::OnPause();
}

ON_EVENT(MetaCore::Events::MapUnpaused) {
    if (!replaying)
        return;
    logger.debug("replay unpaused");
    paused = false;
    Camera::OnUnpause();
    Pause::OnUnpause();
}

ON_EVENT(MetaCore::Events::MapRestarted) {
    if (!replaying)
        return;
    logger.debug("replay restarted");
    paused = false;
    restarting = true;
    Camera::OnRestart();
    Pause::OnUnpause();
    Playback::SeekTo(0);
}

ON_EVENT(MetaCore::Events::MapEnded) {
    if (!replaying)
        return;
    logger.debug("replay ended");
    Camera::FinishReplay();
    paused = false;
}

ON_EVENT(MetaCore::Events::GameplaySceneEnded) {
    if (restarting) {
        restarting = false;
        return;
    }

    logger.debug("replay scene ended");
    auto main = MetaCore::Game::GetMainFlowCoordinator();
    if (replaying && !MetaCore::Internals::mapWasQuit)
        main->_menuLightsManager->SetColorPreset(main->_defaultLightsPreset, false, 0);

    if (!replaying && !MetaCore::Internals::mapWasQuit && !recorderInstalled)
        Replay::MenuView::DisableWithRecordingHint();

    replaying = false;
}
