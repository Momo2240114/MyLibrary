#include"FollowCameraComponent.h"
#include"CameraComponent.h"
#include"GameObject.h"
#include "MouseInput.h"
#include "Transform.h"

void FollowCameraComponent::Update(float deltaTime)
{
    auto camGO = GetGameObject();
    if (!camGO) return;
    auto camTransform = camGO->transform;
    if (!camTransform) return;

    auto tgtGO = target.lock();
    if (!tgtGO) return;
    auto tgtTransform = tgtGO->transform;
    if (!tgtTransform) return;

    // カメラの注視点 = プレイヤーの位置
    DirectX::XMFLOAT3 targetPos = tgtTransform->GetWorldPosition();

    // カメラの回転を計算
    DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(cameraAngle.x, cameraAngle.y, 0.0f);

    // 注視点からの相対的なオフセットを回転させる
    DirectX::XMVECTOR offsetVec = DirectX::XMLoadFloat3(&offset);
    offsetVec = DirectX::XMVector3Transform(offsetVec, rotationMatrix);

    // カメラの新しい位置を計算
    DirectX::XMVECTOR newCamPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&targetPos), offsetVec);

    // カメラの新しい位置を適用
    DirectX::XMStoreFloat3(&camTransform->localPosition, newCamPos);

    // カメラがターゲットを注視
    DirectX::XMFLOAT3 eye = camTransform->localPosition;
    DirectX::XMFLOAT3 focus = targetPos;
    DirectX::XMFLOAT3 up{ 0, 1, 0 };
    camera.lock()->SetLookAt(eye, focus, up);
}


void FollowCameraComponent::Gui()
{
    bool changed = false;

    // === Camera Settings ===
    ImGui::Text("Camera Settings");

    // ピッチの制限設定（上下回転）
    changed |= ImGui::SliderFloat("Pitch (Up/Down)", &cameraAngle.x, cameraMinPitch, cameraMaxPitch, "%.2f");
    // ヨーの制限設定（左右回転）
    changed |= ImGui::SliderFloat("Yaw (Left/Right)", &cameraAngle.y, -DirectX::XM_PI, DirectX::XM_PI, "%.2f");

    // カメラの距離設定（ターゲットからの距離）
    changed |= ImGui::SliderFloat("Camera Distance", &cameraDistance, 1.0f, 50.0f, "%.2f");

    // ピッチ速度、ヨー速度（カメラの回転速度）
    changed |= ImGui::SliderFloat("Pitch Speed", &cameraPitchSpeed, 0.001f, 0.05f, "%.4f");
    changed |= ImGui::SliderFloat("Yaw Speed", &cameraYawSpeed, 0.001f, 0.05f, "%.4f");

    // カメラオフセット設定（ターゲットからの位置）
    changed |= ImGui::SliderFloat("Camera Height Offset", &offset.y, 0.0f, 10.0f, "%.2f");

    if (changed)
    {
        // 必要であれば、カメラの設定を更新する処理
    }

    ImGui::Separator();

    // === Debug info ===
    if (auto camGO = GetGameObject())
    {
        auto camTransform = camGO->transform;
        if (camTransform)
        {
            ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", camTransform->localPosition.x, camTransform->localPosition.y, camTransform->localPosition.z);
        }
    }

    if (auto tgtGO = target.lock())
    {
        auto tgtTransform = tgtGO->transform;
        if (tgtTransform)
        {
            ImGui::Text("Camera Target: (%.2f, %.2f, %.2f)", tgtTransform->GetWorldPosition().x, tgtTransform->GetWorldPosition().y, tgtTransform->GetWorldPosition().z);
        }
    }

    // カメラの回転状態
    ImGui::Text("Camera Angle (Pitch, Yaw): (%.2f, %.2f)", cameraAngle.x, cameraAngle.y);
    ImGui::Text("Camera Distance: %.2f", cameraDistance);
}