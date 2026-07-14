#include "PlayerAttackState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraLockOnComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Physics/CapsuleCollider.h"
#include "PlayerControllerComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    float GetWorldCapsuleRadius(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_ || !object_->transform) return 0.0f;

        const std::shared_ptr<CapsuleCollider> capsule_ =
            object_->GetComponent<CapsuleCollider>();
        if (!capsule_) return 0.0f;

        const DirectX::XMFLOAT3 scale_ = object_->transform->GetWorldScale();
        const float horizontalScale_ =
            (std::max)(std::abs(scale_.x), std::abs(scale_.z));
        return capsule_->radius * capsule_->shapeSizeScale * horizontalScale_;
    }
}

PlayerAttackState::PlayerAttackState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

void PlayerAttackState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;
    player_->EnterAction(action);
    const bool isStrongCharge_ =
        action == PLAYER_ACTION::STRONG_CHARGE_ATTACK ||
        action == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK;
    const bool isAirAttack_ =
        action == PLAYER_ACTION::AIR_WEAK_ATTACK ||
        action == PLAYER_ACTION::AIR_STRONG_ATTACK ||
        action == PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK;

    if (action == PLAYER_ACTION::AIR_WEAK_ATTACK &&
        player_->GetComboIndex() == 0)
    {
        if (std::shared_ptr<CharacterMovementComponent> movement_ =
            GetComponent<CharacterMovementComponent>())
        {
            movement_->StopHorizontalMovementImmediately();
        }
    }

    if (isStrongCharge_)
    {
        DirectX::XMFLOAT3 direction_ = player_->GetMoveInput();
        if (direction_.x == 0.0f && direction_.z == 0.0f)
        {
            direction_ = player_->GetCameraForwardDirection();
        }
        player_->SetFacingDirection(direction_);
        player_->SetHorizontalVelocity(direction_, player_->strongChargeAttackSpeed);
    }
    else if (!isAirAttack_)
    {
        player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    }
}

void PlayerAttackState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    ApplyAttackAssist(elapsedTime_);
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

void PlayerAttackState::Exit()
{
    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
    }
}

std::shared_ptr<GameObject> PlayerAttackState::FindAssistTarget()
{
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!owner_ || !owner_->transform || !player_) return nullptr;

    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const float maximumDistanceSquared_ = settings_.searchDistance * settings_.searchDistance;

    const auto isValidTarget_ = [&](const std::shared_ptr<GameObject>& target_)
    {
        if (!target_ || target_.get() == owner_.get() || !target_->transform) return false;
        if (target_->layer != LayerType::Enemy) return false;

        const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();
        const float differenceX_ = targetPosition_.x - ownerPosition_.x;
        const float differenceZ_ = targetPosition_.z - ownerPosition_.z;
        return differenceX_ * differenceX_ + differenceZ_ * differenceZ_ <= maximumDistanceSquared_;
    };

    const std::shared_ptr<CameraLockOnComponent> lockOn_ = ResolveCameraLockOnComponent();
    if (lockOn_ && lockOn_->IsLockOnEnabled())
    {
        const std::shared_ptr<GameObject> lockOnTarget_ = lockOn_->GetLockOnTarget();
        if (isValidTarget_(lockOnTarget_)) return lockOnTarget_;
    }

    std::shared_ptr<CameraComponent> camera_;
    if (CameraManager* cameraManager_ = owner_->GetManager().GetCameraManager())
    {
        camera_ = cameraManager_->GetActiveCamera();
    }

    DirectX::XMFLOAT3 cameraForward_{ 0.0f, 0.0f, 0.0f };
    bool hasCameraForward_ = false;
    if (camera_)
    {
        cameraForward_ = camera_->GetFront();
        cameraForward_.y = 0.0f;
        const float cameraLengthSquared_ =
            cameraForward_.x * cameraForward_.x + cameraForward_.z * cameraForward_.z;
        if (cameraLengthSquared_ > 0.000001f)
        {
            const float inverseLength_ = 1.0f / std::sqrt(cameraLengthSquared_);
            cameraForward_.x *= inverseLength_;
            cameraForward_.z *= inverseLength_;
            hasCameraForward_ = true;
        }
    }

    const float centerThreshold_ = std::cos(
        std::clamp(settings_.cameraCenterAngle, 0.0f, 180.0f) *
        DirectX::XM_PI / 180.0f);
    std::shared_ptr<GameObject> centerTarget_;
    std::shared_ptr<GameObject> nearestTarget_;
    float bestCenterDot_ = -1.0f;
    float nearestDistanceSquared_ = (std::numeric_limits<float>::max)();

    for (const std::shared_ptr<GameObject>& candidate_ : owner_->GetManager().GetObjects())
    {
        if (!isValidTarget_(candidate_)) continue;

        const DirectX::XMFLOAT3 candidatePosition_ = candidate_->transform->GetWorldPosition();
        const float differenceX_ = candidatePosition_.x - ownerPosition_.x;
        const float differenceZ_ = candidatePosition_.z - ownerPosition_.z;
        const float distanceSquared_ = differenceX_ * differenceX_ + differenceZ_ * differenceZ_;

        if (distanceSquared_ < nearestDistanceSquared_)
        {
            nearestDistanceSquared_ = distanceSquared_;
            nearestTarget_ = candidate_;
        }

        if (!hasCameraForward_ || distanceSquared_ <= 0.000001f) continue;

        const float inverseDistance_ = 1.0f / std::sqrt(distanceSquared_);
        const float centerDot_ =
            differenceX_ * inverseDistance_ * cameraForward_.x +
            differenceZ_ * inverseDistance_ * cameraForward_.z;
        if (centerDot_ >= centerThreshold_ && centerDot_ > bestCenterDot_)
        {
            bestCenterDot_ = centerDot_;
            centerTarget_ = candidate_;
        }
    }

    return centerTarget_ ? centerTarget_ : nearestTarget_;
}

void PlayerAttackState::ApplyAttackAssist(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform) return;

    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    if (!settings_.enabled || settings_.searchDistance <= 0.0f) return;

    const std::shared_ptr<GameObject> target_ = FindAssistTarget();
    if (!target_ || !target_->transform) return;

    ApplyAttackFacingAssist(target_, elapsedTime_);
    ApplyAttackPositionAssist(target_, elapsedTime_);
}


void PlayerAttackState::ApplyAttackFacingAssist(
    const std::shared_ptr<GameObject>& target_,
    float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform || !target_ || !target_->transform) return;

    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();
    if (settings_.strength <= 0.0f || settings_.turnSpeed <= 0.0f) return;

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();

    DirectX::XMFLOAT3 desiredDirection_
    {
        targetPosition_.x - ownerPosition_.x,
        0.0f,
        targetPosition_.z - ownerPosition_.z
    };

    const float desiredLengthSquared_ =
        desiredDirection_.x * desiredDirection_.x +
        desiredDirection_.z * desiredDirection_.z;

    if (desiredLengthSquared_ <= 0.000001f) return;

    const float desiredInverseLength_ = 1.0f / std::sqrt(desiredLengthSquared_);
    desiredDirection_.x *= desiredInverseLength_;
    desiredDirection_.z *= desiredInverseLength_;

    const Vector3 currentForwardVector_ = owner_->transform->GetFront();

    DirectX::XMFLOAT3 currentDirection_
    {
        currentForwardVector_.x,
        0.0f,
        currentForwardVector_.z
    };

    const float currentLengthSquared_ =
        currentDirection_.x * currentDirection_.x +
        currentDirection_.z * currentDirection_.z;

    if (currentLengthSquared_ > 0.000001f)
    {
        const float currentInverseLength_ = 1.0f / std::sqrt(currentLengthSquared_);
        currentDirection_.x *= currentInverseLength_;
        currentDirection_.z *= currentInverseLength_;
    }
    else
    {
        currentDirection_ = desiredDirection_;
    }

    const float blend_ = std::clamp(
        settings_.turnSpeed * (std::max)(elapsedTime_, 0.0f) * settings_.strength,
        0.0f,
        1.0f);

    DirectX::XMFLOAT3 correctedDirection_
    {
        currentDirection_.x + (desiredDirection_.x - currentDirection_.x) * blend_,
        0.0f,
        currentDirection_.z + (desiredDirection_.z - currentDirection_.z) * blend_
    };

    const float correctedLengthSquared_ =
        correctedDirection_.x * correctedDirection_.x +
        correctedDirection_.z * correctedDirection_.z;

    if (correctedLengthSquared_ <= 0.000001f) return;

    const float correctedInverseLength_ = 1.0f / std::sqrt(correctedLengthSquared_);
    correctedDirection_.x *= correctedInverseLength_;
    correctedDirection_.z *= correctedInverseLength_;

    player_->SetFacingDirection(correctedDirection_);
}

void PlayerAttackState::ApplyAttackPositionAssist(
    const std::shared_ptr<GameObject>& target_,
    float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!player_ || !owner_ || !owner_->transform || !target_ || !target_->transform) return;

    const PlayerAttackAssistSettings& settings_ = player_->GetAttackAssistSettings();

    if (settings_.positionStrength <= 0.0f ||
        settings_.positionSpeed <= 0.0f)
    {
        return;
    }

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 targetPosition_ = target_->transform->GetWorldPosition();

    DirectX::XMFLOAT3 toTarget_
    {
        targetPosition_.x - ownerPosition_.x,
        0.0f,
        targetPosition_.z - ownerPosition_.z
    };

    const float distanceSquared_ =
        toTarget_.x * toTarget_.x +
        toTarget_.z * toTarget_.z;

    if (distanceSquared_ <= 0.000001f) return;

    const float distance_ = std::sqrt(distanceSquared_);
    const float colliderStopDistance_ =
        GetWorldCapsuleRadius(owner_) + GetWorldCapsuleRadius(target_) + 0.05f;
    const float stopDistance_ =
        (std::max)((std::max)(settings_.targetStopDistance, 0.0f), colliderStopDistance_);

    if (distance_ <= stopDistance_)
    {
        return;
    }

    const float inverseDistance_ = 1.0f / distance_;
    toTarget_.x *= inverseDistance_;
    toTarget_.z *= inverseDistance_;

    const float availableDistance_ = distance_ - stopDistance_;
    const float maxCorrectionSpeed_ = (std::max)(settings_.maxPositionCorrection, 0.0f);

    float assistSpeed_ =
        (std::min)(availableDistance_, maxCorrectionSpeed_) *
        settings_.positionSpeed *
        settings_.positionStrength;
    if (elapsedTime_ > 0.000001f)
    {
        assistSpeed_ = (std::min)(assistSpeed_, availableDistance_ / elapsedTime_);
    }

    player_->AddHorizontalVelocityAssist(toTarget_, assistSpeed_);
}
std::shared_ptr<CameraLockOnComponent> PlayerAttackState::ResolveCameraLockOnComponent()
{
    if (const std::shared_ptr<CameraLockOnComponent> cached_ = cameraLockOnComponent.lock())
    {
        return cached_;
    }

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return nullptr;

    for (const std::shared_ptr<GameObject>& object_ : owner_->GetManager().GetObjects())
    {
        if (!object_) continue;
        const std::shared_ptr<CameraLockOnComponent> component_ =
            object_->GetComponent<CameraLockOnComponent>();
        if (!component_) continue;

        cameraLockOnComponent = component_;
        return component_;
    }

    return nullptr;
}
