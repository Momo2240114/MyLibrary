#include"FollowCameraComponent.h"
#include"CameraComponent.h"
#include"Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Transform/Transform.h"

void FollowCameraComponent::Update(float deltaTime)
{
    auto tgtGO = target.lock();
    if (!tgtGO) return;

    // --- 1. マウスの移動量（Delta）を取得 ---
    // m_SubMousePos が中央からのズレなので、これを回転量として加算する
    Vector2 mouseDelta = MouseInput::Instance().GetSubMousePos();

    // Yaw(横回転)はXの動き、Pitch(縦回転)はYの動き
    cameraAngle.y += mouseDelta.x * cameraYawSpeed;
    cameraAngle.x += mouseDelta.y * cameraPitchSpeed;

    // ピッチ制限（ひっくり返らないように）
    cameraAngle.x = std::clamp(cameraAngle.x, cameraMinPitch, cameraMaxPitch);

    // --- 2. 座標計算 ---
    // 戻り値を一度変数に格納する
    DirectX::XMFLOAT3 worldPos = tgtGO->transform->GetWorldPosition();

    // 変数なのでアドレス (&) が取れるようになる
    DirectX::XMVECTOR targetPos = DirectX::XMLoadFloat3(&worldPos);

    // 注視点を少し上げる（腰ではなく頭のあたりを狙う）
    DirectX::XMVECTOR lookAtPos = DirectX::XMVectorAdd(targetPos, DirectX::XMVectorSet(0, offset.y, 0, 0));

    // 回転行列（Rollは0）
    DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(cameraAngle.x, cameraAngle.y, 0.0f);

    // 後方に Distance 分離れたベクトルを計算
    DirectX::XMVECTOR camOffset = DirectX::XMVectorSet(0, 0, -cameraDistance, 0);
    camOffset = DirectX::XMVector3TransformNormal(camOffset, rotation);
    
    DirectX::XMVECTOR newCamPos = DirectX::XMVectorAdd(lookAtPos, camOffset);

    // --- 3. 反映 ---
    DirectX::XMStoreFloat3(&GetGameObject()->transform->localPosition, newCamPos);

    if (auto c = camera.lock()) {
        DirectX::XMFLOAT3 eye, focus;
        DirectX::XMStoreFloat3(&eye, newCamPos);
        DirectX::XMStoreFloat3(&focus, lookAtPos);
        c->SetLookAt(eye, focus, { 0, 1, 0 });
    }
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