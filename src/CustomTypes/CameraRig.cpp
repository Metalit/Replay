#include "Main.hpp"
#include "CustomTypes/CameraRig.hpp"
#include "Config.hpp"
#include "Replay.hpp"

#include "ReplayManager.hpp"

#include "UnityEngine/Transform.hpp"

#include "UnityEngine/XR/XRNode.hpp"

DEFINE_TYPE(ReplayHelpers, CameraRig);

using namespace ReplayHelpers;
using namespace UnityEngine::XR;

void CameraRig::Update() {
    static auto GetLocalPosition = il2cpp_utils::resolve_icall<void, XRNode, ByRef<Vector3>>
        ("UnityEngine.XR.InputTracking::GetLocalPosition_Injected");
    static auto GetLocalRotation = il2cpp_utils::resolve_icall<void, XRNode, ByRef<Quaternion>>
        ("UnityEngine.XR.InputTracking::GetLocalRotation_Injected");
    static auto positionalTrackingDisabled = il2cpp_utils::resolve_icall<bool>
        ("UnityEngine.XR.InputTracking::get_disablePositionalTracking");

    auto trans = get_transform();
    if(Manager::replaying && !Manager::paused) {
        pausedLastFrame = false;

        if(getConfig().Avatar.GetValue()) {
            bool enabled = Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson;
            avatar->get_gameObject()->SetActive(enabled);
            if(enabled) {
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
        if(Manager::Camera::moving && Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson)
            return;
        if(Manager::Camera::GetMode() != (int) CameraMode::Headset) {
            Quaternion rot;
            GetLocalRotation(XRNode::Head, byref(rot));
            rot = Quaternion::Inverse(rot);
            child->set_localRotation(rot);
            // if another mod disables positional tracking, it won't be applied to the camera
            // but we'll still get the real values, so we need to use zero instead
            if(!positionalTrackingDisabled()) {
                Vector3 pos;
                GetLocalPosition(XRNode::Head, byref(pos));
                pos = Sombrero::QuaternionMultiply(rot, pos);
                child->set_localPosition(pos * -1);
            } else
                child->set_localPosition({});
        }
    } else if(!pausedLastFrame) {
        pausedLastFrame = true;
        avatar->get_gameObject()->SetActive(false);
        trans->set_localPosition(Vector3::zero());
        trans->set_localRotation(Quaternion::identity());
        child->set_localPosition(Vector3::zero());
        child->set_localRotation(Quaternion::identity());
    }
}

void CameraRig::SetPositionAndRotation(UnityEngine::Vector3 pos, UnityEngine::Quaternion rot) {
    get_transform()->set_position(pos);
    // position can travel when moving, but rotation shouldn't
    if(!Manager::Camera::moving || Manager::Camera::GetMode() != (int) CameraMode::ThirdPerson)
        get_transform()->set_rotation(rot);
}

#include "GlobalNamespace/AvatarDataModel.hpp"
#include "GlobalNamespace/AvatarData.hpp"
#include "GlobalNamespace/AvatarVisualController.hpp"

using namespace GlobalNamespace;

CameraRig* CameraRig::Create(UnityEngine::Transform* cameraTransform) {
    auto cameraGO = UnityEngine::GameObject::New_ctor("ReplayCameraRig");
    auto cameraRig = cameraGO->AddComponent<ReplayHelpers::CameraRig*>();
    cameraRig->get_transform()->SetParent(cameraTransform->GetParent(), false);

    auto child = UnityEngine::GameObject::New_ctor("ReplayCameraRigChild")->get_transform();
    cameraRig->child = child;
    child->SetParent(cameraRig->get_transform(), false);
    cameraTransform->SetParent(child, false);

    auto playerAvatar = UnityEngine::Resources::FindObjectsOfTypeAll<AvatarPoseController*>().First([](auto x) {
        return x->get_name() == "PlayerAvatar";
    });
    auto customAvatar = UnityEngine::Object::Instantiate(playerAvatar);
    UnityEngine::GameObject::SetName(customAvatar, "ReplayCustomAvatar");
    auto visualController = customAvatar->GetComponent<AvatarVisualController*>();

    auto dataModel = UnityEngine::Resources::FindObjectsOfTypeAll<AvatarDataModel*>().First();
    visualController->avatarPartsModel = dataModel->avatarPartsModel;
    visualController->UpdateAvatarVisual(dataModel->avatarData);

    auto transform = customAvatar->get_transform();
    transform->SetParent(cameraRig->get_transform()->GetParent());
    transform->set_position({0, 0, 0});
    transform->set_localScale({1, 1, 1});

    customAvatar->get_gameObject()->SetActive(Manager::Camera::GetMode() == (int) CameraMode::ThirdPerson && getConfig().Avatar.GetValue());
    cameraRig->avatar = customAvatar;

    cameraRig->pausedLastFrame = false;

    return cameraRig;
}
