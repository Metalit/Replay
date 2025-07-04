#include "CustomTypes/CameraRig.hpp"

#include "BeatSaber/BeatAvatarSDK/AvatarData.hpp"
#include "BeatSaber/BeatAvatarSDK/AvatarDataModel.hpp"
#include "BeatSaber/BeatAvatarSDK/BeatAvatarVisualController.hpp"
#include "GlobalNamespace/BeatAvatarEditorFlowCoordinator.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/SpatialTracking/PoseDataFlags.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRNode.hpp"
#include "bsml/shared/Helpers/utilities.hpp"
#include "config.hpp"
#include "main.hpp"
#include "manager.hpp"
#include "metacore/shared/stats.hpp"
#include "metacore/shared/strings.hpp"
#include "playback.hpp"
#include "replay.hpp"
#include "utils.hpp"

DEFINE_TYPE(Replay, CameraRig);

using namespace BeatSaber::BeatAvatarSDK;
using namespace GlobalNamespace;
using namespace Replay;

void CameraRig::SetPositionAndRotation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot) {
    // when rendering, show a progress screen in the middle of nowhere instead of the gameplay
    if (Manager::Rendering() && !Manager::Paused()) {
        transform->position = {0, -1000, -1000};
        transform->rotation = Quaternion::Euler({0, 0, -1});
        UpdateProgress();
    } else {
        transform->rotation = rot;
        transform->position = pos;
    }
}

void CameraRig::UpdateProgress() {
    std::string time = MetaCore::Strings::SecondsToString(MetaCore::Stats::GetSongTime());
    std::string total = MetaCore::Strings::SecondsToString(MetaCore::Stats::GetSongLength());
    std::string queue = "";
    int len = getConfig().LevelsToSelect.GetValue().size();
    if (len > 0)
        queue = fmt::format("\n{} in queue", len);
    std::string label = fmt::format("{}\nRendering...\nSong Time: {} / {}{}", mapString, time, total, queue);
    progressText->text = label;
}

static void SetOriginPoseToInverseTracking(UnityEngine::SpatialTracking::TrackedPoseDriver* tracker) {
    UnityEngine::Pose trackedPose;
    if (tracker->GetPoseData(tracker->m_Device, tracker->m_PoseSource, byref(trackedPose)) == UnityEngine::SpatialTracking::PoseDataFlags::NoData)
        logger.warn("failed to get pose data for relative tracking calculation");

    Quaternion originRotation = Quaternion::Inverse(trackedPose.rotation);
    tracker->m_OriginPose.position = -(originRotation * trackedPose.position);
    tracker->m_OriginPose.rotation = originRotation;
}

void CameraRig::SetTrackingEnabled(bool value, bool preservePosition) {
    if (tracking == value)
        return;
    tracking = value;

    if (!preservePosition)
        cameraTransform->SetLocalPositionAndRotation(Vector3::zero(), Quaternion::identity());
    cameraTracker->CacheLocalPosition();
    cameraTracker->enabled = tracking;

    if (tracking) {
        if (!preservePosition)
            transform->SetPositionAndRotation(Vector3::zero(), Quaternion::identity());
        else
            SetOriginPoseToInverseTracking(cameraTracker);
        cameraTracker->m_UseRelativeTransform = preservePosition;
    }
}

UnityEngine::Vector3 CameraRig::GetPosition() {
    return cameraTransform->position;
}

UnityEngine::Quaternion CameraRig::GetRotation() {
    return cameraTransform->rotation;
}

CameraRig* CameraRig::Create(UnityEngine::Transform* cameraTransform) {
    // original hierarchy:
    // parent
    //  - cameraTransform (playerTransforms->headTransform)

    // new hierarchy:
    // parent
    //  - headReplacement (playerTransforms->headTransform)
    //  - customAvatar
    // cameraRig (root object)
    //  - cameraTransform (main camera, TrackedPoseDriver)
    //     - progress

    // have the camera rig itself not be parented, since we calculate the local position offset ourselves
    auto cameraRig = UnityEngine::GameObject::New_ctor("ReplayCameraRig")->AddComponent<Replay::CameraRig*>();
    cameraRig->cameraTransform = cameraTransform;
    cameraRig->cameraTracker = cameraTransform->GetComponent<UnityEngine::SpatialTracking::TrackedPoseDriver*>();
    auto parent = cameraTransform->parent;
    cameraTransform->SetParent(cameraRig->transform, false);

    cameraRig->tracking = true;
    cameraRig->SetTrackingEnabled(false, false);

    auto headReplacement = UnityEngine::GameObject::New_ctor("PlayerTransformsHeadReplacement")->transform;
    headReplacement->SetParent(parent, false);
    cameraRig->fakeHead = headReplacement;

    auto avatarCoordinator = UnityEngine::Resources::FindObjectsOfTypeAll<GlobalNamespace::BeatAvatarEditorFlowCoordinator*>()->First([](auto x) {
        return x->_avatarDataModel != nullptr;
    });
    auto playerAvatar = avatarCoordinator->_avatarContainerGameObject->transform->GetChild(0)->GetComponent<BeatAvatarPoseController*>();
    auto customAvatar = UnityEngine::Object::Instantiate(playerAvatar);
    UnityEngine::GameObject::SetName(customAvatar, "ReplayCustomAvatar");
    auto visualController = customAvatar->GetComponent<BeatAvatarVisualController*>();

    auto dataModel = avatarCoordinator->_avatarDataModel;
    visualController->_avatarPartsModel = dataModel->_avatarPartsModel;
    visualController->UpdateAvatarVisual(dataModel->avatarData);

    auto transform = customAvatar->transform;
    // avatar uses the frame data, not the calculations in PlayerTransforms_Update
    if (Manager::GetCurrentInfo().positionsAreLocal)
        transform->SetParent(parent);
    transform->position = {0, 0, 0};
    transform->localScale = {1, 1, 1};

    cameraRig->avatar = customAvatar;

    auto progress = BSML::Lite::CreateCanvas();
    progress->name = "RenderProgressScreen";
    transform = progress->transform;
    transform->SetParent(cameraTransform, false);
    transform->localPosition = {0, 0, 2};
    transform->localScale = {0.05, 0.05, 0.05};

    auto image = BSML::Lite::CreateImage(progress, BSML::Utilities::ImageResources::GetWhitePixel());
    image->color = UnityEngine::Color::get_black();
    image->rectTransform->anchorMin = {0, 0};
    image->rectTransform->anchorMax = {1, 1};
    auto text = BSML::Lite::CreateText(progress, "Rendering...");
    text->alignment = TMPro::TextAlignmentOptions::Center;
    text->rectTransform->anchorMin = {0.5, 0.5};
    text->rectTransform->anchorMax = {0.5, 0.5};
    text->rectTransform->sizeDelta = {50, 100};
    text->enableWordWrapping = true;

    progress->active = Manager::Rendering();
    cameraRig->progress = progress;
    cameraRig->progressText = text;
    cameraRig->mapString = Utils::GetMapString();

    return cameraRig;
}
