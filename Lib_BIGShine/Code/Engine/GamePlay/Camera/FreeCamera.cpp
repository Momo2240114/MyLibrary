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
    // シングルトンからマウス入力を取得
    auto& mouse = MouseInput::Instance();

    // MouseInput内で計算されているマウスの移動量（差分）を取得
    Vector2 mouseDelta = mouse.GetSubMousePos();

    // 【防酔い修正1】右クリックを押している間だけ視点を動かすようにする
    // ※もし常に動かしたい場合は、このif文の条件を外すか調整するが良い
    if (mouse.RightDown())
    {
        // 【防酔い修正2】deltaTimeを乗算し、PCの性能（FPS）による回転速度の爆発を防ぐ
        // ※マウスの生データ（delta）の仕様によっては、lookSpeedの調整（0.2fから10.0f程度）が必要になるぞ
        currentYaw += mouseDelta.x * lookSpeed;
        currentPitch += mouseDelta.y * lookSpeed; // 上下反転（FPS挙動）にする場合はマイナスにする

        // 首が反転して天変地異が起きぬよう、上下のピッチ回転を -89.9度 から 89.9度の範囲に制限
        currentPitch = std::clamp(currentPitch, -2090.0f, 2090.0f);
    }

    // 計算した角度から「視線方向」を割り出し、Transform に適用する
    if (auto t = GetGameObject()->transform)
    {
        float toRadians_ = 3.14159265f / 180.0f;

        Vector3 newRot_ =
        {
            currentPitch * toRadians_,
            currentYaw * toRadians_,
            0.0f
        };

        t->SetRotation(newRot_);

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

    if (KeyInput::KeyPressed(KeyInput::Key::K_E))
    {
        moveDir.y += t->GetUp().y;
    }
    if (KeyInput::KeyPressed(KeyInput::Key::K_Q))
    {
        moveDir.y -= t->GetUp().y;
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