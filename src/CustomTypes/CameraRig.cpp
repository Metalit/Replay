#include "CustomTypes/CameraRig.hpp"

#include "BeatSaber/BeatAvatarSDK/AvatarData.hpp"
#include "BeatSaber/BeatAvatarSDK/AvatarDataModel.hpp"
#include "BeatSaber/BeatAvatarSDK/BeatAvatarVisualController.hpp"
#include "GlobalNamespace/BeatAvatarEditorFlowCoordinator.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "UnityEngine/Resources.hpp"
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

using namespace Replay;

void CameraRig::SetPositionAndRotation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot) {
    // when rendering, show a progress screen in the middle of nowhere instead of the gameplay
    if (Manager::Rendering() && !Manager::Paused()) {
        transform->position = {0, -1000, -1000};
        transform->rotation = Quaternion::Euler({0, 0, -1});
        UpdateProgress();
        return;
    }
    transform->rotation = rot;
    transform->position = pos;
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

void CameraRig::SetTrackingEnabled(bool value) {
    if (cameraTracker->enabled == value)
        return;
    cameraTracker->enabled = value;
    // when disabling tracking, remove all of its effects
    if (!value)
        cameraTracker->transform->SetLocalPositionAndRotation(Vector3::zero(), Quaternion::identity());
    // when paused, we want to take the player's real position and rotation into account
    // otherwise, we want to avoid a sudden teleportation when we change if tracking is enabled
    cameraTracker->CacheLocalPosition();
    cameraTracker->m_UseRelativeTransform = !Manager::Paused();
}

UnityEngine::Vector3 CameraRig::GetCameraPosition() {
    return cameraTracker->transform->position;
}

UnityEngine::Quaternion CameraRig::GetCameraRotation() {
    return cameraTracker->transform->rotation;
}

using namespace GlobalNamespace;
using namespace BeatSaber::BeatAvatarSDK;

CameraRig* CameraRig::Create(UnityEngine::Transform* cameraTransform) {
    // original hierarchy:
    // parent
    //  - cameraTransform (playerTransforms->headTransform)

    // new hierarchy:
    // parent
    //  - headReplacement (playerTransforms->headTransform)
    //  - customAvatar
    // cameraTransform (CameraRig component, root object)
    //  - progress

    // have the camera itself not be parented, since we calculate the local position offset ourselves
    auto cameraRig = cameraTransform->gameObject->AddComponent<Replay::CameraRig*>();
    cameraRig->cameraTracker = cameraTransform->GetComponent<UnityEngine::SpatialTracking::TrackedPoseDriver*>();
    auto parent = cameraTransform->parent;
    cameraTransform->SetParent(nullptr, true);

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
