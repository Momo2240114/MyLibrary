#include "PrecompiledHeader.h"
#include "PlayerControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/HurtBox.h"
#include <algorithm>
#include <cmath>
#include <limits>

// 拘束を重複させず、拘束姿勢と復帰動作の再生Stateが登録されているか確認する。
bool PlayerControllerComponent::CanBeginEnemyExecution() const
{
    const auto actor_ = gameObject.lock();
    const auto health_ = actor_ ? actor_->GetComponent<HealthComponent>() : nullptr;
    if (!actor_ || !actor_->transform || actor_->IsPendingDestroy() || !movement ||
        enemyExecutionCaptured || enemyExecutionRecovering || IsPlayerControlLockedByGameMode() || (health_ && health_->IsDead()))
    {
        return false;
    }
    const auto graph_ = AnimationGraphBridgeComponent::Acquire(actor_);
    if (!graph_)
    {
        return false;
    }
    const auto& states_ = graph_->GetStateCatalog();
    return states_.count("Execution_Target_03_Seq") && states_.count("Roll_Combat_F") && states_.count("Get_Up_Seq");
}

// 弾かれたひるみまたは拘束へ入り、開始姿勢へ遷移できなければ一時設定を巻き戻す。
bool PlayerControllerComponent::BeginEnemyExecution(const std::shared_ptr<GameObject>& attacker_, float holdTime_, float forwardDistance_, bool showRecoil_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::BeginEnemyExecution");
    // 同じ敵の予備段階から本拘束へ移る時だけ、一時設定を復元して通常の開始経路へ渡す。
    if (!showRecoil_ && enemyExecutionRecoilActive && enemyExecutionAttacker.lock() == attacker_)
    {
        CancelEnemyExecution(false);
    }
    if (!attacker_ || !attacker_->transform || !CanBeginEnemyExecution())
    {
        return false;
    }
    const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetGameObject());
    if (!graph_ || (showRecoil_ && !graph_->GetStateCatalog().count("Block_Hit_Break_Seq")))
    {
        return false;
    }
    StopSkill();
    StopNormalGuard();
    stateMachine.ChangeState(PLAYER_ACTION::IDLE);
    const auto actor_ = GetGameObject();
    // 武器の判定を閉じ、拘束前の攻撃が固定中に命中し続けることを防ぐ。
    const auto deactivate_ = [&](const auto& self_, const std::shared_ptr<GameObject>& object_) -> void
    {
        if (!object_)
        {
            return;
        }
        for (const auto& hit_ : object_->GetComponents<HitBox>())
        {
            if (hit_)
            {
                hit_->Deactivate();
            }
        }
        if (object_->transform)
        {
            for (const auto& child_ : object_->transform->children)
            {
                if (child_)
                {
                    self_(self_, child_->gameObject.lock());
                }
            }
        }
    };
    deactivate_(deactivate_, actor_);
    // 耐性の元の値と上書きの有無を別々に保存し、拘束終了後の設定を復元できるようにする。
    enemyExecutionPreviousArmorOverride = HasRuntimeArmorOverride();
    enemyExecutionPreviousArmor = GetCurrentArmor();
    SetRuntimeArmor((std::numeric_limits<float>::max)());
    enemyExecutionAttacker = attacker_;
    enemyExecutionCaptured = true;
    enemyExecutionRecoilActive = showRecoil_;
    enemyExecutionElapsed = 0.0f;
    enemyExecutionHoldTime = std::clamp(holdTime_, 0.1f, 10.0f);
    enemyExecutionDistance = std::clamp(forwardDistance_, 0.8f, 4.0f);
    if (blackboard)
    {
        blackboard->SetBool("Anim", "EnemyExecutionActive", true);
    }
    graph_->ClearTrigger("EnemyExecutionAbort");
    if (showRecoil_)
    {
        // カメラや拘束位置はまだ変えず、攻撃を弾かれて腕が上がるモーションを見せる。
        graph_->RequestStateTransition("Block_Hit_Break_Seq", 0.04f, true, 0.0f, true);
    }
    else
    {
        graph_->RequestTrigger("EnemyExecutionStart");
        graph_->RequestEvaluation();
    }
    const auto state_ = graph_->GetActiveState();
    if (!state_ || state_->name != (showRecoil_ ? "Block_Hit_Break_Seq" : "Execution_Target_03_Seq"))
    {
        CancelEnemyExecution();
        return false;
    }
    UpdateEnemyExecution(0.0f);
    return true;
}

// 拘束した相手の要求だけを受け、解除時の移動入力からロール・起き上がりを選ぶ。
void PlayerControllerComponent::ReleaseEnemyExecution(const std::shared_ptr<GameObject>& attacker_)
{
    BS_PROFILE_SCOPE("PlayerControllerComponent::ReleaseEnemyExecution");
    if (!enemyExecutionCaptured || enemyExecutionAttacker.lock() != attacker_)
    {
        return;
    }
    if (enemyExecutionRecoilActive)
    {
        // 本拘束前の中断では、未実行の叩きつけに対応するロール・起き上がりを再生しない。
        CancelEnemyExecution();
        return;
    }
    enemyExecutionCaptured = false;
    enemyExecutionRecovering = true;
    enemyExecutionRecoveryStarted = false;
    enemyExecutionElapsed = 0.0f;
    if (movement)
    {
        movement->ResetMovementRequests();
        movement->SetVerticalMovementLocked(false);
    }
    const auto input_ = GetMoveInput();
    const bool roll_ = input_.x * input_.x + input_.z * input_.z > 0.01f;
    if (roll_ && movement)
    {
        // 前方ロール用の素材を入力方向へ向け、ロックオン方向には引き戻さない。
        movement->SetFacingDirection(input_, true);
        const auto actor_ = GetGameObject();
        if (actor_ && actor_->transform)
        {
            const auto position_ = actor_->transform->GetWorldPosition();
            const auto scale_ = actor_->transform->GetWorldScale();
            actor_->transform->ApplyWorldMatrix(DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) *
                DirectX::XMMatrixRotationY(std::atan2(input_.x, input_.z)) *
                DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z));
            actor_->UpdateTransform();
        }
    }
    enemyExecutionRecoveryState = roll_ ? "Roll_Combat_F" : "Get_Up_Seq";
    if (const auto graph_ = AnimationGraphBridgeComponent::Acquire(GetGameObject()))
    {
        graph_->RequestTrigger(roll_ ? "EnemyExecutionRoll" : "EnemyExecutionGetUp");
        graph_->RequestEvaluation();
    }
}

// 拘束位置と復帰再生を管理し、専用処理中はtrueを返して通常Stateの更新を止める。
bool PlayerControllerComponent::UpdateEnemyExecution(float deltaTime_)
{
    if (!enemyExecutionCaptured && !enemyExecutionRecovering)
    {
        return false;
    }
    BS_PROFILE_SCOPE("PlayerControllerComponent::UpdateEnemyExecution");
    const auto actor_ = GetGameObject();
    const auto attacker_ = enemyExecutionAttacker.lock();
    const auto health_ = actor_ ? actor_->GetComponent<HealthComponent>() : nullptr;
    const auto graph_ = actor_ ? AnimationGraphBridgeComponent::Acquire(actor_) : nullptr;
    if (!actor_ || !actor_->transform || !graph_ || actor_->IsPendingDestroy() || (health_ && health_->IsDead()))
    {
        CancelEnemyExecution(false);
        return false;
    }
    enemyExecutionElapsed += (std::max)(deltaTime_, 0.0f);
    hasPendingHorizontalVelocity = false;
    hasPendingFacingDirection = false;
    ResetStrongAttackInput();
    SendMoveInputToBlackboard({ 0.0f, 0.0f, 0.0f });
    SendGuardInputToBlackboard(false);
    if (enemyExecutionCaptured)
    {
        const auto attackerHealth_ = attacker_ ? attacker_->GetComponent<HealthComponent>() : nullptr;
        if (!attacker_ || !attacker_->transform || attacker_->IsPendingDestroy() ||
            (attackerHealth_ && attackerHealth_->IsDead()) || enemyExecutionElapsed >= enemyExecutionHoldTime)
        {
            if (enemyExecutionRecoilActive)
            {
                CancelEnemyExecution();
            }
            else
            {
                ReleaseEnemyExecution(attacker_);
            }
            return true;
        }
        if (movement)
        {
            movement->ResetMovementRequests();
            movement->SetVerticalMovementLocked(true);
        }
        // ひるみ中はその場で姿勢を見せ、本拘束に入るまでは敵の正面へ固定しない。
        if (enemyExecutionRecoilActive)
        {
            return true;
        }
        // 固定位置は敵の正面から求め、PhysXの外部Transform同期で判定も同じ位置へ戻す。
        const auto center_ = attacker_->transform->GetWorldPosition();
        const auto scale_ = actor_->transform->GetWorldScale();
        const float yaw_ = DirectX::XMConvertToRadians(attacker_->transform->GetWorldRotation().y);
        actor_->transform->ApplyWorldMatrix(DirectX::XMMatrixScaling(scale_.x, scale_.y, scale_.z) * DirectX::XMMatrixRotationY(yaw_ + DirectX::XM_PI) *
            DirectX::XMMatrixTranslation(center_.x + std::sin(yaw_) * enemyExecutionDistance, center_.y,
                center_.z + std::cos(yaw_) * enemyExecutionDistance));
        actor_->UpdateTransform();
    }
    else
    {
        // 復帰Stateへの進入後の離脱を終了とし、遷移しなかった場合も上限時間で操作を返す。
        const auto state_ = graph_->GetActiveState();
        if (state_ && state_->name == enemyExecutionRecoveryState)
        {
            enemyExecutionRecoveryStarted = true;
        }
        if ((enemyExecutionRecoveryStarted && (!state_ || state_->name != enemyExecutionRecoveryState)) ||
            (!enemyExecutionRecoveryStarted && enemyExecutionElapsed > 0.5f) || enemyExecutionElapsed > 10.0f)
        {
            CancelEnemyExecution();
            return true;
        }
    }
    return true;
}

// 正常復帰・死亡・演出ロック・破棄に共通する、移動制限と耐性の解除を行う。
void PlayerControllerComponent::CancelEnemyExecution(bool returnToIdle_)
{
    if (!enemyExecutionCaptured && !enemyExecutionRecovering)
    {
        return;
    }
    BS_PROFILE_SCOPE("PlayerControllerComponent::CancelEnemyExecution");
    enemyExecutionCaptured = false;
    enemyExecutionRecovering = false;
    enemyExecutionRecoilActive = false;
    enemyExecutionRecoveryStarted = false;
    enemyExecutionAttacker.reset();
    if (movement)
    {
        movement->ResetMovementRequests();
        movement->SetVerticalMovementLocked(false);
    }
    if (enemyExecutionPreviousArmorOverride)
    {
        SetRuntimeArmor(enemyExecutionPreviousArmor);
    }
    else
    {
        ResetRuntimeArmor();
    }
    if (blackboard)
    {
        blackboard->SetBool("Anim", "EnemyExecutionActive", false);
    }
    const auto actor_ = GetGameObject();
    const auto graph_ = actor_ && !actor_->IsPendingDestroy() ? AnimationGraphBridgeComponent::Acquire(actor_) : nullptr;
    if (graph_)
    {
        // 中断前の要求を残さず、演出ロック解除後も拘束姿勢へ戻らないようにする。
        graph_->ClearTrigger("EnemyExecutionStart");
        graph_->ClearTrigger("EnemyExecutionRoll");
        graph_->ClearTrigger("EnemyExecutionGetUp");
        graph_->ClearTrigger("EnemyExecutionAbort");
        const auto state_ = graph_->GetActiveState();
        const auto health_ = actor_->GetComponent<HealthComponent>();
        if ((!state_ || state_->name != "SS_Idle") && (!health_ || !health_->IsDead()))
        {
            graph_->RequestTrigger("EnemyExecutionAbort");
        }
    }
    if (returnToIdle_)
    {
        stateMachine.ChangeState(PLAYER_ACTION::IDLE);
        if (graph_)
        {
            graph_->RequestEvaluation();
        }
    }
}

void PlayerControllerComponent::OnDestroy()
{
    if (hurtBox && GetCurrentAction() == PLAYER_ACTION::JUST_DODGE_RUSH)
    {
        hurtBox->SetTemporaryInvincible(false);
    }
    ResetGroundMovement();
    // Scene終了時にも一時的な移動制限と耐性を解除する。
    CancelEnemyExecution(false);
}
