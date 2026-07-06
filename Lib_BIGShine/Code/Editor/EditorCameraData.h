#pragma once

#include <DirectXMath.h>

class CameraComponent;

//Editorのワークスペースでのみ使用するフリーカメラのデータです
struct EditorCameraData
{
    void UpdatePreviewFreeCamera(CameraComponent& camera_, float deltaTime_, bool previewHovered_);
    void ResetPreviewCamera(CameraComponent& camera_);
    void ApplyToCamera(CameraComponent& camera_) const;
    void Gui();

    DirectX::XMFLOAT3 previewCameraPosition = { 0.0f, 1.5f, -6.0f };
    float previewCameraYaw = 0.0f;
    float previewCameraPitch = -0.18f;
    float previewCameraMoveSpeed = 8.0f;
    float previewCameraLookSensitivity = 0.003f;
    float previewCameraSpeedMultiplier = 1.0f;
    bool enablePreviewFreeCamera = true;
};