#include "camera.hpp"

#include "BeatSaber/Settings/QualitySettings.hpp"
#include "BeatSaber/Settings/Settings.hpp"
#include "CustomTypes/CameraRig.hpp"
#include "GlobalNamespace/IRenderingParamsApplicator.hpp"
#include "GlobalNamespace/MainCamera.hpp"
#include "GlobalNamespace/MainCameraCullingMask.hpp"
#include "GlobalNamespace/MainEffectController.hpp"
#include "GlobalNamespace/SceneType.hpp"
#include "GlobalNamespace/SettingsApplicatorSO.hpp"
#include "GlobalNamespace/VRRenderingParamsSetup.hpp"
#include "UnityEngine/Application.hpp"
#include "UnityEngine/AudioClip.hpp"
#include "UnityEngine/AudioListener.hpp"
#include "UnityEngine/AudioSettings.hpp"
#include "UnityEngine/AudioSource.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/StereoTargetEyeMask.hpp"
#include "UnityEngine/Time.hpp"
#include "bsml/shared/BSML/MainThreadScheduler.hpp"
#include "config.hpp"
#include "hollywood/shared/hollywood.hpp"
#include "manager.hpp"
#include "math.hpp"
#include "metacore/shared/game.hpp"
#include "metacore/shared/internals.hpp"
#include "metacore/shared/operators.hpp"
#include "metacore/shared/songs.hpp"
#include "metacore/shared/strings.hpp"
#include "playback.hpp"
#include "utils.hpp"

static Vector3 const BaseMovement = {0, 0, 1.5};

static auto const TmpVidPath = "/sdcard/replay-tmp.h264";
static auto const TmpAudPath = "/sdcard/replay-tmp.wav";
static auto const TmpOutPath = "/sdcard/replay-tmp-mux.mp4";

static std::string fileName = "";

static Vector3 baseCameraPosition;
static Quaternion baseCameraRotation;

int Camera::reinitDistortions = -1;

static Replay::CameraRig* cameraRig = nullptr;
UnityEngine::Camera* mainCamera = nullptr;
int oldCullingMask = 0;
int uiCullingMask = 1 << 5;

static bool moving = false;

Hollywood::AudioCapture* audioCapture = nullptr;
UnityEngine::Camera* customCamera = nullptr;
std::fstream videoOutput;

static void SetGraphicsSettings() {
    auto applicator = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::SettingsApplicatorSO*>()->First();
    auto settings = BeatSaber::Settings::Settings();
    settings.quality.vrResolutionScale = 0.8;

    // unchanged
    settings.quality.renderViewportScale = 1;
    settings.quality.maxQueuedFrames = -1;
    settings.quality.vSyncCount = 0;
    settings.quality.targetFramerate = UnityEngine::Application::get_targetFrameRate();

    // shockwaves and walls unused in ApplyPerformancePreset

    int antiAlias = 0;
    bool distortions = (getConfig().Walls.GetValue() == 2) || (getConfig().ShockwavesOn.GetValue() && getConfig().Shockwaves.GetValue() > 0);
    if (!distortions) {
        switch (getConfig().AntiAlias.GetValue()) {
            case 0:
                antiAlias = 0;
                break;
            case 1:
                antiAlias = 2;
                break;
            case 2:
                antiAlias = 4;
                break;
            case 3:
                antiAlias = 8;
                break;
        }
    }
    settings.quality.antiAliasingLevel = antiAlias;

    settings.quality.bloom = BeatSaber::Settings::QualitySettings::BloomQuality::Game;
    settings.quality.mainEffect = getConfig().Bloom.GetValue() ? BeatSaber::Settings::QualitySettings::MainEffectOption::Game
                                                               : BeatSaber::Settings::QualitySettings::MainEffectOption::Off;
    settings.quality.mirror = getConfig().Mirrors.GetValue();

    applicator->ApplyGraphicSettings(settings, GlobalNamespace::SceneType::Game);
    logger.info("applied custom graphics settings");
}

static void UnsetGraphicsSettings() {
    auto renderSetup = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::VRRenderingParamsSetup*>()->First();
    renderSetup->_applicator->Apply(GlobalNamespace::SceneType::Menu, "");
    logger.info("reset graphics settings");
}

static void SetupRecording() {
    SetGraphicsSettings();

    logger.info("Setting up recording");
    fileName = MetaCore::Strings::SanitizedPath(Utils::GetMapString());
    logger.info("Using filename: {}", fileName);

    logger.info("Keeping screen on");
    Hollywood::SetScreenOn(true);

    logger.info("Beginning video capture");
    customCamera = UnityEngine::Object::Instantiate(mainCamera);
    customCamera->tag = "Untagged";
    customCamera->gameObject->active = false;
    customCamera->enabled = true;

    int kept = 0;
    while (customCamera->transform->childCount > kept) {
        auto child = customCamera->transform->GetChild(kept)->gameObject;
        if (child->name == "AudioListener")
            kept++;
        else
            UnityEngine::Object::DestroyImmediate(child);
    }
    if (auto comp = customCamera->GetComponent<GlobalNamespace::MainCameraCullingMask*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent<GlobalNamespace::MainCamera*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent<UnityEngine::SpatialTracking::TrackedPoseDriver*>())
        UnityEngine::Object::DestroyImmediate(comp);
    if (auto comp = customCamera->GetComponent<Replay::CameraRig*>())
        UnityEngine::Object::DestroyImmediate(comp);

    customCamera->clearFlags = mainCamera->clearFlags;
    customCamera->nearClipPlane = mainCamera->nearClipPlane;
    customCamera->farClipPlane = mainCamera->farClipPlane;
    customCamera->backgroundColor = mainCamera->backgroundColor;
    customCamera->hideFlags = mainCamera->hideFlags;
    customCamera->depthTextureMode = mainCamera->depthTextureMode;
    customCamera->cullingMask = mainCamera->cullingMask;
    // Makes the camera render before the main
    customCamera->depth = mainCamera->depth - 1;

    customCamera->stereoTargetEye = UnityEngine::StereoTargetEyeMask::None;

    int width = getConfig().OverrideWidth.GetValue();
    int height = getConfig().OverrideHeight.GetValue();

    if (width <= 0)
        width = Resolutions[getConfig().Resolution.GetValue()].first;
    if (height <= 0)
        height = Resolutions[getConfig().Resolution.GetValue()].second;

    auto video = customCamera->gameObject->AddComponent<Hollywood::CameraCapture*>();
    videoOutput.open(TmpVidPath, std::ios::out | std::ios::binary);
    video->onOutputUnit = [](uint8_t* data, size_t len) {
        videoOutput.write((char*) data, len);
    };
    video->Init(width, height, getConfig().FPS.GetValue(), getConfig().Bitrate.GetValue() * 1000, getConfig().FOV.GetValue(), false);

    UnityEngine::Time::set_captureDeltaTime(1 / (float) getConfig().FPS.GetValue());

    Hollywood::SetSyncTimes(true);

    logger.info("Beginning audio capture");

    auto audioListener = customCamera->GetComponentInChildren<UnityEngine::AudioListener*>();
    audioCapture = audioListener->gameObject->AddComponent<Hollywood::AudioCapture*>();
    audioCapture->OpenFile(TmpAudPath);

    // make sure other audio listeners are disabled
    for (auto listener : UnityEngine::Object::FindObjectsOfType<UnityEngine::AudioListener*>())
        listener->enabled = listener == audioListener;

    customCamera->gameObject->active = true;

    // prevent bloom from applying to the main camera
    if (auto comp = mainCamera->GetComponent<GlobalNamespace::MainEffectController*>())
        UnityEngine::Object::DestroyImmediate(comp);

    // mask everything but ui
    oldCullingMask = mainCamera->cullingMask;
    mainCamera->cullingMask = uiCullingMask;

    Camera::reinitDistortions = 0;
}

static Replay::Transform const& GetHead() {
    return Playback::GetPose().head;
}

static void ResetBasePosition() {
    if (getConfig().CamMode.GetValue() == (int) CameraMode::ThirdPerson) {
        baseCameraPosition = getConfig().ThirdPerPos.GetValue();
        baseCameraRotation = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
    } else {
        baseCameraPosition = GetHead().position;
        baseCameraRotation = GetHead().rotation;
    }
}

static int GetMode() {
    CameraMode mode = (CameraMode) getConfig().CamMode.GetValue();
    if (mode == CameraMode::Headset && Manager::Rendering())
        return (int) CameraMode::Smooth;
    return (int) mode;
}

static void SetMoving(bool value) {
    if (GetMode() != (int) CameraMode::ThirdPerson || Manager::Rendering())
        return;
    if (moving && !value) {
        getConfig().ThirdPerPos.SetValue(cameraRig->GetCameraPosition());
        getConfig().ThirdPerRot.SetValue(cameraRig->GetCameraRotation().eulerAngles);
        cameraRig->SetTrackingEnabled(false);
    }
    moving = value;
}

static void Travel(int direction) {
    if (direction == 0 || GetMode() != (int) CameraMode::ThirdPerson || Manager::Rendering())
        return;
    float delta = UnityEngine::Time::get_deltaTime() * getConfig().TravelSpeed.GetValue();
    baseCameraPosition += Sombrero::QuaternionMultiply(cameraRig->GetCameraRotation(), BaseMovement * delta * direction);
    getConfig().ThirdPerPos.SetValue(baseCameraPosition);
}

void Camera::SetMode(int value) {
    CameraMode oldMode = (CameraMode) getConfig().CamMode.GetValue();
    CameraMode mode = (CameraMode) value;
    if (mode == oldMode)
        return;
    SetMoving(false);
    getConfig().CamMode.SetValue(value);
    ResetBasePosition();
}

static Quaternion ApplyTilt(Quaternion const& rotation, float tilt) {
    if (tilt == 0)
        return rotation;
    // rotate on local x axis
    return Sombrero::QuaternionMultiply(rotation, Quaternion::Euler({tilt, 0, 0}));
}

static constexpr inline UnityEngine::Matrix4x4 MatrixTranslate(UnityEngine::Vector3 const& vector) {
    UnityEngine::Matrix4x4 result;
    result.m00 = 1;
    result.m01 = 0;
    result.m02 = 0;
    result.m03 = vector.x;
    result.m10 = 0;
    result.m11 = 1;
    result.m12 = 0;
    result.m13 = vector.y;
    result.m20 = 0;
    result.m21 = 0;
    result.m22 = 1;
    result.m23 = vector.z;
    result.m30 = 0;
    result.m31 = 0;
    result.m32 = 0;
    result.m33 = 1;
    return result;
}

void Camera::SetupCamera() {
    logger.debug("setting up camera");
    // set culling matrix for moved camera modes and for rendering
    mainCamera = UnityEngine::Camera::get_main();

    auto mat = UnityEngine::Matrix4x4::Ortho(-99999, 99999, -99999, 99999, 0.001, 99999);
    mat = (mat * MatrixTranslate(Vector3::forward() * -49999.5)) * mainCamera->worldToCameraMatrix;
    mainCamera->cullingMatrix = mat;

    int mask = 2147483647;
    // Everything except FirstPerson (6) or ThirdPerson (3)
    if (GetMode() == (int) CameraMode::ThirdPerson)
        mask &= ~(1 << 6);
    else
        mask &= ~(1 << 3);
    if (!mainCamera->GetComponent<GlobalNamespace::MainCameraCullingMask*>()->_initData->showDebris)
        mask &= ~(1 << 9);
    mainCamera->cullingMask = mask;

    if (Manager::Rendering())
        SetupRecording();

    cameraRig = Replay::CameraRig::Create(mainCamera->transform);
    ResetBasePosition();
}

static void FinishMux() {
    if (getConfig().CleanFiles.GetValue()) {
        if (fileexists(TmpVidPath))
            std::filesystem::remove(TmpVidPath);
        if (fileexists(TmpAudPath))
            std::filesystem::remove(TmpAudPath);
    }

    logger.info("Removing screen on");
    if (MetaCore::Internals::mapWasQuit || getConfig().LevelsToSelect.GetValue().empty())
        Hollywood::SetScreenOn(false);

    Manager::CameraFinished();
}

static void DoMux() {
    std::string outputFile = fmt::format("{}/{}.mp4", RendersFolder, fileName);
    int num = 1;
    while (fileexists(outputFile))
        outputFile = fmt::format("{}/{}_{}.mp4", RendersFolder, fileName, num++);

    Hollywood::MuxFilesSync(TmpVidPath, TmpAudPath, TmpOutPath, getConfig().FPS.GetValue());

    if (fileexists(TmpOutPath)) {
        // idk how but I got a "no such file or directory" error here once
        int tries = 2;
        while (tries-- > 0) {
            try {
                std::filesystem::rename(TmpOutPath, outputFile);
                break;
            } catch (std::exception const& e) {
                logger.error("filesystem error renaming file {} -> {}: {}", TmpOutPath, outputFile, e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
    }
    FinishMux();
}

static void WaitThenMux() {
    bool vidExists = fileexists(TmpVidPath);
    bool audExists = fileexists(TmpAudPath);

    if (vidExists && audExists)
        BSML::MainThreadScheduler::ScheduleAfterTime(5, DoMux);
    else {
        logger.error("Cannot mux. Video exists: {} Audio exists: {}", vidExists, audExists);
        FinishMux();
    }
}

void Camera::FinishReplay() {
    mainCamera = nullptr;
    oldCullingMask = 0;

    if (audioCapture)
        UnityEngine::Object::DestroyImmediate(audioCapture);
    audioCapture = nullptr;
    if (customCamera)
        UnityEngine::Object::DestroyImmediate(customCamera->gameObject);
    customCamera = nullptr;
    if (cameraRig)
        UnityEngine::Object::DestroyImmediate(cameraRig);
    cameraRig = nullptr;

    if (Manager::Rendering()) {
        Hollywood::SetSyncTimes(false);
        UnityEngine::Time::set_captureDeltaTime(0);
        videoOutput.close();
        MetaCore::Game::SetCameraFadeOut(MOD_ID, true, 0);
        WaitThenMux();
        UnsetGraphicsSettings();
        // make distortions work for the headset camera again
        Camera::reinitDistortions = 3;
    } else
        Manager::CameraFinished();
}

void Camera::OnPause() {
    if (mainCamera && Manager::Rendering())
        mainCamera->cullingMask = oldCullingMask;
    SetMoving(false);
    cameraRig->SetTrackingEnabled(true);
    cameraRig->progress->active = false;
}

void Camera::OnUnpause() {
    if (mainCamera && Manager::Rendering())
        mainCamera->cullingMask = uiCullingMask;
    cameraRig->progress->active = Manager::Rendering();
}

void Camera::OnRestart() {
    OnPause();

    mainCamera = nullptr;
    oldCullingMask = 0;

    if (cameraRig)
        UnityEngine::Object::DestroyImmediate(cameraRig);
    cameraRig = nullptr;
}

static Quaternion GetCameraRotation() {
    auto ret = baseCameraRotation;
    if (GetMode() == (int) CameraMode::Smooth && getConfig().Correction.GetValue())
        ret = Sombrero::QuaternionMultiply(baseCameraRotation, Manager::GetCurrentInfo().averageOffset);
    return ApplyTilt(ret, getConfig().TargetTilt.GetValue());
}

static Vector3 GetCameraPosition() {
    if (GetMode() != (int) CameraMode::Smooth)
        return baseCameraPosition;
    auto offset = getConfig().Offset.GetValue();
    if (getConfig().Relative.GetValue())
        offset = Sombrero::QuaternionMultiply(GetCameraRotation(), offset);
    return baseCameraPosition + offset;
}

static void RunDefaultTimeSync(GlobalNamespace::AudioTimeSyncController* self) {
    float estimatedTimeIncrease = UnityEngine::Time::get_deltaTime() * self->_timeScale;

    int timeSamples = self->_audioSource->timeSamples;
    logger.debug("time samples is {} vs {}", timeSamples, self->_prevAudioSamplePos);
    if (self->_prevAudioSamplePos > timeSamples)
        self->_playbackLoopIndex++;
    if (self->_prevAudioSamplePos == timeSamples)
        self->_inBetweenDSPBufferingTimeEstimate += estimatedTimeIncrease;
    else
        self->_inBetweenDSPBufferingTimeEstimate = 0;
    self->_prevAudioSamplePos = timeSamples;

    // add regular delta time after audio ends
    float audioSourceTime = self->_songTime + estimatedTimeIncrease;
    if (self->_audioSource->isPlaying)
        audioSourceTime = self->_audioSource->time + self->_playbackLoopIndex * self->_audioSource->clip->length / self->_timeScale +
                          self->_inBetweenDSPBufferingTimeEstimate;
    float unityClockTime = self->timeSinceStart - self->_audioStartTimeOffsetSinceStart;

    self->_dspTimeOffset = UnityEngine::AudioSettings::get_dspTime() - audioSourceTime / self->_timeScale;
    float timeDifference = std::abs(unityClockTime - audioSourceTime);
    if (timeDifference > self->_forcedSyncDeltaTime) {
        self->_audioStartTimeOffsetSinceStart = self->timeSinceStart - audioSourceTime;
        unityClockTime = audioSourceTime;
    } else {
        if (self->_fixingAudioSyncError) {
            if (timeDifference < self->_stopSyncDeltaTime)
                self->_fixingAudioSyncError = false;
        } else if (timeDifference > self->_startSyncDeltaTime)
            self->_fixingAudioSyncError = true;

        if (self->_fixingAudioSyncError)
            self->_audioStartTimeOffsetSinceStart = std::lerp(
                self->_audioStartTimeOffsetSinceStart, self->timeSinceStart - audioSourceTime, estimatedTimeIncrease * self->_audioSyncLerpSpeed
            );
    }

    // ignore songTimeOffset and audioLatency
    float time = std::max(self->_songTime, unityClockTime);
    self->_lastFrameDeltaSongTime = time - self->_songTime;
    self->_songTime = time;
    self->_isReady = true;
}

bool Camera::UpdateTime(GlobalNamespace::AudioTimeSyncController* controller) {
    if (!Manager::Rendering() || Manager::Paused())
        return true;
    RunDefaultTimeSync(controller);
    return false;
}

static void UpdateCameraTransform(GlobalNamespace::PlayerTransforms* player) {
    if (GetMode() == (int) CameraMode::Smooth) {
        float deltaTime = UnityEngine::Time::get_deltaTime();
        baseCameraPosition =
            EaseLerp(baseCameraPosition, GetHead().position, UnityEngine::Time::get_time(), deltaTime * 2 / getConfig().Smoothing.GetValue());
        baseCameraRotation = Slerp(baseCameraRotation, GetHead().rotation, deltaTime * 2 / getConfig().Smoothing.GetValue());
    } else if (GetMode() == (int) CameraMode::ThirdPerson)
        ResetBasePosition();

    Vector3 cameraPosition = GetCameraPosition();
    Quaternion cameraRotation = GetCameraRotation();
    if (Manager::GetCurrentInfo().positionsAreLocal) {
        auto parent = player->_originParentTransform ? player->_originParentTransform : cameraRig->fakeHead->parent;
        cameraPosition = Sombrero::QuaternionMultiply(parent->rotation, cameraPosition) + parent->position;
        cameraRotation = Sombrero::QuaternionMultiply(parent->rotation, cameraRotation);
    }
    if (cameraRig)
        cameraRig->SetPositionAndRotation(cameraPosition, cameraRotation);
    if (customCamera)
        customCamera->transform->SetPositionAndRotation(cameraPosition, cameraRotation);
}

void Camera::UpdateCamera(GlobalNamespace::PlayerTransforms* player) {
    if (!cameraRig)
        return;

    CameraMode mode = (CameraMode) GetMode();
    bool overridePosition = !Manager::Paused() && mode != CameraMode::Headset;
    auto& pose = Playback::GetPose();

    // set fake head position so playertransforms is the same as during gameplay
    player->_headTransform = cameraRig->fakeHead;
    if (Manager::GetCurrentInfo().positionsAreLocal) {
        cameraRig->fakeHead->localPosition = pose.head.position;
        cameraRig->fakeHead->localRotation = pose.head.rotation;
    } else
        cameraRig->fakeHead->SetPositionAndRotation(pose.head.position, pose.head.rotation);

    if (overridePosition)
        UpdateCameraTransform(player);

    cameraRig->SetTrackingEnabled(!overridePosition || moving);

    bool enabled = mode == CameraMode::ThirdPerson && !Manager::Paused() && getConfig().Avatar.GetValue();
    cameraRig->avatar->gameObject->active = enabled;
    if (enabled)
        cameraRig->avatar->UpdateTransforms(
            pose.head.position, pose.leftHand.position, pose.rightHand.position, pose.head.rotation, pose.leftHand.rotation, pose.rightHand.rotation
        );
}

void Camera::UpdateInputs() {
    SetMoving(Utils::IsButtonDown(getConfig().MoveButton.GetValue()));
    Travel(Utils::IsButtonDown(getConfig().TravelButton.GetValue()));
}

void Camera::CreateReplayText() {
    auto levelData = MetaCore::Songs::GetSelectedLevel();
    std::string songName = levelData->songName;
    std::string songAuthor = levelData->songAuthorName;

    auto player = Manager::GetCurrentInfo().playerName;
    if (player.has_value() && (!Manager::GetCurrentInfo().playerOk || !getConfig().HideText.GetValue())) {
        std::string text = fmt::format("<color=red>REPLAY</color>    {} - {}    Player: {}", songName, songAuthor, player.value());

        auto canvas = BSML::Lite::CreateCanvas();
        canvas->transform->position = {0, getConfig().TextHeight.GetValue(), 7};

        BSML::Lite::CreateText(canvas, text, TMPro::FontStyles::Italic, 7, {}, {200, 10})->alignment = TMPro::TextAlignmentOptions::Center;
    }
}
