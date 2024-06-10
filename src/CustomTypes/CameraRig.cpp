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
    static auto GetLocalPosition =
        il2cpp_utils::resolve_icall<void, XRNode, ByRef<Vector3>>("UnityEngine.XR.InputTracking::GetLocalPosition_Injected");
    static auto GetLocalRotation =
        il2cpp_utils::resolve_icall<void, XRNode, ByRef<Quaternion>>("UnityEngine.XR.InputTracking::GetLocalRotation_Injected");
    static auto positionalTrackingDisabled = il2cpp_utils::resolve_icall<bool>("UnityEngine.XR.InputTracking::get_disablePositionalTracking");

    auto trans = transform;
    if (Manager::replaying && !Manager::paused) {
        pausedLastFrame = false;

        auto& transform = Manager::GetFrame();
        auto targetRot = transform.head.rotation;
        auto targetPos = transform.head.position;

        // set fake head position and let playertransforms use it as normal
        if (Manager::GetCurrentInfo().positionsAreLocal) {
            fakeHead->localRotation = targetRot;
            fakeHead->localPosition = targetPos;
        } else
            fakeHead->SetPositionAndRotation(targetPos, targetRot);

        if (getConfig().Avatar.GetValue()) {
            bool enabled = Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson;
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
        }
        if (Manager::Camera::moving && Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson)
            return;
        if (Manager::Camera::GetMode() != (int) CameraMode::Headset) {
            Quaternion rot;
            GetLocalRotation(XRNode::Head, byref(rot));
            rot = Quaternion::Inverse(rot);
            child->localRotation = rot;
            // if another mod disables positional tracking, it won't be applied to the camera
            // but we'll still get the real values, so we need to use zero instead
            if (!positionalTrackingDisabled()) {
                Vector3 pos;
                GetLocalPosition(XRNode::Head, byref(pos));
                pos = Sombrero::QuaternionMultiply(rot, pos);
                child->localPosition = pos * -1;
            } else
                child->localPosition = {};
        }
        if (Manager::Camera::rendering) {
            progress->active = true;
            UpdateProgress();
        }
    } else if (!pausedLastFrame) {
        pausedLastFrame = true;
        progress->active = false;
        avatar->gameObject->active = false;
        trans->localPosition = Vector3::zero();
        trans->localRotation = Quaternion::identity();
        child->localPosition = Vector3::zero();
        child->localRotation = Quaternion::identity();
    }
}

void CameraRig::SetPositionAndRotation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot) {
    // position can travel when moving, but rotation shouldn't
    if (!Manager::Camera::moving || Manager::Camera::GetMode() != (int) CameraMode::ThirdPerson) {
        if (Manager::Camera::rendering) {
            pos = {0, -1000, -1000};
            rot = Quaternion::Euler({0, 0, -1});
        }
        transform->rotation = rot;
    }
    transform->position = pos;
}

void CameraRig::UpdateProgress() {
    std::string typ = "";
    if (getConfig().SFX.GetValue())
        typ = Manager::Camera::GetAudioMode() ? " Audio" : " Video";
    std::string time = SecondsToString(Manager::GetSongTime());
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
    //  - cameraRig
    //     - child
    //        - cameraTransform
    //           - progress

    auto cameraGO = UnityEngine::GameObject::New_ctor("ReplayCameraRig");
    auto cameraRig = cameraGO->AddComponent<ReplayHelpers::CameraRig*>();
    cameraRig->cameraTransform = cameraTransform;

    auto parent = cameraTransform->parent;
    cameraRig->transform->SetParent(parent, false);

    auto playerTransforms = UnityEngine::Resources::FindObjectsOfTypeAll<PlayerTransforms*>()->First();
    auto headReplacement = UnityEngine::GameObject::New_ctor("PlayerTransformsHeadReplacement")->transform;
    headReplacement->SetParent(parent, false);
    playerTransforms->_headTransform = headReplacement;
    cameraRig->fakeHead = headReplacement;

    auto child = UnityEngine::GameObject::New_ctor("ReplayCameraRigChild")->transform;
    cameraRig->child = child;
    child->SetParent(cameraRig->transform, false);
    cameraTransform->SetParent(child, false);

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
    transform->SetParent(cameraRig->transform->parent);
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
