#include "PlayerDodgeState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/Physics/ColliderComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>

void PlayerDodgeState::Enter()
{
    RegisterJustDodgeActions();

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    player_->EnterAction(PLAYER_ACTION::DODGE);
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);

    const DirectX::XMFLOAT3 cameraForward_ = player_->GetCameraForwardDirection();
    player_->SetFacingDirection(cameraForward_);

    dodgeDirection = player_->GetMoveInput();
    dodgeBlendInput = player_->GetMoveInputAxis();
    if (dodgeDirection.x == 0.0f && dodgeDirection.z == 0.0f)
    {
        dodgeDirection =
        {
            -cameraForward_.x,
            0.0f,
            -cameraForward_.z
        };

        dodgeBlendInput = { 0.0f, 0.0f, -1.0f };
    }

    SendDodgeInputToBlackboard(dodgeBlendInput);

    if (ColliderComponent* collider_ = player_->GetCollider())
    {
        collider_->velocity.y = 0.0f;
    }

    dodgeActive = true;
    justDodgeFrameActive = false;
    justDodgeSucceeded = false;
    counterAvailable = false;
    justDodgeFrameTimer = 0.0f;

    ClearJustDodgeBlackboard();

}

void PlayerDodgeState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    if (justDodgeFrameTimer > 0.0f)
    {
        justDodgeFrameTimer = (std::max)(
            justDodgeFrameTimer - (std::max)(elapsedTime_, 0.0f),
            0.0f);

        if (justDodgeFrameTimer <= 0.0f)
        {
            EndJustDodgeFrame();
        }
    }

    player_->SendMoveInputToBlackboard(dodgeDirection);
    SendDodgeInputToBlackboard(dodgeBlendInput);
    player_->ProcessActionInput();
}

void PlayerDodgeState::Exit()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    EndJustDodgeFrame();

    dodgeActive = false;

    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

void PlayerDodgeState::NotifyJustDodgeSucceeded()
{
    if (!dodgeActive) return;
    if (!justDodgeFrameActive) return;
    if (justDodgeSucceeded) return;

    HandleJustDodgeSuccess();
}

void PlayerDodgeState::RegisterJustDodgeActions()
{
    if (justDodgeActionsRegistered) return;

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return;

    owner_->RegisterAction("Player", frameStartActionName, [this]()
        {
            StartJustDodgeFrame();
        });

    owner_->RegisterAction("Player", frameEndActionName, [this]()
        {
            EndJustDodgeFrame();
        });

    owner_->RegisterAction("Player", consumeCounterActionName, [this]()
        {
            SetCounterAvailable(false);
        });

    justDodgeActionsRegistered = true;
}

void PlayerDodgeState::ActivateDodgeCollision()
{
    ExecuteOwnerAction("HurtBoxInvincibleOn");
    ExecuteOwnerAction(justDodgeColliderActivateActionName);
}

void PlayerDodgeState::DeactivateDodgeCollision()
{
    ExecuteOwnerAction(justDodgeColliderDeactivateActionName);
    ExecuteOwnerAction("HurtBoxInvincibleOff");
}

void PlayerDodgeState::StartJustDodgeFrame()
{
    if (!dodgeActive) return;

    justDodgeFrameActive = true;
    justDodgeFrameTimer = (std::max)(justDodgeFrameDuration, 0.0f);
    ActivateDodgeCollision();
}

void PlayerDodgeState::EndJustDodgeFrame()
{
    justDodgeFrameActive = false;
    justDodgeFrameTimer = 0.0f;
    DeactivateDodgeCollision();
}

void PlayerDodgeState::HandleJustDodgeSuccess()
{
    justDodgeSucceeded = true;
    SetCounterAvailable(true);
    ApplyJustDodgeSlowMotion();

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (owner_)
    {
        owner_->ExecuteAction("JustDodge_Success");
    }
}

void PlayerDodgeState::ApplyJustDodgeSlowMotion()
{
    if (!useSlowMotion) return;
    if (slowMotionDuration <= 0.0f) return;

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return;

    const std::shared_ptr<TimeManager> timeManager_ =
        owner_->GetManager().GetTimeManager();
    if (!timeManager_) return;

    const uint32_t selfLayerMask_ =
        static_cast<uint32_t>(owner_->GetLayer());
    const uint32_t otherLayerMask_ =
        slowMotionLayerMask & ~selfLayerMask_;
    if (otherLayerMask_ != 0u)
    {
        timeManager_->TriggerHitStop(
            otherLayerMask_,
            slowMotionDuration,
            slowMotionScale);
    }

    if (selfLayerMask_ != 0u)
    {
        timeManager_->TriggerHitStop(
            selfLayerMask_,
            slowMotionDuration * selfSlowMotionDurationRatio,
            slowMotionScale);
    }
}

void PlayerDodgeState::ClearJustDodgeBlackboard()
{
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetBool("Dodge", "JustDodgeSucceeded", false);
    blackboard_->SetBool("Dodge", "CanCounter", false);
    blackboard_->SetString("Dodge", "DodgedAttackTag", "");
    blackboard_->SetString("Dodge", "DodgedSourceAction", "");

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard_->SetBool(category_, "JustDodgeSucceeded", false);
        blackboard_->SetBool(category_, "CanCounter", false);
    }
}

void PlayerDodgeState::SetCounterAvailable(bool enabled_)
{
    counterAvailable = enabled_;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetBool("Dodge", "CanCounter", enabled_);

    constexpr const char* categories_[] =
    {
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardCategory::ANIM
    };

    for (const char* category_ : categories_)
    {
        blackboard_->SetBool(category_, "CanCounter", enabled_);
    }
}

void PlayerDodgeState::ExecuteOwnerAction(const std::string& actionName_)
{
    if (actionName_.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return;

    owner_->ExecuteAction(actionName_);
}

void PlayerDodgeState::SendDodgeInputToBlackboard(
    const DirectX::XMFLOAT3& input_)
{
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetVector2(
        PlayerBlackboardCategory::PLAYER,
        PlayerBlackboardKey::DODGE_INPUT,
        { input_.x, input_.z });
    blackboard_->SetVector2(
        PlayerBlackboardCategory::ANIM,
        PlayerBlackboardKey::DODGE_INPUT,
        { input_.x, input_.z });
}
