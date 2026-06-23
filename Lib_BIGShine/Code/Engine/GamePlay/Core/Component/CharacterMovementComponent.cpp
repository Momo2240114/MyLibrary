#include "CharacterMovementComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
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
    const DirectX::XMFLOAT3 rootMotionVelocity_
    {
        rootMotionDelta.x / safePhysicsDeltaTime_,
        rootMotionDelta.y / safePhysicsDeltaTime_,
        rootMotionDelta.z / safePhysicsDeltaTime_
    };

    const std::shared_ptr<ColliderComponent> collider_ = ResolveCollider();
    if (usePhysicsVelocity && collider_)
    {
        collider_->velocity.x =
            desiredHorizontalVelocity.x + rootMotionVelocity_.x + pendingImpulse.x;
        collider_->velocity.z =
            desiredHorizontalVelocity.z + rootMotionVelocity_.z + pendingImpulse.z;
        collider_->velocity.y += rootMotionVelocity_.y + pendingImpulse.y;
        lastAppliedVelocity = collider_->velocity;
    }
    else
    {
        const DirectX::XMFLOAT3 movementDelta_
        {
            desiredHorizontalVelocity.x * deltaTime_ + rootMotionDelta.x + pendingImpulse.x * deltaTime_,
            desiredHorizontalVelocity.y * deltaTime_ + rootMotionDelta.y + pendingImpulse.y * deltaTime_,
            desiredHorizontalVelocity.z * deltaTime_ + rootMotionDelta.z + pendingImpulse.z * deltaTime_
        };
        owner_->transform->AddMoveOffset(movementDelta_);
        lastAppliedVelocity =
        {
            movementDelta_.x / safeDeltaTime_,
            movementDelta_.y / safeDeltaTime_,
            movementDelta_.z / safeDeltaTime_
        };
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
    pendingImpulse.x += impulse_.x;
    pendingImpulse.y += impulse_.y;
    pendingImpulse.z += impulse_.z;
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

std::shared_ptr<ColliderComponent> CharacterMovementComponent::ResolveCollider()
{
    if (const std::shared_ptr<ColliderComponent> collider_ = collider.lock())
    {
        return collider_;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    const std::shared_ptr<ColliderComponent> collider_ =
        owner_->GetComponent<ColliderComponent>();
    if (collider_)
    {
        collider_->SetVelocityRotationMode(
            ColliderComponent::VelocityRotationMode::NONE);
    }
    collider = collider_;
    return collider_;
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
    pendingImpulse = { 0.0f, 0.0f, 0.0f };
    unscaledDeltaTime = 0.0f;
}

void CharacterMovementComponent::Gui()
{
    ImGui::Checkbox("Use Physics Velocity", &usePhysicsVelocity);
    ImGui::DragFloat("Facing Turn Speed", &facingTurnSpeed, 0.1f, 0.0f, 100.0f);
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
}

void CharacterMovementComponent::Deserialize(const nlohmann::json& json_)
{
    usePhysicsVelocity = json_.value("UsePhysicsVelocity", true);
    facingTurnSpeed = (std::max)(json_.value("FacingTurnSpeed", 10.0f), 0.0f);
}

REGISTER_COMPONENT(CharacterMovementComponent, "CharacterMovementComponent")
