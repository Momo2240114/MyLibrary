#include "ThirdPersonCameraComponent.h"

#include "CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
    DirectX::XMFLOAT3 AddFloat3(const DirectX::XMFLOAT3& left_, const DirectX::XMFLOAT3& right_)
    {
        return { left_.x + right_.x, left_.y + right_.y, left_.z + right_.z };
    }

    DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_, float rate_)
    {
        return
        {
            start_.x + (end_.x - start_.x) * rate_,
            start_.y + (end_.y - start_.y) * rate_,
            start_.z + (end_.z - start_.z) * rate_
        };
    }
}

void ThirdPersonCameraComponent::Start()
{
    ResolveCamera();
    ResolveTarget();
}

void ThirdPersonCameraComponent::Update(float deltaTime_)
{
    ResolveCamera();
    ResolveTarget();

    auto targetObject_ = target.lock();
    auto owner_ = GetGameObject();
    if (!targetObject_ || !owner_ || !owner_->transform || !targetObject_->transform) return;

    Vector2 mouseDelta_ = MouseInput::Instance().GetSubMousePos();
    DirectX::XMFLOAT2 stickDelta_{ 0.0f, 0.0f };
    auto& controller_ = ControllerInput::Instance();
    if (controller_.IsConnected())
    {
        stickDelta_ = controller_.GetRightStick();
    }

    float lookX_ = stickDelta_.x + mouseDelta_.x * mouseSensitivity;
    float lookY_ = -stickDelta_.y + mouseDelta_.y * mouseSensitivity;

    currentYaw += -lookX_ * lookSpeedX * deltaTime_;
    currentPitch += -lookY_ * lookSpeedY * deltaTime_;
    currentPitch = std::clamp(currentPitch, minPitch, maxPitch);

    float yawRad_ = DirectX::XMConvertToRadians(currentYaw);
    float pitchRad_ = DirectX::XMConvertToRadians(currentPitch);

    DirectX::XMFLOAT3 front_
    {
        std::cos(yawRad_) * std::cos(pitchRad_),
        std::sin(pitchRad_),
        std::sin(yawRad_) * std::cos(pitchRad_)
    };

    DirectX::XMVECTOR frontVec_ = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&front_));
    DirectX::XMStoreFloat3(&front_, frontVec_);

    DirectX::XMFLOAT3 targetWorldPos_ = targetObject_->transform->GetWorldPosition();
    DirectX::XMFLOAT3 lookAtPos_ = AddFloat3(targetWorldPos_, targetOffset);

    DirectX::XMVECTOR lookAtVec_ = DirectX::XMLoadFloat3(&lookAtPos_);
    DirectX::XMVECTOR goalPosVec_ = DirectX::XMVectorSubtract(lookAtVec_, DirectX::XMVectorScale(frontVec_, distance));
    DirectX::XMFLOAT3 goalPos_;
    DirectX::XMStoreFloat3(&goalPos_, goalPosVec_);

    DirectX::XMFLOAT3 currentPos_ = owner_->transform->localPosition;
    float lerpRate_ = 1.0f - std::exp(-followSpeed * deltaTime_);
    DirectX::XMFLOAT3 nextPos_ = LerpFloat3(currentPos_, goalPos_, lerpRate_);

    DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixLookToLH(
        DirectX::XMLoadFloat3(&nextPos_),
        frontVec_,
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
    );
    DirectX::XMVECTOR determinant_;
    DirectX::XMMATRIX worldMatrix_ = DirectX::XMMatrixInverse(&determinant_, viewMatrix_);
    owner_->transform->ApplyWorldMatrix(worldMatrix_);

    if (auto camera_ = camera.lock())
    {
        camera_->SetLookAt(nextPos_, lookAtPos_, { 0.0f, 1.0f, 0.0f });
    }
}

void ThirdPersonCameraComponent::Gui()
{
    char targetBuffer_[128] = {};
    strcpy_s(targetBuffer_, targetName.c_str());
    if (ImGui::InputText("Target Object", targetBuffer_, sizeof(targetBuffer_)))
    {
        targetName = targetBuffer_;
        target.reset();
        ResolveTarget();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            std::shared_ptr<GameObject> droppedObject_ = *(std::shared_ptr<GameObject>*)payload_->Data;
            SetTarget(droppedObject_);
        }
        ImGui::EndDragDropTarget();
    }

    if (auto targetObject_ = target.lock())
    {
        ImGui::Text("Current Target: %s", targetObject_->GetName().c_str());
    }
    else
    {
        ImGui::TextDisabled("Drop a GameObject from Hierarchy here.");
    }

    ImGui::Separator();
    ImGui::DragFloat("Distance", &distance, 0.1f, 0.1f, 100.0f, "%.2f");
    ImGui::DragFloat("Pitch", &currentPitch, 0.1f, minPitch, maxPitch, "%.2f");
    ImGui::DragFloat("Yaw", &currentYaw, 0.1f, -360.0f, 360.0f, "%.2f");
    ImGui::DragFloatRange2("Pitch Limit", &minPitch, &maxPitch, 0.1f, -89.0f, 89.0f, "Min: %.1f", "Max: %.1f");
    ImGui::DragFloat("Look Speed X", &lookSpeedX, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("Look Speed Y", &lookSpeedY, 1.0f, 1.0f, 1000.0f, "%.1f");
    ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 0.0f, 10.0f, "%.2f");
    ImGui::DragFloat("Follow Speed", &followSpeed, 0.1f, 0.1f, 100.0f, "%.2f");
    ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.1f);
}

void ThirdPersonCameraComponent::Serialize(nlohmann::json& json_) const
{
    json_["targetName"] = targetName;
    json_["distance"] = distance;
    json_["currentPitch"] = currentPitch;
    json_["currentYaw"] = currentYaw;
    json_["minPitch"] = minPitch;
    json_["maxPitch"] = maxPitch;
    json_["lookSpeedX"] = lookSpeedX;
    json_["lookSpeedY"] = lookSpeedY;
    json_["mouseSensitivity"] = mouseSensitivity;
    json_["followSpeed"] = followSpeed;
    json_["targetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
}

void ThirdPersonCameraComponent::Deserialize(const nlohmann::json& json_)
{
    targetName = json_.value("targetName", "");
    distance = json_.value("distance", 6.0f);
    currentPitch = json_.value("currentPitch", 20.0f);
    currentYaw = json_.value("currentYaw", 0.0f);
    minPitch = json_.value("minPitch", -60.0f);
    maxPitch = json_.value("maxPitch", 60.0f);
    lookSpeedX = json_.value("lookSpeedX", 300.0f);
    lookSpeedY = json_.value("lookSpeedY", 200.0f);
    mouseSensitivity = json_.value("mouseSensitivity", 0.1f);
    followSpeed = json_.value("followSpeed", 15.0f);

    if (json_.contains("targetOffset"))
    {
        targetOffset =
        {
            json_["targetOffset"][0].get<float>(),
            json_["targetOffset"][1].get<float>(),
            json_["targetOffset"][2].get<float>()
        };
    }
}

void ThirdPersonCameraComponent::ResolveTarget()
{
    if (target.lock() || targetName.empty()) return;

    auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;

    target = owner_->manager->Find(targetName);
}

void ThirdPersonCameraComponent::ResolveCamera()
{
    if (camera.lock()) return;

    auto owner_ = GetGameObject();
    if (!owner_) return;

    camera = owner_->GetComponent<CameraComponent>();
}

void ThirdPersonCameraComponent::SetTarget(std::shared_ptr<GameObject> target_)
{
    target = target_;
    targetName = target_ ? target_->GetName() : "";
}

REGISTER_COMPONENT(ThirdPersonCameraComponent, "ThirdPersonCameraComponent")
