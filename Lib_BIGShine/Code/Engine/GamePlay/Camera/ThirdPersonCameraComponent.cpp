#include "PrecompiledHeader.h"

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
    DirectX::XMFLOAT3 AddFloat3(
        const DirectX::XMFLOAT3& left_,
        const DirectX::XMFLOAT3& right_)
    {
        return { left_.x + right_.x, left_.y + right_.y, left_.z + right_.z };
    }

    DirectX::XMFLOAT3 LerpFloat3(
        const DirectX::XMFLOAT3& start_,
        const DirectX::XMFLOAT3& end_,
        float rate_)
    {
        return
        {
            start_.x + (end_.x - start_.x) * rate_,
            start_.y + (end_.y - start_.y) * rate_,
            start_.z + (end_.z - start_.z) * rate_
        };
    }

    float ExpSmoothingRate(float speed_, float deltaTime_)
    {
        return 1.0f - std::exp(-speed_ * deltaTime_);
    }

    float SmoothStep(float value_)
    {
        const float clamped_ = std::clamp(value_, 0.0f, 1.0f);
        return clamped_ * clamped_ * (3.0f - 2.0f * clamped_);
    }
}

void ThirdPersonCameraComponent::Start()
{
    ResolveCamera();
    ResolveTarget();
}

void ThirdPersonCameraComponent::LateUpdate(float deltaTime_)
{
    ResolveCamera();
    ResolveTarget();
    if (controlSuppressed) return;

    const std::shared_ptr<GameObject> targetObject_ = target.lock();
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!targetObject_ || !owner_ ||
        !owner_->transform || !targetObject_->transform)
    {
        return;
    }

    if (!returnBlendActive)
    {
        const Vector2 mouseDelta_ = MouseInput::Instance().GetSubMousePos();
        DirectX::XMFLOAT2 stickDelta_{ 0.0f, 0.0f };
        ControllerInput& controller_ = ControllerInput::Instance();
        if (controller_.IsConnected()) stickDelta_ = controller_.GetRightStick();

        const float lookX_ = stickDelta_.x + mouseDelta_.x * mouseSensitivity;
        const float lookY_ = -stickDelta_.y + mouseDelta_.y * mouseSensitivity;
        currentYaw += -lookX_ * lookSpeedX * deltaTime_;
        currentPitch += -lookY_ * lookSpeedY * deltaTime_;
        currentPitch = std::clamp(currentPitch, minPitch, maxPitch);
    }

    const float yawRadian_ = DirectX::XMConvertToRadians(currentYaw);
    const float pitchRadian_ = DirectX::XMConvertToRadians(currentPitch);
    DirectX::XMFLOAT3 front_
    {
        std::cos(yawRadian_) * std::cos(pitchRadian_),
        std::sin(pitchRadian_),
        std::sin(yawRadian_) * std::cos(pitchRadian_)
    };
    DirectX::XMStoreFloat3(
        &front_,
        DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&front_)));

    const DirectX::XMFLOAT3 targetPosition_ =
        targetObject_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 followPosition_ =
        AddFloat3(targetPosition_, targetOffset);
    if (!hasCameraCenter)
    {
        smoothedCameraCenter = followPosition_;
        hasCameraCenter = true;
    }
    smoothedCameraCenter = LerpFloat3(
        smoothedCameraCenter,
        followPosition_,
        ExpSmoothingRate(objectFollowSpeed, deltaTime_));

    const DirectX::XMVECTOR cameraPositionVector_ = DirectX::XMVectorSubtract(
        DirectX::XMLoadFloat3(&smoothedCameraCenter),
        DirectX::XMVectorScale(DirectX::XMLoadFloat3(&front_), distance));
    DirectX::XMFLOAT3 cameraPosition_{};
    DirectX::XMStoreFloat3(&cameraPosition_, cameraPositionVector_);
    DirectX::XMFLOAT3 lookAtPosition_ = followPosition_;
    if (returnBlendActive)
    {
        returnBlendTimer += deltaTime_;
        const float blendRate_ = CalculateReturnBlendRate();
        cameraPosition_ = LerpFloat3(
            returnBlendStartPosition,
            cameraPosition_,
            blendRate_);
        lookAtPosition_ = LerpFloat3(
            returnBlendStartLookAt,
            followPosition_,
            blendRate_);
        if (returnBlendTimer >= returnBlendDuration)
        {
            returnBlendActive = false;
            returnBlendTimer = 0.0f;
        }
    }

    const DirectX::XMMATRIX viewMatrix_ = DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(&cameraPosition_),
        DirectX::XMLoadFloat3(&lookAtPosition_),
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    DirectX::XMVECTOR determinant_;
    owner_->transform->ApplyWorldMatrix(
        DirectX::XMMatrixInverse(&determinant_, viewMatrix_));

    if (const std::shared_ptr<CameraComponent> camera_ = camera.lock())
    {
        camera_->SetLookAt(
            cameraPosition_,
            lookAtPosition_,
            { 0.0f, 1.0f, 0.0f });
    }
}

void ThirdPersonCameraComponent::SetControlSuppressed(bool suppressed_)
{
    controlSuppressed = suppressed_;
    if (!controlSuppressed) ResetFollowTracking();
}

void ThirdPersonCameraComponent::SynchronizeViewAngles(
    const DirectX::XMFLOAT3& cameraPosition_,
    const DirectX::XMFLOAT3& lookAtPosition_)
{
    const DirectX::XMVECTOR directionVector_ = DirectX::XMVector3Normalize(
        DirectX::XMVectorSubtract(
            DirectX::XMLoadFloat3(&lookAtPosition_),
            DirectX::XMLoadFloat3(&cameraPosition_)));
    DirectX::XMFLOAT3 direction_{};
    DirectX::XMStoreFloat3(&direction_, directionVector_);
    currentPitch = DirectX::XMConvertToDegrees(
        std::asin(std::clamp(direction_.y, -1.0f, 1.0f)));
    currentPitch = std::clamp(currentPitch, minPitch, maxPitch);
    currentYaw = DirectX::XMConvertToDegrees(
        std::atan2(direction_.z, direction_.x));
}

void ThirdPersonCameraComponent::BeginReturnBlendFromCurrentCamera()
{
    ResolveCamera();
    const std::shared_ptr<CameraComponent> camera_ = camera.lock();
    const std::shared_ptr<GameObject> targetObject_ = target.lock();
    if (!camera_ || !targetObject_ || !targetObject_->transform)
    {
        ResetFollowTracking();
        return;
    }

    returnBlendStartPosition = camera_->GetEye();
    returnBlendStartLookAt = camera_->GetFocus();

    const DirectX::XMFLOAT3 targetPosition_ =
        targetObject_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 followPosition_ =
        AddFloat3(targetPosition_, targetOffset);
    SynchronizeViewAngles(returnBlendStartPosition, followPosition_);

    const DirectX::XMVECTOR distanceVector_ = DirectX::XMVectorSubtract(
        DirectX::XMLoadFloat3(&followPosition_),
        DirectX::XMLoadFloat3(&returnBlendStartPosition));
    const float currentDistance_ =
        DirectX::XMVectorGetX(DirectX::XMVector3Length(distanceVector_));
    if (currentDistance_ > 0.001f)
    {
        distance = currentDistance_;
    }

    smoothedCameraCenter = followPosition_;
    hasCameraCenter = true;
    returnBlendTimer = 0.0f;
    returnBlendActive = returnBlendDuration > 0.0001f;
}

float ThirdPersonCameraComponent::CalculateReturnBlendRate() const
{
    if (returnBlendDuration <= 0.0001f) return 1.0f;
    return SmoothStep(returnBlendTimer / returnBlendDuration);
}

void ThirdPersonCameraComponent::Gui()
{
    char targetBuffer_[128]{};
    strcpy_s(targetBuffer_, targetName.c_str());
    if (ImGui::InputText("Target Object", targetBuffer_, sizeof(targetBuffer_)))
    {
        targetName = targetBuffer_;
        target.reset();
        ResolveTarget();
    }
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ =
            ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            SetTarget(*(std::shared_ptr<GameObject>*)payload_->Data);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::TextDisabled(controlSuppressed
        ? "Control is owned by CameraLockOnComponent."
        : "TPS control active.");
    ImGui::DragFloat("Distance", &distance, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Pitch", &currentPitch, 0.1f, minPitch, maxPitch);
    ImGui::DragFloat("Yaw", &currentYaw, 0.1f, -360.0f, 360.0f);
    ImGui::DragFloatRange2(
        "Pitch Limit",
        &minPitch,
        &maxPitch,
        0.1f,
        -89.0f,
        89.0f);
    ImGui::DragFloat("Look Speed X", &lookSpeedX, 1.0f, 1.0f, 1000.0f);
    ImGui::DragFloat("Look Speed Y", &lookSpeedY, 1.0f, 1.0f, 1000.0f);
    ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Object Follow Speed", &objectFollowSpeed, 0.1f, 0.1f, 100.0f);
    if (ImGui::DragFloat3("Target Offset", &targetOffset.x, 0.1f))
    {
        ResetFollowTracking();
    }
    ImGui::DragFloat("Return Blend Duration", &returnBlendDuration, 0.01f, 0.0f, 2.0f);
    ImGui::TextDisabled(returnBlendActive ? "Return blending active." : "Return blending idle.");
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
    json_["objectFollowSpeed"] = objectFollowSpeed;
    json_["returnBlendDuration"] = returnBlendDuration;
    json_["targetOffset"] = { targetOffset.x, targetOffset.y, targetOffset.z };
}

void ThirdPersonCameraComponent::Deserialize(const nlohmann::json& json_)
{
    targetName = json_.value("targetName", std::string{});
    distance = json_.value("distance", 6.0f);
    currentPitch = json_.value("currentPitch", 20.0f);
    currentYaw = json_.value("currentYaw", 0.0f);
    minPitch = json_.value("minPitch", -60.0f);
    maxPitch = json_.value("maxPitch", 60.0f);
    lookSpeedX = json_.value("lookSpeedX", 300.0f);
    lookSpeedY = json_.value("lookSpeedY", 200.0f);
    mouseSensitivity = json_.value("mouseSensitivity", 0.1f);
    objectFollowSpeed = json_.value("objectFollowSpeed", 5.0f);
    returnBlendDuration = json_.value("returnBlendDuration", 0.35f);
    if (json_.contains("targetOffset") &&
        json_["targetOffset"].is_array() &&
        json_["targetOffset"].size() >= 3)
    {
        targetOffset =
        {
            json_["targetOffset"][0].get<float>(),
            json_["targetOffset"][1].get<float>(),
            json_["targetOffset"][2].get<float>()
        };
    }
    ResetFollowTracking();
}

void ThirdPersonCameraComponent::ResolveTarget()
{
    if (target.lock() || targetName.empty()) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    const std::shared_ptr<GameObject> resolvedTarget_ =
        owner_->manager->Find(targetName);
    if (resolvedTarget_) SetTarget(resolvedTarget_);
}

void ThirdPersonCameraComponent::ResolveCamera()
{
    if (camera.lock()) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_) camera = owner_->GetComponent<CameraComponent>();
}

void ThirdPersonCameraComponent::ResetFollowTracking()
{
    hasCameraCenter = false;
    smoothedCameraCenter = { 0.0f, 0.0f, 0.0f };
}

void ThirdPersonCameraComponent::SetTarget(std::shared_ptr<GameObject> target_)
{
    target = target_;
    targetName = target_ ? target_->GetName() : "";
    ResetFollowTracking();
}

REGISTER_COMPONENT(ThirdPersonCameraComponent, "ThirdPersonCameraComponent")
