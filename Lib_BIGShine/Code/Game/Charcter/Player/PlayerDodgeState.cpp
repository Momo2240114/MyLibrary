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

// 地上回避または空中回避のどちらとして動くかを受け取ってStateを生成する。
PlayerDodgeState::PlayerDodgeState(
    const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_,
    PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_),
    action(action_)
{
}

// 回避開始時に方向、Blackboard、ジャスト回避受付状態を初期化する。
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
        // 入力なし回避はCamera後方へ出すことで、ロックオンのない場面でも後退回避になる。
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
        // 空中回避はAnimation中の落下を止め、軌道を安定させる。
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

// ジャスト回避受付の残り時間を進め、入力情報をAnimationへ送り続ける。
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
            // Timelineから終了通知が来ない場合でも、保険時間で受付を閉じる。
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

// 回避終了時に受付フレーム、空中重力変更、水平速度を元へ戻す。
void PlayerDodgeState::Exit()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    EndJustDodgeFrame();

    dodgeActive = false;

    RestoreAirDodgeVerticalMovement();
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
}

// JustDodgeCollider側から成功通知を受け、回避中かつ受付中なら成功処理へ進める。
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

// 回避Animationから呼ばれるジャスト回避受付ActionをOwnerへ登録する。
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

// HurtBox無敵とJustDodgeColliderを同時に有効化する。
void PlayerDodgeState::ActivateDodgeCollision()
{
    ExecuteOwnerAction("HurtBoxInvincibleOn");
    ExecuteOwnerAction(justDodgeColliderActivateActionName);
}

// JustDodgeColliderを閉じてからHurtBox無敵を解除する。
void PlayerDodgeState::DeactivateDodgeCollision()
{
    ExecuteOwnerAction(justDodgeColliderDeactivateActionName);
    ExecuteOwnerAction("HurtBoxInvincibleOff");
}

// 空中回避中に重力で高度が落ちないよう、垂直移動を一時的に止める。
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

// 空中回避前の重力設定を復元し、垂直速度をリセットする。
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

// Timelineから受付開始を受け取り、JustDodgeColliderを開く。
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

// 受付終了時にColliderと無敵を閉じる。
void PlayerDodgeState::EndJustDodgeFrame()
{
    BS_LOG_INFO(LOG_CATEGORY::GAME, "[JustDodge] FrameEnd action received.");
    justDodgeFrameActive = false;
    justDodgeFrameTimer = 0.0f;
    DeactivateDodgeCollision();
}

// ジャスト回避成功をBlackboardとReactionComponentへ伝える。
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

// 新しい回避へ入る前に、前回の成功状態とカウンター情報を消す。
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

// カウンター可能状態をDodge専用カテゴリとPlayer/Animカテゴリへ反映する。
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

// 文字列Action名をOwnerのActionRegistryへ委譲して実行する。
void PlayerDodgeState::ExecuteOwnerAction(const std::string& actionName_)
{
    if (actionName_.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetOwner();
    if (!owner_) return;

    owner_->ExecuteAction(actionName_);
}

// 回避方向をAnimationBlendで参照できるようBlackboardへ書き込む。
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
