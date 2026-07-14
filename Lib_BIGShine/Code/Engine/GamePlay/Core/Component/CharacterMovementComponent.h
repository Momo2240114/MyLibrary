#pragma once

#include "ComponentBase.h"

#include <DirectXMath.h>
#include <memory>

class ColliderComponent;

// キャラクターの移動リクエストを収集し、PhysX 互換の速度を 適用させるクラス
class CharacterMovementComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "CharacterMovementComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetHorizontalVelocity(const DirectX::XMFLOAT3& velocity_);
    void StopHorizontalMovement();
    void StopHorizontalMovementImmediately();
    void SetUnscaledDeltaTime(float deltaTime_);
    void AddRootMotionDelta(const DirectX::XMFLOAT3& delta_);
    void AddImpulse(const DirectX::XMFLOAT3& impulse_);
    void ResetVerticalVelocity();
    void SetFacingDirection(const DirectX::XMFLOAT3& direction_, bool immediate_ = true);
    void SetVerticalMovementLocked(bool locked_);

    const DirectX::XMFLOAT3& GetDesiredHorizontalVelocity() const
    {
        return desiredHorizontalVelocity;
    }

private:
    std::shared_ptr<ColliderComponent> ResolveCollider();
    void ApplyFacing(float deltaTime_);
    void ClearFrameRequests();

private:
    std::weak_ptr<ColliderComponent> collider;
    DirectX::XMFLOAT3 desiredHorizontalVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rootMotionDelta{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 impulseVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 facingDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 lastAppliedVelocity{ 0.0f, 0.0f, 0.0f };
    float facingTurnSpeed = 10.0f;
    float impulseDamping = 6.0f;
    float unscaledDeltaTime = 0.0f;
    float pendingVerticalImpulse = 0.0f;
    bool hasFacingRequest = false;
    bool facingImmediate = true;
    bool usePhysicsVelocity = true;
    bool verticalMovementLocked = false;

};
