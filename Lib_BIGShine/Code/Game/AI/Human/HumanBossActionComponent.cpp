#include "PrecompiledHeader.h"
#include "HumanBossActionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Physics/GuardCollider.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include <cmath>
#include <algorithm>

const std::string& HumanBossActionComponent::GetName() const
{
    static const std::string NAME = "HumanBossActionComponent";
    return NAME;
}

void HumanBossActionComponent::Start()
{
    RegisterActions();
}

void HumanBossActionComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    // Registryより先にComponentが破棄されても、通知から解放済みのthisを参照しない。
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    owner_->RegisterAction("HumanBoss", "ActionEnd", [weak_]()
    {
        if (const auto self_ = weak_.lock()) self_->execution.NotifyPhaseEnd();
    });
    owner_->RegisterAction("HumanBoss", "OnJustGuard", [weak_]()
    {
        if (const auto self_ = weak_.lock())
        {
            if (self_->IsActive() && self_->currentDefense == HumanBossDefense::JUST_GUARD) self_->justGuardSucceeded = true;
        }
    });
    owner_->RegisterAction("HumanBoss", "HumanBossStop", [weak_]()
    {
        if (const auto self_ = weak_.lock()) self_->Stop();
    });
}

bool HumanBossActionComponent::CanStart(const SkillDefinition& skill_, HumanBossDefense defense_) const
{
    const auto owner_ = gameObject.lock();
    if (!owner_ || execution.IsActive()) return false;
    if ((defense_ == HumanBossDefense::GUARD || defense_ == HumanBossDefense::JUST_GUARD) && !owner_->GetComponent<GuardCollider>()) return false;
    if (defense_ == HumanBossDefense::DODGE)
    {
        const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
        if (!owner_->GetComponent<HurtBox>() || !movement_ || !movement_->ResolveCollider()) return false;
    }
    const auto animation_ = owner_->GetComponent<AnimationControllerComponent>();
    if (!animation_ || animation_->IsSkillControlled()) return false;
    std::string error_;
    return SkillExecution::CanExecute(owner_, skill_, error_);
}

bool HumanBossActionComponent::Begin(const SkillDefinition& skill_, HumanBossDefense defense_,
    const DirectX::XMFLOAT3& velocity_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Begin");
    if (!CanStart(skill_, defense_)) { status = "Missing animation, sequence, or defense collider."; return false; }
    currentDefense = defense_;
    justGuardSucceeded = false;
    actionVelocity = velocity_;
    actionOwnsVelocity = defense_ == HumanBossDefense::DODGE;
    const auto owner_ = GetGameObject();
    if (actionOwnsVelocity && owner_->transform)
    {
        // DodgeのBlend入力は注視方向から見たローカル方向へ変換する。
        const float yaw_ = owner_->transform->GetWorldRotation().y * DirectX::XM_PI / 180.0f;
        const float speed_ = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
        {
            const float inverseSpeed_ = speed_ > 0.0001f ? 1.0f / speed_ : 0.0f;
            blackboard_->SetVector2("Anim", "DodgeInput",
                { (std::cos(yaw_) * velocity_.x - std::sin(yaw_) * velocity_.z) * inverseSpeed_,
                  (std::sin(yaw_) * velocity_.x + std::cos(yaw_) * velocity_.z) * inverseSpeed_ });
        }
    }
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovement();
    if (!execution.Start(owner_, skill_))
    {
        status = execution.GetError();
        currentDefense = HumanBossDefense::NONE;
        actionOwnsVelocity = false;
        return false;
    }
    if (!execution.IsActive()) CloseDefense();
    else if (actionOwnsVelocity)
        if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->SetHorizontalVelocity(actionVelocity);
    status.clear();
    return true;
}

void HumanBossActionComponent::Tick(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Tick");
    const bool active_ = execution.IsActive();
    execution.Update(deltaTime_);
    if ((active_ || currentDefense != HumanBossDefense::NONE) && !execution.IsActive()) CloseDefense();
    if (execution.IsActive() && actionOwnsVelocity)
    {
        if (const auto owner_ = GetGameObject())
            if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
                movement_->SetHorizontalVelocity(actionVelocity);
    }
    if (!execution.GetError().empty()) status = execution.GetError();
}

void HumanBossActionComponent::CloseDefense()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    if (currentDefense == HumanBossDefense::GUARD || currentDefense == HumanBossDefense::JUST_GUARD)
    {
        if (const auto guard_ = owner_->GetComponent<GuardCollider>())
        {
            guard_->SetJustGuardEnabled(false);
            guard_->Deactivate();
        }
    }
    if (currentDefense == HumanBossDefense::DODGE)
        if (const auto hurt_ = owner_->GetComponent<HurtBox>()) hurt_->SetInvincible(false);
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovement();
    currentDefense = HumanBossDefense::NONE;
    actionOwnsVelocity = false;
}

void HumanBossActionComponent::Stop(const std::string& idleState_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::Stop");
    const bool wasActive_ = execution.IsActive();
    execution.Stop();
    CloseDefense();
    // 死亡・演出ロックで停止した攻撃のAnimation Eventが、その後も発火しないよう閉じる。
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (wasActive_ && animation_ && !animation_->IsSkillControlled() && animation_->GetStates().count(idleState_))
        animation_->ChangeState(idleState_, 0.1f);
    justGuardSucceeded = false;
}

bool HumanBossActionComponent::ConsumeJustGuardSuccess()
{
    const bool succeeded_ = justGuardSucceeded;
    justGuardSucceeded = false;
    return succeeded_;
}

void HumanBossActionComponent::FaceTarget(const DirectX::XMFLOAT3& direction_)
{
    if (const auto owner_ = GetGameObject())
        if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
            movement_->SetFacingDirection(direction_, false);
}

void HumanBossActionComponent::SetLocomotion(const std::string& state_,
    const DirectX::XMFLOAT3& velocity_, const DirectX::XMFLOAT3& facing_, const DirectX::XMFLOAT2& blendInput_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || IsActive()) return;
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        movement_->SetHorizontalVelocity(velocity_);
        movement_->SetFacingDirection(facing_, false);
    }
    if (const auto blackboard_ = owner_->GetComponent<BlackboardComponent>())
    {
        blackboard_->SetVector2("Anim", "GuardMoveInput", { blendInput_.x, blendInput_.y });
        blackboard_->SetVector2("Anim", "MoveInput", { blendInput_.x, blendInput_.y });
    }
    if (const auto animation_ = owner_->GetComponent<AnimationControllerComponent>())
    {
        if (animation_->IsSkillControlled()) return;
        const auto current_ = animation_->GetCurrentState();
        if ((!current_ || current_->name != state_) && animation_->GetStates().count(state_))
            animation_->ChangeState(state_, 0.15f);
    }
}

void HumanBossActionComponent::Gui()
{
    ImGui::Text("Skill: %s / Phase: %d", execution.GetSkillName().c_str(), execution.GetPhaseIndex());
    ImGui::Text("Result: %d", static_cast<int>(execution.GetResult()));
    ImGui::TextWrapped("%s", status.c_str());
}
