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

    void SetFireGroundMarkRadius(float radius_) { fireGroundMarkRadius = radius_; }

    // 通常の直進弾は維持し、必要な弾だけ待機後の制限付き追尾を設定する。
    void ConfigureHoming(const std::shared_ptr<GameObject>& target_, const std::shared_ptr<GameObject>& attacker_,
        float waitTime_, float turnDegreesPerSecond_, float collisionRadius_);
    // 待機中だけ攻撃者の周囲を傾いた円軌道で周回し、発射地点まで見た目と判定を運ぶ。
    void ConfigureOrbit(float radius_, float height_, float tiltDegrees_, float angularSpeed_, float phaseDegrees_, float yawDegrees_);
private:
    // 発射時刻で周回位置を確定し、その位置から既存の追尾移動へ引き継ぐ。
    void UpdateOrbitPosition(float elapsedTime_);
    bool orbitEnabled = false;
    float orbitRadius = 3.2f;
    float orbitHeight = 3.0f;
    float orbitTilt = 35.0f;
    float orbitAngularSpeed = 180.0f;
    float orbitPhase = 0.0f;
    float orbitYaw = 0.0f;
    bool UpdateHoming(float deltaTime_);
    std::weak_ptr<GameObject> homingTarget;
    std::weak_ptr<GameObject> homingAttacker;
    bool homingEnabled = false;
    float homingWaitTime = 0.0f;
    float homingElapsed = 0.0f;
    float homingTurnSpeed = 25.0f;
    float homingCollisionRadius = 0.65f;
    DirectX::XMFLOAT3 hoverPosition{};
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
    float fireGroundMarkRadius = 0.0f;
};
