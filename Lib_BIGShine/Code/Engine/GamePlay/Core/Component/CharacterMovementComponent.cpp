#include "PrecompiledHeader.h"

#include "CharacterMovementComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Physics/CharacterCollider.h"
#include "Engine/Physics/ColliderComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

void CharacterMovementComponent::Start()
{
    // 初回にColliderを解決し、以降のUpdateで速度を書き込める状態にする。
    ResolveCollider();
}

void CharacterMovementComponent::Update(float deltaTime_)
{
    // 予約された移動、RootMotion、Impulseを1Frame分にまとめてColliderまたはTransformへ反映する。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        ClearFrameRequests();
        return;
    }

    ApplyFacing(deltaTime_);

    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.000001f);
    const float physicsDeltaTime_ = unscaledDeltaTime > 0.0f
        ? unscaledDeltaTime
        : deltaTime_;
    const float safePhysicsDeltaTime_ =
        (std::max)(physicsDeltaTime_, 0.000001f);
    const float timeScale_ = safeDeltaTime_ / safePhysicsDeltaTime_;
    // TimeScale中でも物理側の速度が見た目の移動量と合うように水平速度を補正する。
    const DirectX::XMFLOAT3 scaledHorizontalVelocity_
    {
        desiredHorizontalVelocity.x * timeScale_,
        desiredHorizontalVelocity.y * timeScale_,
        desiredHorizontalVelocity.z * timeScale_
    };
    const DirectX::XMFLOAT3 rootMotionVelocity_
    {
        rootMotionDelta.x / safePhysicsDeltaTime_,
        verticalMovementLocked ? 0.0f : rootMotionDelta.y / safePhysicsDeltaTime_,
        rootMotionDelta.z / safePhysicsDeltaTime_
    };
    const float verticalImpulse_ =
        verticalMovementLocked ? 0.0f : pendingVerticalImpulse;

    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (usePhysicsVelocity && collider_)
    {
        // Physics管理下ではColliderのvelocityへ集約し、CollisionStepSystemに任せる。
        collider_->velocity.x =
            scaledHorizontalVelocity_.x + rootMotionVelocity_.x + impulseVelocity.x;
        collider_->velocity.z =
            scaledHorizontalVelocity_.z + rootMotionVelocity_.z + impulseVelocity.z;
        if (std::dynamic_pointer_cast<CharacterCollider>(collider_))
        {
            if (verticalMovementLocked)
            {
                collider_->velocity.y = 0.0f;
                pendingVerticalImpulse = 0.0f;
            }
            else if (collider_->useGravity)
            {
                collider_->velocity.y += rootMotionVelocity_.y + verticalImpulse_;
            }
            else
            {
                collider_->velocity.y = rootMotionVelocity_.y + verticalImpulse_;
            }
        }
        else
        {
            if (verticalMovementLocked)
            {
                collider_->velocity.y = 0.0f;
                pendingVerticalImpulse = 0.0f;
            }
            else
            {
                collider_->velocity.y += rootMotionVelocity_.y + verticalImpulse_;
            }
        }
        lastAppliedVelocity = collider_->velocity;
    }
    else
    {
        // ColliderがないObjectはTransformへ直接移動量を加算する。
        const DirectX::XMFLOAT3 movementDelta_
        {
            desiredHorizontalVelocity.x * deltaTime_ + rootMotionDelta.x + impulseVelocity.x * deltaTime_,
            verticalMovementLocked ? 0.0f : desiredHorizontalVelocity.y * deltaTime_ + rootMotionDelta.y + verticalImpulse_ * deltaTime_,
            desiredHorizontalVelocity.z * deltaTime_ + rootMotionDelta.z + impulseVelocity.z * deltaTime_
        };
        owner_->transform->AddMoveOffset(movementDelta_);
        lastAppliedVelocity =
        {
            movementDelta_.x / safeDeltaTime_,
            movementDelta_.y / safeDeltaTime_,
            movementDelta_.z / safeDeltaTime_
        };
    }

    const float impulseDecay_ = (std::max)(
        0.0f,
        1.0f - impulseDamping * safePhysicsDeltaTime_);
    // 水平方向Impulseは毎Frame減衰させ、短いKnockbackとして扱う。
    impulseVelocity.x *= impulseDecay_;
    impulseVelocity.z *= impulseDecay_;
    const float impulseLengthSquared_ =
        impulseVelocity.x * impulseVelocity.x +
        impulseVelocity.z * impulseVelocity.z;
    if (impulseLengthSquared_ < 0.0001f)
    {
        impulseVelocity = { 0.0f, 0.0f, 0.0f };
    }

    ClearFrameRequests();
}

void CharacterMovementComponent::SetHorizontalVelocity(
    const DirectX::XMFLOAT3& velocity_)
{
    // 移動Stateから渡された速度は水平成分だけ採用する。
    desiredHorizontalVelocity = { velocity_.x, 0.0f, velocity_.z };
}

void CharacterMovementComponent::StopHorizontalMovement()
{
    desiredHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
}

void CharacterMovementComponent::StopHorizontalMovementImmediately()
{
    // 予約速度だけでなく、現在Colliderへ残っている水平速度も即座に止める。
    StopHorizontalMovement();
    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (!collider_)
    {
        return;
    }

    collider_->velocity.x = 0.0f;
    collider_->velocity.z = 0.0f;
}

void CharacterMovementComponent::SetUnscaledDeltaTime(float deltaTime_)
{
    unscaledDeltaTime = (std::max)(deltaTime_, 0.0f);
}

void CharacterMovementComponent::ResetMovementRequests()
{
    ClearFrameRequests();
    desiredHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
    impulseVelocity = { 0.0f, 0.0f, 0.0f };
    hasFacingRequest = false;
    unscaledDeltaTime = 0.0f;
    if (const auto collider_ = ResolveCollider())
    {
        collider_->velocity = { 0.0f, 0.0f, 0.0f };
    }
}

void CharacterMovementComponent::AddRootMotionDelta(
    const DirectX::XMFLOAT3& delta_)
{
    // Animationから渡されたRootMotionをFrame内で加算しておく。
    rootMotionDelta.x += delta_.x;
    rootMotionDelta.y += delta_.y;
    rootMotionDelta.z += delta_.z;
}

void CharacterMovementComponent::AddImpulse(const DirectX::XMFLOAT3& impulse_)
{
    // 水平Impulseは減衰速度、縦ImpulseはこのFrameの追加速度として扱う。
    impulseVelocity.x += impulse_.x;
    pendingVerticalImpulse += impulse_.y;
    impulseVelocity.z += impulse_.z;
}

void CharacterMovementComponent::ResetVerticalVelocity()
{
    // 着地やAction切り替え時に、縦方向の残留速度を消す。
    pendingVerticalImpulse = 0.0f;
    rootMotionDelta.y = 0.0f;

    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (collider_)
    {
        collider_->velocity.y = 0.0f;
    }
}

void CharacterMovementComponent::SetFacingDirection(
    const DirectX::XMFLOAT3& direction_,
    bool immediate_)
{
    // 高さ方向を無視した向きだけを予約し、ApplyFacingでYaw回転へ反映する。
    const float horizontalLengthSquared_ =
        direction_.x * direction_.x + direction_.z * direction_.z;
    if (horizontalLengthSquared_ <= 0.0001f)
    {
        return;
    }

    facingDirection = { direction_.x, 0.0f, direction_.z };
    facingImmediate = immediate_;
    hasFacingRequest = true;
}

void CharacterMovementComponent::SetVerticalMovementLocked(bool locked_)
{
    // 地上固定Actionなどで縦方向のRootMotion/Impulse/Collider速度を止める。
    verticalMovementLocked = locked_;
    if (!verticalMovementLocked)
    {
        return;
    }

    rootMotionDelta.y = 0.0f;
    pendingVerticalImpulse = 0.0f;

    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (collider_)
    {
        collider_->velocity.y = 0.0f;
    }
}

std::shared_ptr<ColliderComponent> CharacterMovementComponent::ResolveCollider()
{
    // CharacterColliderを優先し、なければ通常ColliderへFallbackする。
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return nullptr;
    }

    if (const std::shared_ptr<ColliderComponent> characterCollider_ =
        owner_->GetComponent<CharacterCollider>())
    {
        characterCollider_->SetVelocityRotationMode(
            ColliderComponent::VelocityRotationMode::NONE);
        collider = characterCollider_;
        return characterCollider_;
    }

    if (const std::shared_ptr<ColliderComponent> cachedCollider_ = collider.lock())
    {
        return cachedCollider_;
    }

    const std::shared_ptr<ColliderComponent> fallbackCollider_ =
        owner_->GetComponent<ColliderComponent>();
    if (fallbackCollider_)
    {
        fallbackCollider_->SetVelocityRotationMode(
            ColliderComponent::VelocityRotationMode::NONE);
    }
    collider = fallbackCollider_;
    return fallbackCollider_;
}

void CharacterMovementComponent::ApplyFacing(float deltaTime_)
{
    // 即時旋回か補間旋回かを選んで、TransformのYawだけを更新する。
    if (!hasFacingRequest)
    {
        return;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        return;
    }

    owner_->transform->SetForwardYawOnly(
        { facingDirection.x, 0.0f, facingDirection.z },
        facingImmediate ? 1.0f : deltaTime_,
        facingImmediate ? 1.0f : facingTurnSpeed);
    hasFacingRequest = false;
}

void CharacterMovementComponent::ClearFrameRequests()
{
    // RootMotionと縦Impulseは1Frame消費型なので、Update終端で必ず消す。
    rootMotionDelta = { 0.0f, 0.0f, 0.0f };
    unscaledDeltaTime = 0.0f;
    pendingVerticalImpulse = 0.0f;
}

void CharacterMovementComponent::Gui()
{
    // 移動反映方式と現在の速度状態を確認するDebug UI。
    ImGui::Checkbox("Use Physics Velocity", &usePhysicsVelocity);
    ImGui::Checkbox("Vertical Movement Locked", &verticalMovementLocked);
    ImGui::DragFloat("Facing Turn Speed", &facingTurnSpeed, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Impulse Damping", &impulseDamping, 0.1f, 0.0f, 100.0f);
    ImGui::Text(
        "Desired Horizontal: %.3f, %.3f",
        desiredHorizontalVelocity.x,
        desiredHorizontalVelocity.z);
    ImGui::Text(
        "Last Applied Velocity: %.3f, %.3f, %.3f",
        lastAppliedVelocity.x,
        lastAppliedVelocity.y,
        lastAppliedVelocity.z);
}

void CharacterMovementComponent::Serialize(nlohmann::json& json_) const
{
    // Scene保存対象は調整値だけにし、Frame中の速度予約は保存しない。
    json_["UsePhysicsVelocity"] = usePhysicsVelocity;
    json_["FacingTurnSpeed"] = facingTurnSpeed;
    json_["ImpulseDamping"] = impulseDamping;
}

void CharacterMovementComponent::Deserialize(const nlohmann::json& json_)
{
    // 保存値を復元し、速度や減衰係数が負にならないように丸める。
    usePhysicsVelocity = json_.value("UsePhysicsVelocity", true);
    facingTurnSpeed = (std::max)(json_.value("FacingTurnSpeed", 10.0f), 0.0f);
    impulseDamping = (std::max)(json_.value("ImpulseDamping", 6.0f), 0.0f);
}

REGISTER_COMPONENT(CharacterMovementComponent, "CharacterMovementComponent")
