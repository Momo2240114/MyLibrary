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
    ResolveCollider();
}

void CharacterMovementComponent::Update(float deltaTime_)
{
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

    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (usePhysicsVelocity && collider_)
    {
        collider_->velocity.x =
            scaledHorizontalVelocity_.x + rootMotionVelocity_.x + impulseVelocity.x;
        collider_->velocity.z =
            scaledHorizontalVelocity_.z + rootMotionVelocity_.z + impulseVelocity.z;
        if (std::dynamic_pointer_cast<CharacterCollider>(collider_))
        {
            if (verticalMovementLocked)
            {
                collider_->velocity.y = 0.0f;
                impulseVelocity.y = 0.0f;
            }
            else if (collider_->useGravity)
            {
                collider_->velocity.y += rootMotionVelocity_.y + impulseVelocity.y;
            }
            else
            {
                collider_->velocity.y = rootMotionVelocity_.y + impulseVelocity.y;
            }
        }
        else
        {
            if (verticalMovementLocked)
            {
                collider_->velocity.y = 0.0f;
                impulseVelocity.y = 0.0f;
            }
            else
            {
                collider_->velocity.y += rootMotionVelocity_.y + impulseVelocity.y;
            }
        }
        lastAppliedVelocity = collider_->velocity;
    }
    else
    {
        const DirectX::XMFLOAT3 movementDelta_
        {
            desiredHorizontalVelocity.x * deltaTime_ + rootMotionDelta.x + impulseVelocity.x * deltaTime_,
            verticalMovementLocked ? 0.0f : desiredHorizontalVelocity.y * deltaTime_ + rootMotionDelta.y + impulseVelocity.y * deltaTime_,
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
    impulseVelocity.x *= impulseDecay_;
    impulseVelocity.y *= impulseDecay_;
    impulseVelocity.z *= impulseDecay_;
    const float impulseLengthSquared_ =
        impulseVelocity.x * impulseVelocity.x +
        impulseVelocity.y * impulseVelocity.y +
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
    desiredHorizontalVelocity = { velocity_.x, 0.0f, velocity_.z };
}

void CharacterMovementComponent::StopHorizontalMovement()
{
    desiredHorizontalVelocity = { 0.0f, 0.0f, 0.0f };
}

void CharacterMovementComponent::StopHorizontalMovementImmediately()
{
    StopHorizontalMovement();
    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (!collider_) return;

    collider_->velocity.x = 0.0f;
    collider_->velocity.z = 0.0f;
}

void CharacterMovementComponent::SetUnscaledDeltaTime(float deltaTime_)
{
    unscaledDeltaTime = (std::max)(deltaTime_, 0.0f);
}

void CharacterMovementComponent::AddRootMotionDelta(
    const DirectX::XMFLOAT3& delta_)
{
    rootMotionDelta.x += delta_.x;
    rootMotionDelta.y += delta_.y;
    rootMotionDelta.z += delta_.z;
}

void CharacterMovementComponent::AddImpulse(const DirectX::XMFLOAT3& impulse_)
{
    impulseVelocity.x += impulse_.x;
    impulseVelocity.y += impulse_.y;
    impulseVelocity.z += impulse_.z;
}

void CharacterMovementComponent::ResetVerticalVelocity()
{
    impulseVelocity.y = 0.0f;
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
    const float horizontalLengthSquared_ =
        direction_.x * direction_.x + direction_.z * direction_.z;
    if (horizontalLengthSquared_ <= 0.0001f) return;

    facingDirection = { direction_.x, 0.0f, direction_.z };
    facingImmediate = immediate_;
    hasFacingRequest = true;
}

void CharacterMovementComponent::SetVerticalMovementLocked(bool locked_)
{
    verticalMovementLocked = locked_;
    if (!verticalMovementLocked) return;

    rootMotionDelta.y = 0.0f;
    impulseVelocity.y = 0.0f;

    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (collider_)
    {
        collider_->velocity.y = 0.0f;
    }
}

std::shared_ptr<ColliderComponent> CharacterMovementComponent::ResolveCollider()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

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
    if (!hasFacingRequest) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    owner_->transform->SetForwardYawOnly(
        { facingDirection.x, 0.0f, facingDirection.z },
        facingImmediate ? 1.0f : deltaTime_,
        facingImmediate ? 1.0f : facingTurnSpeed);
    hasFacingRequest = false;
}

void CharacterMovementComponent::ClearFrameRequests()
{
    rootMotionDelta = { 0.0f, 0.0f, 0.0f };
    unscaledDeltaTime = 0.0f;
}

void CharacterMovementComponent::Gui()
{
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
    json_["UsePhysicsVelocity"] = usePhysicsVelocity;
    json_["FacingTurnSpeed"] = facingTurnSpeed;
    json_["ImpulseDamping"] = impulseDamping;
}

void CharacterMovementComponent::Deserialize(const nlohmann::json& json_)
{
    usePhysicsVelocity = json_.value("UsePhysicsVelocity", true);
    facingTurnSpeed = (std::max)(json_.value("FacingTurnSpeed", 10.0f), 0.0f);
    impulseDamping = (std::max)(json_.value("ImpulseDamping", 6.0f), 0.0f);
}

REGISTER_COMPONENT(CharacterMovementComponent, "CharacterMovementComponent")
