#include "EditorCameraData.h"

#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"

#include "Engine/GamePlay/Camera/CameraComponent.h"

#include "ImGui.h"

#include <algorithm>
#include <cmath>

void EditorCameraData::ResetPreviewCamera(CameraComponent& camera_)
{
    previewCameraPosition = { 0.0f, 1.5f, -6.0f };
    previewCameraYaw = 0.0f;
    previewCameraPitch = -0.18f;
    previewCameraSpeedMultiplier = 1.0f;

    ApplyToCamera(camera_);
}

void EditorCameraData::ApplyToCamera(CameraComponent& camera_) const
{
    const float cosPitch_ = cosf(previewCameraPitch);

    const DirectX::XMFLOAT3 forward_
    {
        sinf(previewCameraYaw) * cosPitch_,
        sinf(previewCameraPitch),
        cosf(previewCameraYaw) * cosPitch_
    };

    const DirectX::XMFLOAT3 target_
    {
        previewCameraPosition.x + forward_.x,
        previewCameraPosition.y + forward_.y,
        previewCameraPosition.z + forward_.z
    };

    camera_.SetLookAt(
        previewCameraPosition,
        target_,
        { 0.0f, 1.0f, 0.0f });
}

void EditorCameraData::UpdatePreviewFreeCamera(
    CameraComponent& camera_,
    float deltaTime_,
    bool previewHovered_)
{
    if (!enablePreviewFreeCamera) return;
    if (!previewHovered_) return;

    auto& mouse_ = MouseInput::Instance();

    if (!mouse_.RightDown())
    {
        mouse_.ConsumeWheelDelta();
        return;
    }

    const Vector2 mouseDelta_ = mouse_.GetSubMousePos();

    previewCameraYaw += mouseDelta_.x * previewCameraLookSensitivity;
    previewCameraPitch -= mouseDelta_.y * previewCameraLookSensitivity;
    previewCameraPitch = std::clamp(previewCameraPitch, -1.5f, 1.5f);

    const float wheelDelta_ = mouse_.ConsumeWheelDelta();
    if (wheelDelta_ != 0.0f)
    {
        previewCameraSpeedMultiplier += wheelDelta_ > 0.0f ? 0.1f : -0.1f;
        previewCameraSpeedMultiplier = std::clamp(previewCameraSpeedMultiplier, 0.1f, 20.0f);
    }

    const float cosPitch_ = cosf(previewCameraPitch);

    const DirectX::XMFLOAT3 forward_
    {
        sinf(previewCameraYaw) * cosPitch_,
        sinf(previewCameraPitch),
        cosf(previewCameraYaw) * cosPitch_
    };

    const DirectX::XMFLOAT3 right_
    {
        cosf(previewCameraYaw),
        0.0f,
        -sinf(previewCameraYaw)
    };

    DirectX::XMFLOAT3 moveDirection_{ 0.0f, 0.0f, 0.0f };

    if (KeyInput::KeyPressed(KeyInput::Key::K_W))
    {
        moveDirection_.x += forward_.x;
        moveDirection_.y += forward_.y;
        moveDirection_.z += forward_.z;
    }
    if (KeyInput::KeyPressed(KeyInput::Key::K_S))
    {
        moveDirection_.x -= forward_.x;
        moveDirection_.y -= forward_.y;
        moveDirection_.z -= forward_.z;
    }
    if (KeyInput::KeyPressed(KeyInput::Key::K_D))
    {
        moveDirection_.x += right_.x;
        moveDirection_.z += right_.z;
    }
    if (KeyInput::KeyPressed(KeyInput::Key::K_A))
    {
        moveDirection_.x -= right_.x;
        moveDirection_.z -= right_.z;
    }
    if (KeyInput::KeyPressed(KeyInput::Key::K_E))
    {
        moveDirection_.y += 1.0f;
    }
    if (KeyInput::KeyPressed(KeyInput::Key::K_Q))
    {
        moveDirection_.y -= 1.0f;
    }

    DirectX::XMVECTOR moveVector_ = DirectX::XMLoadFloat3(&moveDirection_);
    const float moveLengthSq_ = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(moveVector_));

    if (moveLengthSq_ > 0.0001f)
    {
        moveVector_ = DirectX::XMVector3Normalize(moveVector_);

        const float finalSpeed_ =
            previewCameraMoveSpeed *
            previewCameraSpeedMultiplier *
            deltaTime_;

        moveVector_ = DirectX::XMVectorScale(moveVector_, finalSpeed_);

        DirectX::XMFLOAT3 moveOffset_{};
        DirectX::XMStoreFloat3(&moveOffset_, moveVector_);

        previewCameraPosition.x += moveOffset_.x;
        previewCameraPosition.y += moveOffset_.y;
        previewCameraPosition.z += moveOffset_.z;
    }

    ApplyToCamera(camera_);
}

void EditorCameraData::Gui()
{
    ImGui::Checkbox("Enable Free Camera", &enablePreviewFreeCamera);
    ImGui::DragFloat("Move Speed", &previewCameraMoveSpeed, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Look Sensitivity", &previewCameraLookSensitivity, 0.0001f, 0.0001f, 0.05f, "%.4f");
    ImGui::SliderFloat("Speed Multiplier", &previewCameraSpeedMultiplier, 0.1f, 20.0f);

    ImGui::Separator();

    ImGui::Text(
        "Position: %.2f %.2f %.2f",
        previewCameraPosition.x,
        previewCameraPosition.y,
        previewCameraPosition.z);

    ImGui::Text("Yaw: %.3f", previewCameraYaw);
    ImGui::Text("Pitch: %.3f", previewCameraPitch);
}