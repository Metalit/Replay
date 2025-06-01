#include "pause.hpp"

#include "CustomTypes/Grabbable.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Shader.hpp"
#include "config.hpp"

static void SetCameraModelToThirdPerson(UnityEngine::GameObject* cameraModel) {
    auto trans = cameraModel->transform;
    Quaternion rot = Quaternion::Euler(getConfig().ThirdPerRot.GetValue());
    // model points upwards
    Quaternion offset = Quaternion::AngleAxis(90, {1, 0, 0});
    rot = rot * offset;
    auto pos = getConfig().ThirdPerPos.GetValue();
    trans->SetPositionAndRotation(pos, rot);
}

static void SetThirdPersonToCameraModel(UnityEngine::GameObject* cameraModel) {
    auto trans = cameraModel->transform;
    Quaternion rot = trans->rotation;
    // model points upwards
    auto offset = Quaternion::AngleAxis(-90, {1, 0, 0});
    rot = rot * offset;
    getConfig().ThirdPerRot.SetValue(rot.eulerAngles);
    auto pos = trans->position;
    getConfig().ThirdPerPos.SetValue(pos);
}

static UnityEngine::GameObject*
CreateCube(UnityEngine::GameObject* parent, UnityEngine::Material* mat, Vector3 pos, Vector3 euler, Vector3 scale, std::string_view name) {
    auto cube = UnityEngine::GameObject::CreatePrimitive(UnityEngine::PrimitiveType::Cube);
    cube->GetComponent<UnityEngine::Renderer*>()->SetMaterial(mat);
    auto trans = cube->transform;
    if (parent)
        trans->SetParent(parent->transform, false);
    trans->localPosition = pos;
    trans->localEulerAngles = euler;
    trans->localScale = scale;
    cube->name = name;
    return cube;
}

static UnityEngine::GameObject* CreateCameraModel() {
    auto mat = UnityEngine::Material::New_ctor(UnityEngine::Shader::Find("Custom/SimpleLit"));
    mat->set_color(UnityEngine::Color::get_white());
    auto ret = CreateCube(nullptr, mat, {}, {}, {0.075, 0.075, 0.075}, "ReplayCameraModel");
    ret->AddComponent<Replay::Grabbable*>()->onRelease = SetThirdPersonToCameraModel;
    CreateCube(ret, mat, {-1.461, 1.08, 1.08}, {45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 0");
    CreateCube(ret, mat, {1.461, 1.08, 1.08}, {45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 1");
    CreateCube(ret, mat, {1.461, 1.08, -1.08}, {-45, 0, -45}, {0.133, 4, 0.133}, "Camera Pillar 2");
    CreateCube(ret, mat, {-1.461, 1.08, -1.08}, {-45, 0, 45}, {0.133, 4, 0.133}, "Camera Pillar 3");
    CreateCube(ret, mat, {0, 2.08, 0}, {}, {5.845, 0.07, 4.322}, "Camera Screen");
    return ret;
}
