#include "PlayerInput.h"
#include "GameObject.h"
#include "PlayerAttack.h"
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include "Transform.h"

using namespace DirectX;

//--------------------------------------
// 角度差（ラジアン）を -π～+π に正規化
//--------------------------------------
static float DeltaAngleRad(float from, float to)
{
    float diff = to - from;
    while (diff > XM_PI)  diff -= XM_2PI;
    while (diff < -XM_PI) diff += XM_2PI;
    return diff;
}

void PlayerInput::Update(float deltaTime_)
{
    auto go = GetGameObject();
    if (!go) return;

    auto& transform = go->transform;
    if (!transform) return;

    auto attackComp = go->GetComponent<PlayerAttack>();

    // 攻撃ボタンのチェック（KeyConfigに"Attack"が設定されている想定）
    if (KeyConfig::Instance().ActionPressed("Attack"))
    {
        if (attackComp) attackComp->DoAttack();
    }

    // 攻撃中なら、ここから先の移動・回転処理をさせない
    if (attackComp && attackComp->isAttacking)
    {
        return;
    }

    // --- 入力取得 ---
    XMFLOAT3 inputDir{ 0, 0, 0 };
    if (KeyConfig::Instance().ActionPressed("MoveForward")) inputDir.z += 1.0f;
    if (KeyConfig::Instance().ActionPressed("MoveBack"))    inputDir.z -= 1.0f;
    if (KeyConfig::Instance().ActionPressed("MoveRight"))   inputDir.x += 1.0f;
    if (KeyConfig::Instance().ActionPressed("MoveLeft"))    inputDir.x -= 1.0f;

    XMVECTOR inputVec = XMVectorSet(inputDir.x, 0.0f, inputDir.z, 0.0f);

    // 入力がないときは回転も移動もしない
    if (XMVector3LengthSq(inputVec).m128_f32[0] < 0.0001f)
    {
        return;
    }

    inputVec = XMVector3Normalize(inputVec);

    // --- 移動方向（frontPatternに応じたベクトル作成） ---
    XMVECTOR moveVec = XMVectorZero();
    switch (frontPattern)
    {
    case FrontPattern::XPlus:  moveVec = XMVectorSet(inputDir.x, 0, inputDir.z, 0); break;
    case FrontPattern::XMinus: moveVec = XMVectorSet(-inputDir.x, 0, -inputDir.z, 0); break;
    case FrontPattern::ZPlus:  moveVec = XMVectorSet(inputDir.x, 0, inputDir.z, 0); break;
    case FrontPattern::ZMinus: moveVec = XMVectorSet(-inputDir.x, 0, -inputDir.z, 0); break;
    }
    moveVec = XMVector3Normalize(moveVec);

    // --- 移動の適用 ---
    XMFLOAT3 move;
    XMStoreFloat3(&move, moveVec);
    transform->AddMoveOffset(XMFLOAT3(
        move.x * moveSpeed * deltaTime_,
        0.0f,
        move.z * moveSpeed * deltaTime_
    ));

    // --- 回転の適用 ---
    {
        // 目標の角度（ラジアン）
        float targetYawRad = atan2f(move.x, move.z);

        // 現在の角度（degree -> rad）
        float currentYawRad = XMConvertToRadians(transform->localRotation.y);

        // 最短距離の差分計算（自作ヘルパー関数）
        auto DeltaAngleRad = [](float current, float target) -> float {
            float diff = target - current;
            while (diff < -XM_PI) diff += XM_2PI;
            while (diff > XM_PI) diff -= XM_2PI;
            return diff;
            };

        float diffRad = DeltaAngleRad(currentYawRad, targetYawRad);
        float diffDeg = XMConvertToDegrees(diffRad);

        // 1フレームの回転制限
        float maxStepDeg = rotateSpeed * deltaTime_;
        float stepDeg = std::clamp(diffDeg, -maxStepDeg, maxStepDeg);

        // Offsetに加算
        transform->rotateOffset.y += stepDeg;
        transform->isDirty = true;
    }
}


void PlayerInput::Gui()
{
    ImGui::SliderFloat("Move Speed", &moveSpeed, 0.1f, 20.0f);
    ImGui::SliderFloat("Rotate Speed", &rotateSpeed, 10.0f, 360.0f);

    const char* frontOptions[] = { "X+", "X-", "Z+", "Z-" };
    int currentSelection = static_cast<int>(frontPattern);
    if (ImGui::Combo("Front Pattern", &currentSelection, frontOptions, IM_ARRAYSIZE(frontOptions)))
    {
        frontPattern = static_cast<FrontPattern>(currentSelection);
    }
}