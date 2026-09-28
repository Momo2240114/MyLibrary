#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <string>

// Projectile系の攻撃Objectを、判定処理から分離して移動させるComponent。
class ProjectileMotionComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 発射方向と速度をまとめて設定する。
    void Configure(
        const DirectX::XMFLOAT3& direction_,
        float speed_,
        float acceleration_,
        float gravity_,
        float maxSpeed_);

private:
    DirectX::XMFLOAT3 NormalizeDirection(const DirectX::XMFLOAT3& direction_) const;
    DirectX::XMFLOAT3 ReadFloat3(
        const nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT3& fallback_) const;

private:
    DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
    float speed = 0.0f;
    float acceleration = 0.0f;
    float gravity = 0.0f;
    float maxSpeed = 0.0f;
};
