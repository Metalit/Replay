#include "Main.hpp"
#include "CustomTypes/CameraRig.hpp"
#include "Config.hpp"

#include "ReplayManager.hpp"

#include "UnityEngine/Transform.hpp"

#include "UnityEngine/XR/XRNode.hpp"

DEFINE_TYPE(ReplayHelpers, CameraRig);

using namespace ReplayHelpers;
using namespace UnityEngine::XR;

inline Quaternion Subtract(Quaternion a, Quaternion& b) {
    return Sombrero::QuaternionMultiply(a, Quaternion::Inverse(b));
}

void CameraRig::Update() {
    static auto disablePositionalTracking = il2cpp_utils::resolve_icall<void, bool>
        ("UnityEngine.XR.InputTracking::set_disablePositionalTracking");

    static auto GetLocalRotation = il2cpp_utils::resolve_icall<void, XRNode, ByRef<Quaternion>>
        ("UnityEngine.XR.InputTracking::GetLocalRotation_Injected");
    
    auto trans = get_transform();
    if(Manager::replaying && !Manager::paused && Manager::Camera::GetMode() != (int) CameraMode::Headset) {
        if(tracking) {
            disablePositionalTracking(true);
            tracking = false;
        }
        Quaternion rot;
        GetLocalRotation(XRNode::Head, byref(rot));
        auto targetPos = Manager::Camera::GetHeadPosition();
        auto targetRot = Subtract(Manager::Camera::GetHeadRotation(), rot);
        if(Manager::currentReplay.type == ReplayType::Event) {
            trans->set_localPosition(targetPos);
            trans->set_localRotation(targetRot);
        } else {
            trans->set_position(targetPos);
            trans->set_rotation(targetRot);
        }
    } else {
        disablePositionalTracking(false);
        tracking = true;
        trans->set_localPosition(Vector3::zero());
        trans->set_localRotation(Quaternion::identity());
    }
}

void CameraRig::dtor() {
    static auto disablePositionalTracking = il2cpp_utils::resolve_icall<void, bool>
        ("UnityEngine.XR.InputTracking::disablePositionalTracking");
    disablePositionalTracking(false);
}
