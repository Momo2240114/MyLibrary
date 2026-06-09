#include "FreeCamera.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include <DirectXMath.h>

void CameraFreeController::Start()
{
    if (auto t = GetGameObject()->transform)
    {
        DirectX::XMFLOAT3 rot = t->GetWorldRotation();
        currentYaw = rot.y;
        currentPitch = rot.x;
    }
}

void CameraFreeController::Update(float deltaTime)
{
    // マウス入力関係の参照を取得
    auto& mouse = MouseInput::Instance();

    // 1. マウスによる視点移動（仕様：いつでもマウスで視点を移動、あるいは右クリック中のみ等、お好みに合わせて調整可能）
    // ここでは、お主の提示した仕様「mouseで視点を移動」に基づき、常に回転入力を受け付けます
    UpdateRotation(deltaTime);

    // 2. 右クリック中の処理（仕様：右クリック中にWASDで移動、ホイールで速度変更）
    if (mouse.RightDown())
    {
        // ホイールの回転量（デルタ）を消費して取得
        float wheelDelta = mouse.ConsumeWheelDelta();
        if (wheelDelta != 0.0f)
        {
            // ホイールが奥に回ったら加速、手前に回ったら減速（1クリックあたり通常+120が入るため、小さな係数を掛ける）
            speedMultiplier += (wheelDelta > 0.0f) ? 0.1f : -0.1f;

            // 速度倍率がマイナスになったり、無限に速くならないように制限（0.1倍から10.0倍）
            speedMultiplier = (std::max)(0.1f, (std::min)(10.0f, speedMultiplier));
        }

        // WASDによる移動処理
        UpdateMovement(deltaTime);
    }
    else
    {
        // 右クリックしていない間も、ホイールの回転によるバッファが溜まり続けないように念のため消費しておく
        mouse.ConsumeWheelDelta();
    }
}

void CameraFreeController::UpdateRotation(float deltaTime)
{
    auto& mouse = MouseInput::Instance();

    // MouseInput内で計算されているマウスの移動量（差分）を取得
    Vector2 mouseDelta = mouse.GetSubMousePos();

    // 移動量に感度を乗算して現在の角度に加算（delta.xが横移動、delta.yが縦移動）
    currentYaw += mouseDelta.x * lookSpeed;
    currentPitch += mouseDelta.y * lookSpeed;

    // 首が反転しないように、上下のピッチ回転を -89度 から 89度の範囲に制限
    currentPitch = (std::max)(-89.0f, (std::min)(89.0f, currentPitch));

    // 計算した角度を Transform に適用する
    if (auto t = GetGameObject()->transform)
    {
        Vector3 newRot = { currentPitch, currentYaw, 0.0f };
        t->SetRotation(newRot);
    }
}

void CameraFreeController::UpdateMovement(float deltaTime)
{
    auto t = GetGameObject()->transform;
    if (!t) return;

    // Transformから現在のカメラの「前方向」と「右方向」のベクトルを取得
    Vector3 front = t->GetFront();
    Vector3 right = t->GetRight();

    // 移動方向を記録するベクトル
    DirectX::XMFLOAT3 moveDir{ 0.0f, 0.0f, 0.0f };

    // KeyInputの列挙型を利用してWASDの入力を検知
    // Wキー：前進
    if (KeyInput::KeyPressed(KeyInput::Key::K_W))
    {
        moveDir.x += front.x;
        moveDir.y += front.y;
        moveDir.z += front.z;
    }
    // Sキー：後退
    if (KeyInput::KeyPressed(KeyInput::Key::K_S))
    {
        moveDir.x -= front.x;
        moveDir.y -= front.y;
        moveDir.z -= front.z;
    }
    // Dキー：右スライド移動
    if (KeyInput::KeyPressed(KeyInput::Key::K_D))
    {
        moveDir.x += right.x;
        moveDir.y += right.y;
        moveDir.z += right.z;
    }
    // Aキー：左スライド移動
    if (KeyInput::KeyPressed(KeyInput::Key::K_A))
    {
        moveDir.x -= right.x;
        moveDir.y -= right.y;
        moveDir.z -= right.z;
    }

    // DirectXMathを使用して移動ベクトルの長さを正規化（斜め移動の速度を一定にする）
    DirectX::XMVECTOR vec = DirectX::XMLoadFloat3(&moveDir);
    if (DirectX::XMVector3LengthSq(vec).m128_f32[0] > 0.0f)
    {
        vec = DirectX::XMVector3Normalize(vec);

        // 最終的な移動量を計算（基本速度 × ホイール倍率 × デルタタイム）
        float finalSpeed = moveSpeed * speedMultiplier * deltaTime;
        vec = DirectX::XMVectorScale(vec, finalSpeed);

        // Transformの移動オフセットとして加算
        DirectX::XMFLOAT3 offset;
        DirectX::XMStoreFloat3(&offset, vec);
        t->AddMoveOffset(offset);
    }
}

void CameraFreeController::Gui()
{
    // ImGuiの機能を用いて現在のステートを表示・変更可能にする
    ImGui::Text("Camera Body Free Settings");
    ImGui::DragFloat("Base Speed", &moveSpeed, 0.5f, 0.1f, 100.0f, "%.1f");
    ImGui::DragFloat("Look Sensitivity", &lookSpeed, 0.01f, 0.01f, 2.0f, "%.2f");
    ImGui::SliderFloat("Speed Multiplier (Wheel)", &speedMultiplier, 0.1f, 10.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("Current Yaw  : %.2f", currentYaw);
    ImGui::Text("Current Pitch: %.2f", currentPitch);
}

REGISTER_COMPONENT(CameraFreeController,"FreeCamera")