#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/Vector.h"


enum class RotationType {
    // 移動方向に合わせた回転を行わない。
    None,
    LookAtVelocity, // 移動方向を向く
};

class Movement : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "Movement"; return name; }

    // velocityに掛ける移動速度。
    float moveSpeed = 5.0f;
    float rotateSpeed = 10.0f; // 補間速度

    void Start() override;
    void Update(float deltaTime) override;

    // 外部AIや入力から、次フレームの移動方向を指定する。
    void SetVelocity(const DirectX::XMFLOAT3& v) { velocity = v; }

    // 移動方向へ向けるかどうかを切り替える。
    void SetRotationType(RotationType type) { rotationType = type; }

private:
    // velocityがある時、Transformの向きを補間して移動方向へ寄せる。
    void ApplyRotation(float deltaTime);

    // 正規化前提の移動方向。UpdateでmoveSpeedとdeltaTimeを掛ける。
    DirectX::XMFLOAT3 velocity{ 0, 0, 0 };
    RotationType rotationType = RotationType::LookAtVelocity;
};
