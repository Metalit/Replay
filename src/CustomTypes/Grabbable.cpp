#include "Main.hpp"
#include "CustomTypes/Grabbable.hpp"

#include "UnityEngine/Transform.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/EventSystems/PointerEventData.hpp"

DEFINE_TYPE(ReplayHelpers, Grabbable);

using namespace ReplayHelpers;
using namespace GlobalNamespace;

void Grabbable::Update() {
    if(get_transform()->get_position().y < 0) {
        auto pos = get_transform()->get_position();
        get_transform()->set_position({pos.x, 0, pos.z});
    }

    if(!inputModule)
        inputModule = Object::FindObjectOfType<VRUIControls::VRInputModule*>();
    auto pointer = inputModule->vrPointer;

    bool wasGrabbing = controller != nullptr;

    if(pointer->get_vrController()->get_triggerValue() > 0.9) {
        // pointerData can be null when not loaded (aka on soft restarts, generally)
        if(controller != pointer->get_vrController() && pointer->pointerData) {
            auto eventData = inputModule->GetLastPointerEventData(-1);
            // check if pointer is on the cube
            bool hovering = false;
            if(eventData) {
                hovering = eventData->pointerEnter == get_gameObject();
                if(!hovering && eventData->pointerEnter)
                    hovering = eventData->pointerEnter->get_transform()->get_parent() == get_transform();
            }
            if(hovering) {
                controller = pointer->get_vrController();
                grabPos = pointer->get_vrController()->get_transform()->InverseTransformPoint(get_transform()->get_position());
                grabRot = UnityEngine::Quaternion::Inverse(pointer->get_vrController()->get_transform()->get_rotation()) * get_transform()->get_rotation();
            } else
                controller = nullptr;
        }
    } else
        controller = nullptr;

    if(wasGrabbing && !controller)
        onRelease(get_gameObject());

    if(!controller)
        return;

    // thumbstick movement
    if(pointer->_get__lastControllerUsedWasRight()) {
        float diff = controller->get_verticalAxisValue() * UnityEngine::Time::get_unscaledDeltaTime();
        // no movement if too close
        if(grabPos.get_magnitude() < 0.5 && diff > 0)
            diff = 0;
        grabPos = grabPos - (UnityEngine::Vector3::get_forward() * diff);
        // rotate on the third axis horizontally
        float h_move = -30 * controller->get_horizontalAxisValue() * UnityEngine::Time::get_unscaledDeltaTime();
        grabRot = grabRot * UnityEngine::Quaternion::Euler({0, 0, h_move});
    }
    else {
        // scale values to a reasonable speed
        float v_move = -30 * controller->get_verticalAxisValue() * UnityEngine::Time::get_unscaledDeltaTime();
        float h_move = -30 * controller->get_horizontalAxisValue() * UnityEngine::Time::get_unscaledDeltaTime();
        auto extraRot = UnityEngine::Quaternion::Euler({0, h_move, 0}) * UnityEngine::Quaternion::Euler({v_move, 0, 0});
        grabRot = grabRot * extraRot;
    }

    auto pos = controller->get_transform()->TransformPoint(grabPos);
    auto rot = controller->get_transform()->get_rotation() * grabRot;

    get_transform()->set_position(UnityEngine::Vector3::Lerp(get_transform()->get_position(), pos, 10 * UnityEngine::Time::get_unscaledDeltaTime()));
    get_transform()->set_rotation(UnityEngine::Quaternion::Slerp(get_transform()->get_rotation(), rot, 5 * UnityEngine::Time::get_unscaledDeltaTime()));
}
