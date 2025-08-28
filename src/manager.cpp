#include "manager.hpp"

#include "CustomTypes/ReplayMenu.hpp"
#include "GlobalNamespace/BeatmapLevelsModel.hpp"
#include "GlobalNamespace/MenuLightsManager.hpp"
#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "camera.hpp"
#include "config.hpp"
#include "metacore/shared/events.hpp"
#include "metacore/shared/game.hpp"
#include "metacore/shared/input.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/songs.hpp"
#include "parsing.hpp"
#include "pause.hpp"
#include "playback.hpp"
#include "utils.hpp"

static bool replaying = false;
static bool rendering = false;
static bool started = false;
static bool paused = false;

static std::vector<std::pair<std::string, std::shared_ptr<Replay::Data>>> replays;
static std::map<std::string, std::shared_ptr<Replay::Data>> tempReplays;
static bool local = true;

static bool hasRotations = false;
static bool cancelPresentation = false;

std::map<std::string, std::vector<std::function<void(char const*, size_t)>>> Manager::customDataCallbacks;

static void SelectFromConfig(int index, bool render) {
    logger.debug("selecting level, render: {}, index: {}", render, index);

    auto queue = getConfig().RenderQueue.GetValue();
    if (index >= queue.size() || index < 0)
        return;
    auto level = queue[index];

    if (render) {
        queue.erase(queue.begin() + index);
        getConfig().RenderQueue.SetValue(queue);
    }

    auto main = MetaCore::Game::GetMainFlowCoordinator();
    auto map = main->_beatmapLevelsModel->GetBeatmapLevel(level.ID);
    auto pack = main->_beatmapLevelsModel->GetLevelPackForLevelId(level.ID);
    if (!map) {
        logger.debug("level id {} not found", level.ID);
        // if we're going through the render queue, go ahead and try to do the next one
        if (render)
            SelectFromConfig(index, render);
        return;
    }
    logger.debug("found level {} ({}) for id {}", fmt::ptr(map), map->songName, level.ID);

    MetaCore::Songs::SelectLevel({Utils::GetCharacteristic(level.Characteristic), level.Difficulty, level.ID}, pack);

    if (tempReplays.contains(level.ReplayHash))
        Manager::SetExternalReplay("", tempReplays[level.ReplayHash]);
    else
        getConfig().LastReplayHash.SetValue(level.ReplayHash);

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

void Manager::SaveCurrentLevelInConfig() {
    auto map = MetaCore::Songs::GetSelectedKey();
    if (!map.IsValid())
        return;
    auto const& info = GetCurrentInfo();

    auto levels = getConfig().RenderQueue.GetValue();
    auto& level = levels.emplace_back();

    level.ID = (std::string) map.levelId;
    level.Difficulty = (int) map.difficulty;
    level.Characteristic = (std::string) map.beatmapCharacteristic->serializedName;
    level.ReplayHash = info.hash;
    level.ReplayDesc = fmt::format("{} {}", info.source, Utils::GetStatusString(info));
    level.Temporary = !AreReplaysLocal();
    if (level.Temporary)
        tempReplays[level.ReplayHash] = replays[0].second;

    getConfig().RenderQueue.SetValue(levels);
}

static std::vector<LevelSelection>::iterator FindCurrentLevel(std::vector<LevelSelection>& levels) {
    auto current = MetaCore::Songs::GetSelectedKey();
    if (!current.IsValid())
        return levels.end();
    for (auto level = levels.begin(); level != levels.end(); level++) {
        if (level->ID == current.levelId && level->Characteristic == current.beatmapCharacteristic->_serializedName &&
            level->Difficulty == (int) current.difficulty && level->ReplayHash == Manager::GetCurrentInfo().hash)
            return level;
    }
    return levels.end();
}

void Manager::RemoveCurrentLevelFromConfig() {
    auto levels = getConfig().RenderQueue.GetValue();
    auto level = FindCurrentLevel(levels);
    if (level == levels.end())
        return;
    levels.erase(level);
    getConfig().RenderQueue.SetValue(levels);
}

bool Manager::IsCurrentLevelInConfig() {
    auto levels = getConfig().RenderQueue.GetValue();
    return FindCurrentLevel(levels) != levels.end();
}

void Manager::ClearLevelsFromConfig() {
    getConfig().RenderQueue.SetValue({});
    tempReplays.clear();
}

void Manager::SetExternalReplay(std::string path, std::shared_ptr<Replay::Data> replay) {
    replays = {{path, replay}};
    local = false;
    Replay::MenuView::GetInstance()->UpdateUI(false);
}

bool Manager::AreReplaysLocal() {
    return local;
}

int Manager::GetReplaysCount() {
    return replays.size();
}

void Manager::SelectReplay(int index) {
    getConfig().LastReplayHash.SetValue(replays[index].second->info.hash);
}

int Manager::GetSelectedIndex() {
    if (!AreReplaysLocal())
        return 0;
    // should never be called with empty replays vector
    std::string hash = getConfig().LastReplayHash.GetValue();
    for (int i = 0; i < replays.size(); i++) {
        if (replays[i].second->info.hash == hash)
            return i;
    }
    return 0;
}

void Manager::StartReplay(bool render) {
    if (replays.empty())
        return;
    logger.info("Starting replay, rendering: {}", render);

    auto& replay = GetCurrentReplay();

    replaying = true;
    rendering = render;
    started = false;
    paused = false;
    MetaCore::Game::SetScoreSubmission(MOD_ID, false);
    MetaCore::Input::SetHaptics(MOD_ID, false);

    auto copy = customDataCallbacks;
    for (auto const& pair : copy) {
        std::string const& key = pair.first;
        auto& callbacks = pair.second;

        auto found = replay.customData.find(key);
        logger.debug("calling {} callbacks for {}, with data: {}", callbacks.size(), key, found != replay.customData.end());

        char const* data = nullptr;
        int size = 0;
        if (found != replay.customData.end()) {
            data = found->second.data();
            size = found->second.size();
        }
        for (auto& callback : callbacks)
            callback(data, size);
    }
}

void Manager::CameraFinished() {
    if (!rendering)
        return;
    MetaCore::Game::SetCameraFadeOut(MOD_ID, false, 0.5);
    if (!getConfig().RenderQueue.GetValue().empty())
        SelectFromConfig(0, true);
    else if (getConfig().Ding.GetValue())
        Utils::PlayDing();
}

Replay::Data& Manager::GetCurrentReplay() {
    return *replays[GetSelectedIndex()].second;
}

void Manager::DeleteCurrentReplay() {
    auto replay = replays.begin() + GetSelectedIndex();
    try {
        std::filesystem::remove(replay->first);
    } catch (std::exception const& e) {
        logger.error("Failed to delete replay at {}: {}", replay->first, e.what());
        return;
    }
    replays.erase(replay);
    if (replays.empty()) {
        Replay::MenuView::Dismiss();
        Replay::MenuView::SetEnabled(false);
    } else
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

bool Manager::HasRotations() {
    return hasRotations;
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
    Camera::UpdateInputs();
    Pause::UpdateInputs();
}

ON_EVENT(MetaCore::Events::MapSelected) {
    if (Replay::MenuView::Presented())  // happens on level end
        return;
    auto map = MetaCore::Songs::GetSelectedKey();
    replays = Parsing::GetReplays(map);
    local = true;
    hasRotations = map.beatmapCharacteristic->_containsRotationEvents;
    Replay::MenuView::CreateShortcut();
    Replay::MenuView::SetEnabled(!replays.empty());
}

ON_EVENT(MetaCore::Events::MapStarted) {
    if (!replaying)
        return;
    logger.debug("replay started");
    Parsing::RecalculateNotes(Manager::GetCurrentReplay(), MetaCore::Internals::beatmapData->i___GlobalNamespace__IReadonlyBeatmapData());
    Camera::SetupCamera();
    Camera::CreateReplayText();
    if (paused) {
        Camera::OnPause();
        Pause::OnPause();
    }
    started = true;
}

ON_EVENT(MetaCore::Events::MapPaused) {
    if (!replaying)
        return;
    logger.debug("replay paused");
    paused = true;
    MetaCore::Input::SetHaptics(MOD_ID, true);
    if (!started)
        return;
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
    MetaCore::Input::SetHaptics(MOD_ID, false);
}

ON_EVENT(MetaCore::Events::MapRestarted) {
    if (!replaying)
        return;
    logger.debug("replay restarted");
    started = false;
    paused = false;
    Camera::OnRestart();
    Pause::OnUnpause();
    Playback::SeekTo(0);
    MetaCore::Input::SetHaptics(MOD_ID, false);
}

ON_EVENT(MetaCore::Events::MapEnded) {
    if (!replaying)
        return;
    logger.debug("replay ended");
    Camera::FinishReplay();
    started = false;
    paused = false;
    cancelPresentation = !MetaCore::Internals::mapWasQuit;
}

ON_EVENT(MetaCore::Events::GameplaySceneEnded) {
    if (MetaCore::Internals::mapWasRestarted)
        return;

    logger.debug("replay scene ended");
    auto main = MetaCore::Game::GetMainFlowCoordinator();
    if (replaying && !MetaCore::Internals::mapWasQuit)
        BSML::MainThreadScheduler::ScheduleNextFrame([main]() { main->_menuLightsManager->SetColorPreset(main->_defaultLightsPreset, false, 0); });

    if (!replaying && !MetaCore::Internals::mapWasQuit && !recorderInstalled)
        Replay::MenuView::DisableWithRecordingHint();

    MetaCore::Game::SetScoreSubmission(MOD_ID, true);
    MetaCore::Input::SetHaptics(MOD_ID, true);

    replaying = false;
}
