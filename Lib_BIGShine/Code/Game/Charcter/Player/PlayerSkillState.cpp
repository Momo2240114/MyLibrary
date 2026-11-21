#include "PrecompiledHeader.h"
#include "PlayerSkillState.h"
#include "PlayerControllerComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/GamePlay/Combat/HitReactionComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Physics/CapsuleCollider.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

PlayerSkillState::PlayerSkillState(const std::shared_ptr<GameObject>& owner_,
    PlayerControllerComponent& playerController_, PLAYER_ACTION action_)
    : PlayerState(owner_, playerController_), action(action_)
{
}

PlayerSkillState::~PlayerSkillState()
{
    StopPhaseMotionWarp();
    // Controllerの破棄中は参照を戻さず、残っているComponentの一時設定だけを解除する。
    for (const auto& snapshot_ : guardSnapshots)
    {
        if (const auto guard_ = snapshot_.collider.lock())
        {
            if (snapshot_.enabled)
            {
                guard_->Activate();
            }
            else
            {
                guard_->Deactivate();
            }
            guard_->SetJustGuardEnabled(snapshot_.justGuard);
        }
    }
    if (armorApplied)
    {
        if (const auto reaction_ = protectedHitReaction.lock())
        {
            reaction_->SetArmor(previousReactionArmor);
        }
    }
}

// 装備定義を検証して共通実行へ渡し、Phaseごとの防御と入力受付をPlayerへ反映する。
void PlayerSkillState::Enter()
{
    BS_PROFILE_SCOPE("PlayerSkillState::Enter");
    auto* player_ = GetPlayerController();
    if (!player_)
    {
        return;
    }
    finishing = false;
    active = false;
    player_->EnterAction(action);
    const auto* definition_ = player_->GetSkillDefinition(action);
    if (!definition_)
    {
        // 空スロットや参照先不足では旧固定Skillへ流さず、通常終了経路へ戻す。
        active = true;
        finishing = true;
        SyncBlackboard();

        return;
    }
    active = true;
    // DebugやBufferによる直接State遷移でも、使用不可のSkillを実行しない。
    if (!player_->CanExecuteSkill(action))
    {
        finishing = true;
        SyncBlackboard();
        return;
    }
    player_->SetHorizontalVelocity({ 0.0f, 0.0f, 0.0f }, 0.0f);
    // 発動時の空中判定を実行用コピーへ反映し、装備定義や着地後の再生Stateを変更しない。
    auto executionDefinition_ = *definition_;
    if (player_->IsAirborneForAction())
    {
        for (auto& phase_ : executionDefinition_.phases)
        {
            if (!phase_.airAnimationState.empty())
            {
                phase_.animationState = phase_.airAnimationState;
            }
        }
    }
    const bool started_ = execution.Start(GetOwner(), executionDefinition_, [this, player_](const SkillPhase& phase_)
    {
        ApplyPhaseDefence(phase_);
        StartPhaseMotionWarp(phase_);
        // Phaseデータは共通とし、Playerの受付窓だけを既存APIへ反映する。
        player_->SetCanBufferInput(phase_.canBufferInput);
        player_->SetCanCombo(false);
        player_->SetCanDodgeCancel(phase_.canDodgeCancel);
        if (!active)
        {
            return;
        }
        player_->SetCanActionBranch(phase_.canActionBranch);
    });
    if (started_ && execution.GetResult() != SkillExecutionResult::PHASE_START_FAILED)
    {
        player_->NotifySkillStarted(action);
    }
    if (!execution.IsActive())
    {
        StopPhaseMotionWarp();
    }
    SyncBlackboard();
}

// 共通実行が終わるまではPhaseを更新し、終了後はControllerに行動全体の終了を通知する。
void PlayerSkillState::Execute(float elapsedTime_)
{
    BS_PROFILE_SCOPE("PlayerSkillState::Execute");
    auto* player_ = GetPlayerController();
    if (!player_)
    {
        return;
    }
    if (active)
    {
        execution.Update(elapsedTime_);
        if (!active)
        {
            return;
        }
        if (!execution.IsActive())
        {
            StopPhaseMotionWarp();
            finishing = true;
            player_->NotifyActionEnd();
            return;
        }
        // Warp未指定の突進は既存の向き補正を使い、指定済みPhaseとの二重補正を避ける。
        const auto* phase_ = execution.GetCurrentPhase();
        if (phase_ && phase_->forwardSpeed != 0.0f && phase_->motionWarpPreset.empty())
        {
            player_->ApplyGuardFacingToTarget();
        }
        SyncBlackboard();
    }
    player_->SendMoveInputToBlackboard(player_->GetMoveInput());
    player_->ProcessActionInput();
}

// 遷移や被弾による中断でも実行を停止し、一時的な防御と公開中の実行情報を解除する。
void PlayerSkillState::Exit()
{
    BS_PROFILE_SCOPE("PlayerSkillState::Exit");
    execution.Stop();
    StopPhaseMotionWarp();
    RestoreDefence();
    active = false;
    finishing = false;
    SyncBlackboard();
}

// アニメーションからの終了通知を、行動全体ではなく現在のPhaseへ渡す。
void PlayerSkillState::NotifyPhaseEnd()
{
    execution.NotifyPhaseEnd();
}

// Stateの実行期間に合わせ、現在のスキル名とPhase番号をBlackboardへ公開する。
void PlayerSkillState::SyncBlackboard()
{
    const auto blackboard_ = GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        return;
    }
    blackboard_->SetBool(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::IS_SKILL_ACTIVE, active);
    blackboard_->SetInt(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::SKILL_PHASE, active ? execution.GetPhaseIndex() : -1);
    blackboard_->SetString(PlayerBlackboardCategory::PLAYER, PlayerBlackboardKey::CURRENT_SKILL,
        active ? execution.GetSkillName() : "");
}

// 中断要求を一度だけ受け付け、終了通知の再入を防ぎながら通常の終了経路へ戻す。
void PlayerSkillState::StopSkill()
{
    BS_PROFILE_SCOPE("PlayerSkillState::StopSkill");
    if (!active || finishing)
    {
        return;
    }
    execution.Stop();
    StopPhaseMotionWarp();
    if (!active)
    {
        return;
    }
    finishing = true;
    if (auto* player_ = GetPlayerController())
    {
        player_->NotifyActionEnd();
    }
}

void PlayerSkillState::StartPhaseMotionWarp(const SkillPhase& phase_)
{
    BS_PROFILE_SCOPE("PlayerSkillState::StartPhaseMotionWarp");
    StopPhaseMotionWarp();
    const auto owner_ = GetOwner();
    const auto player_ = GetPlayerController();
    if (phase_.motionWarpPreset.empty() || !owner_ || !owner_->transform || !player_)
    {
        return;
    }
    constexpr float SKILL_ASSIST_RANGE = 14.0f;
    constexpr float CONTACT_PADDING = 0.08f;
    constexpr float MINIMUM_STOP_DISTANCE = 0.75f;
    const auto target_ = player_->ResolveSkillMotionWarpTarget(SKILL_ASSIST_RANGE);
    if (!target_)
    {
        return;
    }
    const auto capsuleRadius_ = [](const std::shared_ptr<GameObject>& object_)
    {
        const auto capsule_ = object_->GetComponent<CapsuleCollider>();
        if (!capsule_)
        {
            return 0.0f;
        }
        const auto scale_ = object_->transform->GetWorldScale();
        return capsule_->radius * capsule_->shapeSizeScale * (std::max)(std::abs(scale_.x), std::abs(scale_.z));
    };
    const float stopDistance_ = (std::max)(MINIMUM_STOP_DISTANCE,
        capsuleRadius_(owner_) + capsuleRadius_(target_) + CONTACT_PADDING);

    const auto ownerPosition_ = owner_->transform->GetWorldPosition();
    const auto targetPosition_ = target_->transform->GetWorldPosition();
    owner_->transform->SetForwardYawOnly(
        { targetPosition_.x - ownerPosition_.x, 0.0f, targetPosition_.z - ownerPosition_.z }, 1.0f, 1.0f);

    // 突進は速度側で追尾と停止を行い、位置Warpによる踏み込みを重ねない。
    if (phase_.forwardSpeed > 0.0f)
    {
        execution.SetForwardTarget(target_, stopDistance_);
        return;
    }

    const auto animation_ = owner_->GetComponent<AnimationControllerComponent>();
    if (!animation_)
    {
        return;
    }
    const auto warp_ = owner_->GetOrAddComponent<MotionWarpComponent>();
    if (!warp_)
    {
        return;
    }

    MotionWarpPreset preset_;
    preset_.presetName = phase_.motionWarpPreset;
    preset_.positionMode = MOTION_WARP_POSITION_MODE::TARGET_END_POSITION;
    preset_.targetDistance = MINIMUM_STOP_DISTANCE;
    preset_.maximumWarpTargetDistance = SKILL_ASSIST_RANGE + 2.0f;
    preset_.maximumTotalPositionCorrection = SKILL_ASSIST_RANGE;
    preset_.maximumPositionCorrectionPerFrame = 2.0f;
    preset_.maximumPositionCorrectionPerSecond = 72.0f;
    preset_.maximumRotationDegrees = 180.0f;
    preset_.maximumRotationDegreesPerSecond = 1440.0f;
    // 攻撃判定が出る前に寄せるため、踏み込み補正をアニメーション前半へ集中させる。
    constexpr float ASSIST_END_NORMALIZED_TIME = 0.33f;
    preset_.endNormalizedTime = ASSIST_END_NORMALIZED_TIME;
    preset_.rotationFullStrengthEndNormalizedTime = ASSIST_END_NORMALIZED_TIME;
    preset_.rotationEndNormalizedTime = ASSIST_END_NORMALIZED_TIME;
    preset_.lockTargetOnStart = false;
    preset_.lockTargetNormalizedTime = ASSIST_END_NORMALIZED_TIME;
    preset_.maximumTargetTravelDistance = 8.0f;
    preset_.maximumTargetTravelAngleDegrees = 180.0f;
    preset_.warpPosition = phase_.forwardSpeed == 0.0f;
    warp_->AddPresetIfMissing(preset_);
    const auto configuredPreset_ = warp_->GetPreset(phase_.motionWarpPreset);
    if (!configuredPreset_)
    {
        return;
    }
    DirectX::XMFLOAT3 localMotion_{ 0.0f, 0.0f, 0.0f };
    float duration_ = 0.0f;
    animation_->EstimateRootMotion(phase_.animationState, configuredPreset_->startNormalizedTime,
        configuredPreset_->endNormalizedTime, localMotion_, duration_);
    // 先に向きを合わせたので、実際に再生される方向で予定RootMotionを差し引く。
    const auto rotation_ = owner_->transform->localRotation;
    const auto quaternion_ = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
        DirectX::XMConvertToRadians(rotation_.y), DirectX::XMConvertToRadians(rotation_.x), DirectX::XMConvertToRadians(rotation_.z));
    const auto plannedMotion_ = DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3(localMotion_), quaternion_);
    if (warp_->PlayForTarget(phase_.motionWarpPreset, target_, plannedMotion_, stopDistance_,
        configuredPreset_->startNormalizedTime, configuredPreset_->endNormalizedTime))
    {
        motionWarp = warp_;
        motionWarpRequestName = phase_.motionWarpPreset;
    }
}

void PlayerSkillState::StopPhaseMotionWarp()
{
    if (!motionWarpRequestName.empty())
    {
        if (const auto warp_ = motionWarp.lock())
        {
            warp_->Stop(motionWarpRequestName);
        }
    }
    motionWarpRequestName.clear();
    motionWarp.reset();
}

// Player固有の防御をデータで切り替え、スキル名による分岐は作らない。
void PlayerSkillState::ApplyPhaseDefence(const SkillPhase& phase_)
{
    BS_PROFILE_SCOPE("PlayerSkillState::ApplyPhaseDefence");
    auto* player_ = GetPlayerController();
    if (!player_)
    {
        return;
    }
    if (!phase_.enableGuard && guardApplied)
    {
        for (const auto& snapshot_ : guardSnapshots)
        {
            if (const auto guard_ = snapshot_.collider.lock())
            {
                if (snapshot_.enabled)
                {
                    guard_->Activate();
                }
                else
                {
                    guard_->Deactivate();
                }
                guard_->SetJustGuardEnabled(snapshot_.justGuard);
            }
        }
        guardSnapshots.clear();
        guardApplied = false;
    }
    if (phase_.enableGuard && !guardApplied)
    {
        // 個々の判定の有効状態を退避し、終了後に元から有効だったガードまで閉じないようにする。
        std::function<void(const std::shared_ptr<GameObject>&)> collect_;
        collect_ = [&](const std::shared_ptr<GameObject>& object_)
        {
            if (!object_)
            {
                return;
            }
            for (const auto& guard_ : object_->GetComponents<GuardCollider>())
            {
                guardSnapshots.push_back({ guard_, guard_->IsEnabled(), guard_->IsJustGuardEnabled() });
                guard_->SetJustGuardEnabled(false);
                guard_->Activate();
            }
            if (object_->transform)
            {
                for (const auto& child_ : object_->transform->children)
                {
                    if (child_)
                    {
                        collect_(child_->gameObject.lock());
                    }
                }
            }
        };
        collect_(GetOwner());
        guardApplied = true;
    }
    if (phase_.preventFlinch && !armorApplied)
    {
        // 耐性は最初の適用時だけ保存し、連続する防御Phaseで復元元を上書きしない。
        const auto owner_ = GetOwner();
        const auto reaction_ = owner_ ? owner_->GetComponent<HitReactionComponent>() : nullptr;
        protectedHitReaction = reaction_;
        previousReactionArmor = reaction_ ? reaction_->GetArmor() : 0.0f;
        previousArmor = player_->GetCurrentArmor();
        previousArmorOverride = player_->HasRuntimeArmorOverride();
        player_->SetRuntimeArmor((std::numeric_limits<float>::max)());
        armorApplied = true;
    }
    else if (!phase_.preventFlinch && armorApplied)
    {
        if (previousArmorOverride)
        {
            player_->SetRuntimeArmor(previousArmor);
        }
        else
        {
            player_->ResetRuntimeArmor();
        }
        armorApplied = false;
    }
}

// 防御指定のないPhaseを適用し、ガードと耐性を保存していた設定へ戻す。
void PlayerSkillState::RestoreDefence()
{
    SkillPhase phase_;
    ApplyPhaseDefence(phase_);
}

// Controllerの被弾遷移判定へ、実行中Phaseのひるみ防止だけを公開する。
bool PlayerSkillState::PreventsHitReaction() const
{
    const auto* phase_ = execution.GetCurrentPhase();
    return active && phase_ && phase_->preventFlinch;
}
