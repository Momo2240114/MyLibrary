#include "PrecompiledHeader.h"

#include "PlayerDodgeState.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Combat/JustDodgeReactionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Physics/ColliderComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>

PlayerDodgeState::PlayerDodgeState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

void PlayerDodgeState::Enter()
{
    RegisterJustDodgeActions();

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    dodgeActive = true;
    justDodgeFrameActive = false;
    justDodgeSucceeded = false;
    counterAvailable = false;
    justDodgeFrameTimer = 0.0f;

    ClearJustDodgeBlackboard();

    player_->EnterAction(action);
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

    if (action == PLAYER_ACTION::AIR_DODGE)
    {
        SuspendAirDodgeVerticalMovement();
    }
    else
    {
        if (ColliderComponent* collider_ = player_->GetCollider())
        {
            collider_->velocity.y = 0.0f;
        }
    }
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
    if (action == PLAYER_ACTION::AIR_DODGE)
    {
        SuspendAirDodgeVerticalMovement();
    }
    player_->ProcessActionInput();
}

void PlayerDodgeState::Exit()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    EndJustDodgeFrame();

    dodgeActive = false;

    RestoreAirDodgeVerticalMovement();
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

void PlayerDodgeState::NotifyJustDodgeSucceeded()
{
    BS_LOG_INFO(LOG_CATEGORY::GAME, "[JustDodge] PlayerDodgeState NotifyJustDodgeSucceeded entered.");
    if (!dodgeActive)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] PlayerDodgeState rejected: dodgeActive is false.");
        return;
    }
    PlayerControllerComponent* player_ = GetPlayerController();
    const bool colliderFrameActive_ =
        player_ && player_->IsJustDodgeColliderActive();
    if (!justDodgeFrameActive && !colliderFrameActive_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] PlayerDodgeState rejected: frame flag and collider are inactive.");
        return;
    }
    if (!justDodgeFrameActive && colliderFrameActive_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] PlayerDodgeState recovered: frame flag was false but collider is active.");
        justDodgeFrameActive = true;
    }
    if (justDodgeSucceeded)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] PlayerDodgeState rejected: already succeeded.");
        return;
    }

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

void PlayerDodgeState::SuspendAirDodgeVerticalMovement()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    ColliderComponent* collider_ = player_->GetCollider();
    if (!collider_) return;

    if (CharacterMovementComponent* movement_ = player_->GetMovement())
    {
        movement_->SetVerticalMovementLocked(true);
    }

    if (!airDodgeGravityOverridden)
    {
        previousAirDodgeUseGravity = collider_->useGravity;
        airDodgeGravityOverridden = true;
    }

    collider_->useGravity = false;
    collider_->velocity.y = 0.0f;
}

void PlayerDodgeState::RestoreAirDodgeVerticalMovement()
{
    if (!airDodgeGravityOverridden) return;

    PlayerControllerComponent* player_ = GetPlayerController();
    ColliderComponent* collider_ = player_ ? player_->GetCollider() : nullptr;
    if (player_)
    {
        if (CharacterMovementComponent* movement_ = player_->GetMovement())
        {
            movement_->SetVerticalMovementLocked(false);
        }
    }

    if (collider_)
    {
        collider_->useGravity = previousAirDodgeUseGravity;
        collider_->velocity.y = 0.0f;
    }

    airDodgeGravityOverridden = false;
}

void PlayerDodgeState::StartJustDodgeFrame()
{
    BS_LOG_INFO(LOG_CATEGORY::GAME, "[JustDodge] FrameStart action received.");
    if (!dodgeActive)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[JustDodge] FrameStart rejected: dodgeActive is false.");
        return;
    }

    justDodgeFrameActive = true;
    justDodgeFrameTimer = (std::max)(justDodgeFrameDuration, 0.0f);
    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[JustDodge] Frame active. Activating JustDodgeCollider.");
    ActivateDodgeCollision();
}

void PlayerDodgeState::EndJustDodgeFrame()
{
    BS_LOG_INFO(LOG_CATEGORY::GAME, "[JustDodge] FrameEnd action received.");
    justDodgeFrameActive = false;
    justDodgeFrameTimer = 0.0f;
    DeactivateDodgeCollision();
}

void PlayerDodgeState::HandleJustDodgeSuccess()
{
    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[JustDodge] PlayerDodgeState success. Applying blackboard/reaction.");
    justDodgeSucceeded = true;
    SetCounterAvailable(true);

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        GetComponent<BlackboardComponent>();
    if (blackboard_)
    {
        blackboard_->SetBool("Dodge", "JustDodgeSucceeded", true);

        constexpr const char* categories_[] =
        {
            PlayerBlackboardCategory::PLAYER,
            PlayerBlackboardCategory::ANIM
        };

        for (const char* category_ : categories_)
        {
            blackboard_->SetBool(category_, "JustDodgeSucceeded", true);
            blackboard_->SetTrigger(category_, PlayerBlackboardKey::JUST_DODGE_TRIGGER);
        }
    }

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (owner_)
    {
        const std::shared_ptr<JustDodgeReactionComponent> reaction_ =
            owner_->GetComponent<JustDodgeReactionComponent>();
        if (reaction_) reaction_->ExecuteReaction();
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
