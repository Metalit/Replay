#include "CustomTypes/Grabbable.hpp"

#include "UnityEngine/EventSystems/PointerEventData.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Time.hpp"
#include "UnityEngine/Transform.hpp"
#include "VRUIControls/VRPointer.hpp"
#include "main.hpp"

DEFINE_TYPE(Replay, Grabbable);

using namespace GlobalNamespace;
using namespace Replay;

void Grabbable::Update() {
    if (transform->position.y < 0) {
        auto pos = transform->position;
        transform->position = {pos.x, 0, pos.z};
    }
    if (!inputModule)
        inputModule = Object::FindObjectOfType<VRUIControls::VRInputModule*>();

    auto pointer = inputModule->_vrPointer;

    bool wasGrabbing = controller != nullptr;

    if (pointer->lastSelectedVrController->triggerValue > 0.9) {
        // pointerData can be null when not loaded (aka on soft restarts, generally)
        if (controller != pointer->lastSelectedVrController.unsafePtr() && pointer->_currentPointerData) {
            auto eventData = inputModule->GetLastPointerEventData(-1);
            // check if pointer is on the cube
            bool hovering = false;
            if (eventData) {
                hovering = eventData->pointerEnter == gameObject;
                if (!hovering && eventData->pointerEnter)
                    hovering = eventData->pointerEnter->transform->parent == transform;
            }
            if (hovering) {
                controller = pointer->lastSelectedVrController;
                grabPos = pointer->lastSelectedVrController->transform->InverseTransformPoint(transform->position);
                grabRot = Quaternion(Quaternion::Inverse(pointer->lastSelectedVrController->transform->rotation)) * transform->rotation;
            } else
                controller = nullptr;
        }
    } else
        controller = nullptr;

    if (wasGrabbing && !controller)
        onRelease();

    if (!controller)
        return;

    // thumbstick movement
    if (pointer->_lastSelectedControllerWasRight) {
        float diff = controller->thumbstick.y * UnityEngine::Time::get_unscaledDeltaTime();
        // no movement if too close
        if (grabPos.magnitude < 0.5 && diff > 0)
            diff = 0;
        grabPos = grabPos - (Vector3::forward() * diff);
        // rotate on the third axis horizontally
        float h_move = -30 * controller->thumbstick.x * UnityEngine::Time::get_unscaledDeltaTime();
        grabRot = grabRot * Quaternion::Euler({0, 0, h_move});
    } else {
        // scale values to a reasonable speed
        float v_move = -30 * controller->thumbstick.y * UnityEngine::Time::get_unscaledDeltaTime();
        float h_move = -30 * controller->thumbstick.x * UnityEngine::Time::get_unscaledDeltaTime();
        auto extraRot = Quaternion(Quaternion::Euler({0, h_move, 0})) * Quaternion::Euler({v_move, 0, 0});
        grabRot = grabRot * extraRot;
    }

    auto pos = controller->transform->TransformPoint(grabPos);
    auto rot = Quaternion(controller->transform->rotation) * grabRot;

    transform->position = Vector3::Lerp(transform->position, pos, 10 * UnityEngine::Time::get_unscaledDeltaTime());
    transform->rotation = Quaternion::Slerp(transform->rotation, rot, 5 * UnityEngine::Time::get_unscaledDeltaTime());
}
