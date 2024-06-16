#include "CustomTypes/CameraRig.hpp"

#include "BeatSaber/BeatAvatarSDK/AvatarData.hpp"
#include "BeatSaber/BeatAvatarSDK/AvatarDataModel.hpp"
#include "BeatSaber/BeatAvatarSDK/BeatAvatarVisualController.hpp"
#include "Config.hpp"
#include "GlobalNamespace/BeatAvatarEditorFlowCoordinator.hpp"
#include "GlobalNamespace/PlayerTransforms.hpp"
#include "Main.hpp"
#include "Replay.hpp"
#include "ReplayManager.hpp"
#include "Sprites.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/XR/XRNode.hpp"

DEFINE_TYPE(ReplayHelpers, CameraRig);

using namespace ReplayHelpers;
using namespace UnityEngine::XR;

void CameraRig::Update() {
    auto trans = transform;
    if (Manager::replaying && !Manager::paused) {
        pausedLastFrame = false;

        auto mode = (CameraMode) Manager::Camera::GetMode();

        bool enabled = getConfig().Avatar.GetValue() && mode == CameraMode::ThirdPerson;
        avatar->gameObject->active = enabled;
        if (enabled) {
            auto& frame = Manager::GetFrame();
            auto& nextFrame = Manager::GetNextFrame();
            float lerp = Manager::GetFrameProgress();
            avatar->UpdateTransforms(
                Vector3::Lerp(frame.head.position, nextFrame.head.position, lerp),
                Vector3::Lerp(frame.leftHand.position, nextFrame.leftHand.position, lerp),
                Vector3::Lerp(frame.rightHand.position, nextFrame.rightHand.position, lerp),
                Quaternion::Lerp(frame.head.rotation, nextFrame.head.rotation, lerp),
                Quaternion::Lerp(frame.leftHand.rotation, nextFrame.leftHand.rotation, lerp),
                Quaternion::Lerp(frame.rightHand.rotation, nextFrame.rightHand.rotation, lerp)
            );
        }
        cameraTracker->enabled = mode == CameraMode::Headset || (Manager::Camera::moving && mode == CameraMode::ThirdPerson);
        if (Manager::Camera::rendering) {
            progress->active = true;
            UpdateProgress();
        }
    } else if (!pausedLastFrame) {
        pausedLastFrame = true;
        progress->active = false;
        avatar->gameObject->active = false;
        cameraTracker->enabled = true;
    }
}

void CameraRig::SetPositionAndRotation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot) {
    fakeHead->SetPositionAndRotation(pos, rot);
    if (Manager::Camera::rendering) {
        transform->position = {0, -1000, -1000};
        transform->rotation = Quaternion::Euler({0, 0, -1});
        return;
    }
    // position can travel when moving, but rotation shouldn't
    if (!Manager::Camera::moving || Manager::Camera::GetMode() != (int) CameraMode::ThirdPerson)
        transform->rotation = rot;
    transform->position = pos;
}

void CameraRig::UpdateProgress() {
    std::string typ = "";
    if (getConfig().SFX.GetValue())
        typ = Manager::Camera::GetAudioMode() ? " Audio" : " Video";
    std::string time = SecondsToString(fmin(Manager::GetSongTime(), Manager::GetAudioTime()));
    std::string tot = SecondsToString(Manager::GetLength());
    std::string queue = "";
    int len = getConfig().LevelsToSelect.GetValue().size();
    if (len > 0)
        queue = fmt::format("\n{} in queue", len);
    std::string label = fmt::format("{}\nRendering{}...\nSong Time: {} / {}{}", mapString, typ, time, tot, queue);
    progressText->text = label;
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

    // have the camera itself not be parented, since we calculate the local position offset in PlayerTransforms_Update
    auto cameraRig = cameraTransform->gameObject->AddComponent<ReplayHelpers::CameraRig*>();
    cameraRig->cameraTracker = cameraTransform->GetComponent<UnityEngine::SpatialTracking::TrackedPoseDriver*>();
    auto parent = cameraTransform->parent;
    cameraTransform->parent = nullptr;

    auto playerTransforms = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerTransforms*>()->First();
    auto headReplacement = UnityEngine::GameObject::New_ctor("PlayerTransformsHeadReplacement")->transform;
    headReplacement->SetParent(parent, false);
    playerTransforms->_headTransform = headReplacement;
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

    customAvatar->gameObject->active = Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson && getConfig().Avatar.GetValue();
    cameraRig->avatar = customAvatar;

    auto progress = BSML::Lite::CreateCanvas();
    progress->name = "RenderProgressScreen";
    transform = progress->transform;
    transform->SetParent(cameraTransform, false);
    transform->localPosition = {0, 0, 2};
    transform->localScale = {0.05, 0.05, 0.05};

    auto image = BSML::Lite::CreateImage(progress, GetWhiteIcon());
    image->color = UnityEngine::Color::get_black();
    image->rectTransform->anchorMin = {0, 0};
    image->rectTransform->anchorMax = {1, 1};
    auto text = BSML::Lite::CreateText(progress, "Rendering...");
    text->alignment = TMPro::TextAlignmentOptions::Center;
    text->rectTransform->anchorMin = {0.5, 0.5};
    text->rectTransform->anchorMax = {0.5, 0.5};
    text->rectTransform->sizeDelta = {60, 100};
    text->enableWordWrapping = true;

    progress->active = Manager::Camera::rendering;
    cameraRig->progress = progress;
    cameraRig->progressText = text;

    cameraRig->pausedLastFrame = false;
    cameraRig->mapString = GetMapString(Manager::beatmap);

    return cameraRig;
}
