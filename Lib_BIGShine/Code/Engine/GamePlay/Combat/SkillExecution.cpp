#include "PrecompiledHeader.h"
#include "SkillExecution.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/EventSystem/ActionSequence.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include <algorithm>
#include <cmath>
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include <utility>
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

namespace
{
    constexpr float GROUND_CONTACT_CONFIRM_TIME = 0.05f;
    // 未設定の既存Skillには移動予約を追加せず、既存AIなどの制御を維持する。
    bool HasSkillMovement(const SkillPhase& phase_)
    {
        return phase_.forwardSpeed != 0.0f || !phase_.forwardSpeedCurveName.empty() || phase_.verticalImpulse != 0.0f ||
            phase_.verticalSpeed != 0.0f || phase_.suspendGravity || !phase_.verticalSpeedCurveName.empty() || phase_.warpHeight != 0.0f;
    }
}

SkillExecution::~SkillExecution()
{
    // 所有State/Componentの破棄中は、その所有側を参照するActionを呼ばない。
    phaseStarted = false;
    definition.cleanupAction.clear();
    if (active) Stop();
}

bool SkillExecution::CanExecute(const std::shared_ptr<GameObject>& owner_,
    const SkillDefinition& definition_, std::string& error_)
{
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (!definition_.enabled || definition_.name.empty() || definition_.phases.empty() || !animation_)
    {
        error_ = "Skill is disabled, empty, or has no AnimationController.";
        return false;
    }
    const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
    for (const auto& phase_ : definition_.phases)
    {
        const auto state_ = animation_->GetStates().find(phase_.animationState);
        if (phase_.animationState.empty() || state_ == animation_->GetStates().end() || !state_->second)
        {
            error_ = "Animation State not found: " + phase_.animationState;
            return false;
        }
        if ((HasSkillMovement(phase_) || phase_.endCondition == SkillPhaseEnd::GROUND_CONTACT) &&
            (!movement_ || !movement_->ResolveCollider()))
        {
            error_ = "Skill movement or landing requires CharacterMovement and Collider.";
            return false;
        }
        if (!phase_.sequence.empty() && (!owner_->GetComponent<ActionSequence>() ||
            !owner_->GetLocalRegistry().HasActionInCategory("Sequence", phase_.sequence)))
        {
            error_ = "Owner ActionSequence not found: " + phase_.sequence;
            return false;
        }
    }
    error_.clear();
    return true;
}

bool SkillExecution::Start(const std::shared_ptr<GameObject>& owner_, const SkillDefinition& definition_,
    PhaseCallback phaseCallback_)
{
    BS_PROFILE_SCOPE("SkillExecution::Start");
    if (active || stopping)
    {
        error = "A skill is already running.";
        return false;
    }
    if (!CanExecute(owner_, definition_, error)) return false;
    // Sceneへ保存された表示用BB値ではなく、ランタイムの制御権で多重開始を防ぐ。
    if (owner_->GetComponent<AnimationControllerComponent>()->IsSkillControlled())
    {
        error = "Another skill host is controlling this actor.";
        return false;
    }
    owner_->GetOrAddComponent<BlackboardComponent>();
    owner = owner_;
    definition = definition_;
    phaseCallback = std::move(phaseCallback_);
    ownsMovement = std::any_of(definition.phases.begin(), definition.phases.end(), HasSkillMovement);
    if (ownsMovement)
    {
        const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
        movement = movement_;
        const auto collider_ = movement_->ResolveCollider();
        movementCollider = collider_;
        originalUseGravity = collider_->useGravity;
        originalGravityScale = collider_->gravityScale;
        gravitySuspended = false;
    }
    phaseIndex = 0;
    active = true;
    result = SkillExecutionResult::NONE;
    ++generation;
    owner_->GetComponent<AnimationControllerComponent>()->SetSkillControlled(true);
    SyncBlackboard();
    if (!BeginPhase() && active) Stop(SkillExecutionResult::PHASE_START_FAILED);
    // 開始済みの場合は、時刻0のActionによる即時終了も開始成功として扱う。
    return true;
}

void SkillExecution::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("SkillExecution::Update");
    if (!active || stopping) return;
    const auto owner_ = owner.lock();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto* current_ = animation_ ? animation_->GetCurrentState() : nullptr;
    const auto& phase_ = definition.phases[phaseIndex];
    if (!current_ || current_->name != phase_.animationState)
    {
        error = "Animation State changed outside the skill.";
        Stop(SkillExecutionResult::ANIMATION_CHANGED);
        return;
    }
    phaseTime += (std::max)(deltaTime_, 0.0f);
    bool ended_ = false;
    if (phase_.endCondition == SkillPhaseEnd::ANIMATION_END)
    {
        const float duration_ = animation_->GetCurrentSampleDuration();
        const float animationTime_ = animation_->GetAnimatorTime();
        // ループStateでも最初の一周をPhaseの終了として扱う。
        ended_ = duration_ > 0.0001f && (animationTime_ >= duration_ || animationTime_ < previousAnimationTime);
        previousAnimationTime = animationTime_;
    }
    else if (phase_.endCondition == SkillPhaseEnd::DURATION) ended_ = phaseTime >= phase_.duration;
    else if (phase_.endCondition == SkillPhaseEnd::GROUND_CONTACT)
    {
        const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
        const auto collider_ = movement_ ? movement_->ResolveCollider() : nullptr;
        // Physics更新後の接地が連続して成立するまで、古い一瞬のフラグでEndへ進めない。
        groundedTime = collider_ && collider_->isGrounded
            ? groundedTime + (std::max)(deltaTime_, 0.0f) : 0.0f;
        ended_ = phaseTime >= phase_.duration && groundedTime >= GROUND_CONTACT_CONFIRM_TIME;
    }
    else ended_ = phaseEndRequested;
    // 正常終了が同時に成立した場合は、時間上限よりそちらを優先する。
    if (!ended_ && phaseTime >= phase_.maximumDuration)
    {
        error = "Phase exceeded MaximumDuration.";
        Stop(SkillExecutionResult::TIMEOUT);
        return;
    }
    if (ended_)
    {
        const unsigned int generation_ = generation;
        EndPhase();
        if (!active || generation_ != generation) return;
        ++phaseIndex;
        if (phaseIndex >= static_cast<int>(definition.phases.size()))
        {
            Stop(SkillExecutionResult::COMPLETED);
            return;
        }
        if (!BeginPhase() && active) Stop(SkillExecutionResult::PHASE_START_FAILED);
    }
    if (active)
    {
        UpdateMovement(deltaTime_);
        SyncBlackboard();
    }
}

bool SkillExecution::BeginPhase()
{
    BS_PROFILE_SCOPE("SkillExecution::BeginPhase");
    phaseStarted = false;
    phaseEndRequested = false;
    phaseTime = 0.0f;
    groundedTime = 0.0f;
    previousAnimationTime = 0.0f;
    const auto owner_ = owner.lock();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    const auto& phase_ = definition.phases[phaseIndex];
    if (!animation_ || animation_->GetStates().count(phase_.animationState) == 0 ||
        !animation_->GetStates().at(phase_.animationState))
    {
        error = "Phase Animation State not found: " + phase_.animationState;
        return false;
    }
    const unsigned int generation_ = generation;
    // Warpを先に適用し、移動後の位置でStartの遷移・姿勢初期化を行う。
    BeginPhaseMovement(phase_);
    animation_->ChangeState(phase_.animationState, phase_.blendDuration, true);
    if (!active || generation_ != generation) return false;
    phaseStarted = true;
    const PhaseCallback callback_ = phaseCallback;
    if (callback_) callback_(phase_);
    if (!active || generation_ != generation) return false;
    CallAction(phase_.enterAction);
    if (!active || generation_ != generation) return false;
    if (!phase_.sequence.empty())
    {
        const auto sequence_ = owner_->GetComponent<ActionSequence>();
        const auto action_ = owner_->GetLocalRegistry().GetAction(phase_.sequence);
        if (!sequence_ || !action_) { error = "Phase Sequence not found: " + phase_.sequence; return false; }
        // 時刻0のActionで中断される場合も、開始直後の予約をExitから停止できるようにする。
        sequenceGeneration = sequence_->GetPlaybackGeneration() + 1;
        ownsSequence = true;
        action_();
        if (!active || generation_ != generation) return false;
        ownsSequence = sequenceGeneration == sequence_->GetPlaybackGeneration();
        if (!ownsSequence) { error = "Phase Sequence did not start: " + phase_.sequence; return false; }
    }
    SyncBlackboard();
    return true;
}

void SkillExecution::EndPhase()
{
    BS_PROFILE_SCOPE("SkillExecution::EndPhase");
    if (ownsSequence)
    {
        const auto owner_ = owner.lock();
        const auto sequence_ = owner_ ? owner_->GetComponent<ActionSequence>() : nullptr;
        if (sequence_) sequence_->StopPlayback(sequenceGeneration);
        ownsSequence = false;
    }
    const std::string exitAction_ = phaseStarted && phaseIndex >= 0 && phaseIndex < static_cast<int>(definition.phases.size())
        ? definition.phases[phaseIndex].exitAction : std::string();
    phaseStarted = false;
    CallAction(exitAction_);
}

void SkillExecution::Stop(SkillExecutionResult result_)
{
    BS_PROFILE_SCOPE("SkillExecution::Stop");
    if (!active || stopping) return;
    stopping = true;
    ++generation;
    EndPhase();
    CallAction(definition.cleanupAction);
    const auto owner_ = owner.lock();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    RestoreMovement();
    if (animation_) animation_->SetSkillControlled(false);
    active = false;
    stopping = false;
    result = result_;
    phaseCallback = {};
    SyncBlackboard();
}

void SkillExecution::NotifyPhaseEnd()
{
    // 旧PhaseのStateExitイベントを新Phaseの終了通知として扱わない。
    if (active && !stopping && phaseStarted && phaseIndex >= 0 && phaseIndex < static_cast<int>(definition.phases.size()) &&
        definition.phases[phaseIndex].endCondition == SkillPhaseEnd::ACTION_END) phaseEndRequested = true;
}

void SkillExecution::CallAction(const std::string& name_)
{
    if (name_.empty()) return;
    const auto owner_ = owner.lock();
    if (!owner_) return;
    // Animation Eventと同じく、Owner名付きの登録と素のローカル名の両方を解決する。
    auto action_ = owner_->GetLocalRegistry().GetAction(owner_->BuildActionName(name_));
    if (!action_) action_ = owner_->GetLocalRegistry().GetAction(name_);
    if (!action_) action_ = ActionRegistry::Instance().GetAction(name_);
    if (action_) action_();
}

void SkillExecution::SyncBlackboard()
{
    const auto owner_ = owner.lock();
    const auto blackboard_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!blackboard_) return;
    blackboard_->SetString("Skill", "Name", definition.name);
    blackboard_->SetBool("Skill", "Active", active);
    blackboard_->SetBool("Skill", "Ended", !active);
    blackboard_->SetInt("Skill", "Phase", active ? phaseIndex : -1);
    blackboard_->SetFloat("Skill", "PhaseTime", phaseTime);
    blackboard_->SetInt("Skill", "Result", static_cast<int>(result));
}

// Phaseの切替時だけ重力設定を変更し、踏み切りImpulseは次の有効な更新へ予約する。
void SkillExecution::BeginPhaseMovement(const SkillPhase& phase_)
{
    if (!ownsMovement) return;
    const auto collider_ = movementCollider.lock();
    if (!collider_) return;
    if (gravitySuspended && !phase_.suspendGravity)
    {
        if (const auto movement_ = movement.lock()) movement_->ResetVerticalVelocity();
    }
    gravitySuspended = phase_.suspendGravity;
    // CharacterColliderはUseGravity=falseで縦速度を消すため、重力係数を0にして制御する。
    collider_->useGravity = gravitySuspended ? true : originalUseGravity;
    collider_->gravityScale = gravitySuspended ? 0.0f : originalGravityScale;
    pendingVerticalImpulse = phase_.verticalImpulse;
    if (phase_.warpHeight > 0.0f)
    {
        const auto actor_ = owner.lock();
        if (actor_ && actor_->transform)
        {
            // 既存Colliderの外部Transform変更検出へ渡し、PhysX側も通常Stepで同期する。
            actor_->transform->localPosition.y += phase_.warpHeight;
            actor_->transform->MarkDirty();
            collider_->isGrounded = false;
            if (const auto movement_ = movement.lock()) movement_->ResetVerticalVelocity();
        }
    }
}

// 専用の物理更新を作らず、通常のCharacterMovement更新へ速度を予約する。
void SkillExecution::UpdateMovement(float deltaTime_)
{
    if (!ownsMovement || deltaTime_ <= 0.0f) return;
    BS_PROFILE_SCOPE("SkillExecution::UpdateMovement");
    const auto owner_ = owner.lock();
    const auto movement_ = movement.lock();
    if (!owner_ || !owner_->transform || !movement_) return;
    const auto& phase_ = definition.phases[phaseIndex];
    const auto forward_ = owner_->transform->GetFront();
    const float length_ = std::sqrt(forward_.x * forward_.x + forward_.z * forward_.z);
    float speed_ = phase_.forwardSpeed;
    const auto curve_ = owner_->GetComponent<CurveComponent>();
    if (curve_ && !phase_.forwardSpeedCurveName.empty() && curve_->HasTrack(phase_.forwardSpeedCurveName))
    {
        // 時間指定LoopはPhase秒数、その他はAnimationの進行率で加速・制動を評価する。
        float progress_ = 0.0f;
        if (phase_.endCondition == SkillPhaseEnd::DURATION && phase_.duration > 0.0001f)
            progress_ = phaseTime / phase_.duration;
        else if (const auto animation_ = owner_->GetComponent<AnimationControllerComponent>())
        {
            const float duration_ = animation_->GetCurrentSampleDuration();
            if (duration_ > 0.0001f) progress_ = animation_->GetAnimatorTime() / duration_;
        }
        const float factor_ = curve_->GetValue(phase_.forwardSpeedCurveName, (std::clamp)(progress_, 0.0f, 1.0f));
        if (std::isfinite(factor_)) speed_ *= (std::max)(0.0f, factor_);
    }
    const float scale_ = length_ > 0.0001f ? speed_ / length_ : 0.0f;
    movement_->SetHorizontalVelocity({ forward_.x * scale_, 0.0f, forward_.z * scale_ });
    if (gravitySuspended)
    {
        movement_->ResetVerticalVelocity();
        float verticalSpeed_ = phase_.verticalSpeed;
        if (curve_ && !phase_.verticalSpeedCurveName.empty() && curve_->HasTrack(phase_.verticalSpeedCurveName))
        {
            // Loopの再生時間が折り返しても、溜めが繰り返されないようPhase経過秒を使う。
            const float factor_ = curve_->GetValue(phase_.verticalSpeedCurveName, phaseTime);
            if (std::isfinite(factor_)) verticalSpeed_ *= (std::max)(factor_, 0.0f);
        }
        movement_->AddImpulse({ 0.0f, verticalSpeed_, 0.0f });
    }
    if (pendingVerticalImpulse != 0.0f)
    {
        movement_->ResetVerticalVelocity();
        movement_->AddImpulse({ 0.0f, pendingVerticalImpulse, 0.0f });
        pendingVerticalImpulse = 0.0f;
    }
}

// 通常終了・被弾・キャンセル・破棄のすべてで、移動と重力の一時設定を解除する。
void SkillExecution::RestoreMovement()
{
    if (!ownsMovement) return;
    const auto movement_ = movement.lock();
    if (movement_)
    {
        movement_->StopHorizontalMovementImmediately();
        if (gravitySuspended) movement_->ResetVerticalVelocity();
    }
    if (const auto collider_ = movementCollider.lock())
    {
        collider_->useGravity = originalUseGravity;
        collider_->gravityScale = originalGravityScale;
    }
    ownsMovement = false;
    gravitySuspended = false;
    pendingVerticalImpulse = 0.0f;
}
