#pragma once

#include "ComponentBase.h"

#include <DirectXMath.h>
#include <memory>

class ColliderComponent;

// Characterの移動リクエストを収集し、PhysX互換の速度として適用するComponent。
// PlayerやAIは速度、RootMotion、Impulse、向きだけを予約し、実際の反映はUpdateでまとめて行う。
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
    // 演出への受け渡し時に、前のRootMotion・Impulse・向きの予約を消す。
    void ResetMovementRequests();
    // TimeScale中でも物理速度へ正しく変換するため、元のDeltaTimeを渡す。
    void SetUnscaledDeltaTime(float deltaTime_);
    void AddRootMotionDelta(const DirectX::XMFLOAT3& delta_);
    // 攻撃の中断時に、未反映の元移動とWarp移動だけを破棄し、被弾のImpulseは残す。
    void ClearRootMotionDelta() { rootMotionDelta = { 0.0f, 0.0f, 0.0f }; }
    void AddImpulse(const DirectX::XMFLOAT3& impulse_);
    void ResetVerticalVelocity();
    void SetFacingDirection(const DirectX::XMFLOAT3& direction_, bool immediate_ = true);
    void SetVerticalMovementLocked(bool locked_);

    const DirectX::XMFLOAT3& GetDesiredHorizontalVelocity() const
    {
        return desiredHorizontalVelocity;
    }

    std::shared_ptr<ColliderComponent> ResolveCollider();
private:

    // 予約された向きをTransformへ反映する。
    void ApplyFacing(float deltaTime_);
    // 1Frameだけ有効なRootMotionや縦Impulseを消す。
    void ClearFrameRequests();

private:
    std::weak_ptr<ColliderComponent> collider;
    DirectX::XMFLOAT3 desiredHorizontalVelocity{ 0.0f, 0.0f, 0.0f }; // StateやAIから要求された水平速度。
    DirectX::XMFLOAT3 rootMotionDelta{ 0.0f, 0.0f, 0.0f }; // Animation由来の1Frame移動量。
    DirectX::XMFLOAT3 impulseVelocity{ 0.0f, 0.0f, 0.0f }; // Knockbackなどの減衰する外力。
    DirectX::XMFLOAT3 facingDirection{ 0.0f, 0.0f, 1.0f }; // 次に向ける水平向き。
    DirectX::XMFLOAT3 lastAppliedVelocity{ 0.0f, 0.0f, 0.0f }; // Debug表示用の最終適用速度。
    float facingTurnSpeed = 10.0f;
    float impulseDamping = 6.0f;
    float unscaledDeltaTime = 0.0f;
    float pendingVerticalImpulse = 0.0f;
    bool hasFacingRequest = false;
    bool facingImmediate = true;
    bool usePhysicsVelocity = true;
    bool verticalMovementLocked = false;

};
