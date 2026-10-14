#include "PrecompiledHeader.h"
#include "HumanBossActionComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Physics/PhysXSystem.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Combat/AttackTemplateManager.h"
#include "Engine/GamePlay/Combat/ProjectileMotionComponent.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/VFX/DangerAreaComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>

namespace
{
    // 新行動だけを区別し、既存の影渡り派生へ専用処理を混ぜない。
    bool IsRangedSkill(const std::string& name_)
    {
        return name_ == "SHADOW_CHARGE" || name_ == "SHADOW_ORB" || name_ == "SHADOW_RAGNAROK";
    }
}

void HumanBossActionComponent::OnDestroy()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::OnDestroy");
    Stop();
    shadowOrbs.clear();
}

bool HumanBossActionComponent::PlanCharge(const std::shared_ptr<GameObject>& target_, DirectX::XMFLOAT3& end_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::PlanCharge");
    const auto owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform || !target_ || !target_->transform) return false;
    const auto start_ = owner_->transform->GetWorldPosition();
    const auto position_ = target_->transform->GetWorldPosition();
    const float dx_ = position_.x - start_.x;
    const float dz_ = position_.z - start_.z;
    const float distance_ = std::sqrt(dx_ * dx_ + dz_ * dz_);
    const float length_ = (std::min)(chargeMaximumDistance, (std::max)(0.0f, distance_ - 2.0f));
    if (distance_ < 0.001f || length_ < 1.0f) return false;
    end_ = { start_.x + dx_ / distance_ * length_, start_.y, start_.z + dz_ / distance_ * length_ };
    const float edgeX_ = end_.x - rangedStageCenter.x;
    const float edgeZ_ = end_.z - rangedStageCenter.z;
    const float safeRadius_ = (std::max)(1.0f, rangedStageRadius - chargeWidth * 0.5f - 1.0f);
    // 終点を別方向へ曲げず、境界内に収まる直線だけを候補にする。
    if (edgeX_ * edgeX_ + edgeZ_ * edgeZ_ > safeRadius_ * safeRadius_) return false;
    return CanWarpPath(start_, end_);
}

bool HumanBossActionComponent::CanUseRagnarok() const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::CanUseRagnarok");
    const auto owner_ = gameObject.lock();
    const auto health_ = owner_ ? owner_->GetComponent<HealthComponent>() : nullptr;
    const auto reject_ = [this](const char* reason_)
    { ragnarokBlockReason = reason_; return false; };
    if (!ragnarokEnabled) return reject_("Ragnarok disabled");
    if (!owner_ || !owner_->transform || !health_) return reject_("Owner, Transform or Health missing");
    if (health_->IsDead() || health_->GetMaxHealth() <= 0.0f) return reject_("Boss dead or invalid maximum HP");
    if (health_->GetCurrentHealth() / health_->GetMaxHealth() > ragnarokHpThreshold) return reject_("HP above Ragnarok threshold");
    // 解禁と中央までの全経路を分け、遠方やPlayerの一時的な遮断で候補を失わない。
    const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
    if (!movement_ || !movement_->ResolveCollider()) return reject_("Character movement or collider missing");
    DirectX::XMFLOAT3 ground_{};
    if (!ResolveRangedGround(rangedStageCenter, ground_)) return reject_("Ground at stage center unavailable");
    ragnarokBlockReason.clear();
    return true;
}

bool HumanBossActionComponent::CanStartRanged(const std::string& skillName_, const std::shared_ptr<GameObject>& target_) const
{
    if (!IsRangedSkill(skillName_)) return true;
    BS_PROFILE_SCOPE("HumanBossActionComponent::CanStartRanged");
    const auto owner_ = gameObject.lock();
    if (!owner_ || !target_ || !target_->transform) return false;
    auto& templates_ = AttackTemplateManager::Instance();
    if (templates_.GetTemplates().empty()) templates_.LoadAll();
    const std::string templateName_ = skillName_ == "SHADOW_CHARGE" ? "HumanBossShadowCharge" :
        skillName_ == "SHADOW_ORB" ? "HumanBossShadowOrb" : "HumanBossShadowRagnarok";
    if (!templates_.FindTemplate(templateName_)) { validationError = "Missing ranged AttackTemplate: " + templateName_; return false; }
    if (skillName_ == "SHADOW_CHARGE")
    {
        DirectX::XMFLOAT3 end_{};
        if (!attackAssistEnabled || !PlanCharge(target_, end_)) { validationError = "Charge path blocked, outside stage, or MotionWarp disabled."; return false; }
    }
    if (skillName_ == "SHADOW_ORB")
    {
        int count_ = 0;
        for (const auto& weak_ : shadowOrbs)
            if (const auto orb_ = weak_.lock(); orb_ && !orb_->IsPendingDestroy()) ++count_;
        if (count_ >= orbMaximumCount) { validationError = "Shadow Orb concurrent limit reached."; return false; }
    }
    if (skillName_ == "SHADOW_RAGNAROK" && !CanUseRagnarok())
    { validationError = ragnarokBlockReason.empty() ? "Attack assist disabled" : ragnarokBlockReason; return false; }
    return true;
}

SkillDefinition HumanBossActionComponent::PrepareRangedSkill(const SkillDefinition& skill_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::PrepareRangedSkill");
    auto result_ = skill_;
    if (skill_.name == "SHADOW_CHARGE" && !result_.phases.empty())
    {
        result_.phases[0].duration = chargeGrowthTime + 0.2f;
        result_.phases[0].endCondition = SkillPhaseEnd::ACTION_END;
        result_.phases[0].maximumDuration = chargeGrowthTime + 3.0f;
    }
    if (skill_.name == "SHADOW_RAGNAROK" && skill_.phases.size() >= 4)
    {
        result_.phases.clear();
        const auto owner_ = gameObject.lock();
        const auto position_ = owner_ && owner_->transform ? owner_->transform->GetWorldPosition() : rangedStageCenter;
        const float dx_ = position_.x - rangedStageCenter.x;
        const float dz_ = position_.z - rangedStageCenter.z;
        // 到達済みなら移動クリップを省き、RootMotionで中央から離れる経路を作らない。
        if (dx_ * dx_ + dz_ * dz_ > 1.5f * 1.5f)
        {
            auto advance_ = skill_.phases[0];
            advance_.endCondition = SkillPhaseEnd::ACTION_END;
            advance_.maximumDuration = std::sqrt(dx_ * dx_ + dz_ * dz_) / ragnarokAdvanceSpeed + 3.0f;
            advance_.forwardSpeed = 0.0f; // 中央方向の速度は実行担当だけが予約する。
            result_.phases.push_back(advance_);
        }
        for (int round_ = 0; round_ < ragnarokAttackCount; ++round_)
        {
            auto tell_ = skill_.phases[1];
            tell_.duration = (std::max)(tell_.duration, ragnarokRotationTime + 0.5f);
            result_.phases.push_back(tell_);
            auto strike_ = skill_.phases[2];
            strike_.duration = ragnarokHitDuration;
            result_.phases.push_back(strike_);
        }
        result_.phases.push_back(skill_.phases.back());
    }
    return result_;
}

bool HumanBossActionComponent::StartFixedWarp(const SkillPhase& phase_, const DirectX::XMFLOAT3& end_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::StartFixedWarp");
    const auto owner_ = GetGameObject();
    const auto animation_ = owner_ ? AnimationGraphBridgeComponent::Acquire(owner_) : nullptr;
    const auto warp_ = owner_ ? owner_->GetComponent<MotionWarpComponent>() : nullptr;
    if (!owner_ || !owner_->transform || !animation_ || !warp_) return false;
    const auto start_ = owner_->transform->GetWorldPosition();
    FaceTarget({ end_.x - start_.x, 0.0f, end_.z - start_.z }, true);
    DirectX::XMFLOAT3 local_{};
    float duration_ = 0.0f;
    if (!animation_->EstimateRootMotion(phase_.animationState, 0.0f, 0.75f, local_, duration_)) return false;
    const float yaw_ = std::atan2(end_.x - start_.x, end_.z - start_.z);
    DirectX::XMFLOAT3 planned_{};
    DirectX::XMStoreFloat3(&planned_, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&local_), DirectX::XMMatrixRotationY(yaw_)));
    planned_.y = 0.0f;
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    return warp_->PlayForTarget("HumanBoss.ShadowCharge", attackTarget.lock(), planned_, 0.0f, 0.0f, 0.75f, &end_,
        [weak_](const DirectX::XMFLOAT3& from_, const DirectX::XMFLOAT3& to_)
        { const auto self_ = weak_.lock(); return self_ && self_->CanWarpPath(from_, to_); });
}

void HumanBossActionComponent::BeginRangedPhase(const SkillPhase& phase_)
{
    if (!IsRangedSkill(rangedSkill)) return;
    BS_PROFILE_SCOPE("HumanBossActionComponent::BeginRangedPhase");
    if (phase_.animationState == "Run_Attack_01_Seq_0")
    {
        // 予兆生成・伸長が完了していない状態では突進へ進ませない。
        if (!chargeWarningReady) { status = "Charge warning did not complete"; Stop(); return; }
        if (!StartFixedWarp(phase_, chargeEnd)) { status = "Charge Warp failed"; Stop(); return; }
        rangedProgress = "SHADOW_CHARGE: running";
        SetWeaponTrails(true, true);
        shadowEffectTimer = shadowEffectInterval;
        if (!rangedAttackSoundPath.empty()) AudioManager::Instance().PlaySEByPath(rangedAttackSoundPath);
        SpawnShadowEffect();
    }
    if (phase_.animationState == "HumanBossRagnarokAdvance")
    {
        ragnarokRound = 0;
        DirectX::XMFLOAT3 ground_{};
        if (!ResolveRangedGround(rangedStageCenter, ground_))
        { ragnarokBlockReason = "Center ground unavailable"; status = ragnarokBlockReason; Stop(); return; }
        rangedProgress = "SHADOW_RAGNAROK: approaching stage center";
        if (const auto owner_ = GetGameObject())
            if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>()) warp_->Stop();
    }
}

void HumanBossActionComponent::RegisterRangedActions()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::RegisterRangedActions");
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    owner_->RegisterAction("HumanBoss", "HumanBoss_ChargeShowArea", [weak_]() { if (const auto self_ = weak_.lock()) self_->ShowChargeArea(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_OrbSpawn", [weak_]() { if (const auto self_ = weak_.lock()) self_->SpawnShadowOrbs(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_RagnarokShowArea", [weak_]() { if (const auto self_ = weak_.lock()) self_->ShowRagnarokArea(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_RagnarokStrike", [weak_]() { if (const auto self_ = weak_.lock()) self_->StrikeRagnarokArea(); });
    owner_->RegisterAction("HumanBoss", "HumanBoss_RangedClear", [weak_]() { if (const auto self_ = weak_.lock()) self_->ClearRangedArea(); });
}

bool HumanBossActionComponent::ResolveRangedGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ResolveRangedGround");
    const auto owner_ = gameObject.lock();
    const auto movement_ = owner_ ? owner_->GetComponent<CharacterMovementComponent>() : nullptr;
    const auto collider_ = movement_ ? movement_->ResolveCollider() : nullptr;
    auto scene_ = PhysXSystem::GetScene();
    if (!owner_ || !collider_ || !scene_) return false;
    physx::PxRaycastHit hits_[32];
    physx::PxRaycastBuffer hit_(hits_, 32);
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC |
        physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::eNO_BLOCK;
    filter_.data.word3 = collider_->GetCollisionOwnerId();
    // すべての接触候補から地面だけを選び、Playerの頭やHitBoxを高さとして使わない。
    scene_->raycast(physx::PxVec3(position_.x, position_.y + 3.0f, position_.z), physx::PxVec3(0, -1, 0),
        16.0f, hit_, physx::PxHitFlag::eDEFAULT, filter_, PhysXSystem::GetCharacterControllerQueryFilterCallback());
    bool found_ = false;
    float distance_ = 16.0f;
    for (physx::PxU32 index_ = 0; index_ < hit_.nbTouches; ++index_)
    {
        const auto& touch_ = hits_[index_];
        const auto groundCollider_ = touch_.actor ? static_cast<const ColliderComponent*>(touch_.actor->userData) : nullptr;
        if (!groundCollider_ || groundCollider_->GetCollisionRole() != ColliderComponent::CollisionRole::PHYSICAL ||
            !groundCollider_->CanBeCharacterGround() || touch_.normal.y < 0.5f || touch_.distance >= distance_) continue;
        ground_ = { touch_.position.x, touch_.position.y, touch_.position.z };
        distance_ = touch_.distance;
        found_ = true;
    }
    return found_;
}

void HumanBossActionComponent::ShowChargeArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ShowChargeArea");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform || rangedSkill != "SHADOW_CHARGE") return;
    chargeWarningReady = false;
    chargeWarningVisibleStartTime = -1.0f;
    rangedProgress = "SHADOW_CHARGE: warning preparation";
    if (!PlanCharge(attackTarget.lock(), chargeEnd)) { status = "Charge path unavailable"; Stop(); return; }
    chargeStart = owner_->transform->GetWorldPosition();
    if (!ResolveRangedGround(chargeStart, chargeGroundStart)) { status = "Charge ground unavailable"; Stop(); return; }
    const float dx_ = chargeEnd.x - chargeStart.x;
    const float dz_ = chargeEnd.z - chargeStart.z;
    chargeLength = std::sqrt(dx_ * dx_ + dz_ * dz_);
    chargeYaw = DirectX::XMConvertToDegrees(std::atan2(dx_, dz_));
    FaceTarget({ dx_, 0.0f, dz_ }, true);
    DangerAreaTemplate danger_;
    danger_.materialPath = shadowDangerMaterial;
    danger_.shapeType = DangerAreaShapeType::CAPSULE;
    danger_.radius = chargeWidth * 0.5f;
    danger_.length = chargeLength;
    danger_.groundOffset = 0.06f;
    danger_.lifeTime = chargeGrowthTime + 3.0f; // 伸長後の待機と突進中も同じ予兆を維持する。
    ClearRangedArea();
    rangedDanger = owner_->GetOrAddComponent<DangerAreaComponent>()->ShowAt(danger_, chargeGroundStart, chargeYaw);
    if (rangedDanger.expired()) { status = "Charge warning creation failed"; Stop(); return; }
    UpdateRangedAttack();
}

void HumanBossActionComponent::UpdateRangedAttack()
{
    if (!IsRangedSkill(rangedSkill)) return;
    BS_PROFILE_SCOPE("HumanBossActionComponent::UpdateRangedAttack");
    const auto owner_ = GetGameObject();
    const auto danger_ = rangedDanger.lock();
    const auto phase_ = execution.GetCurrentPhase();
    if (!owner_ || !phase_) return;
    if (rangedSkill == "SHADOW_RAGNAROK" && phase_->animationState == "HumanBossRagnarokAdvance")
    {
        const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
        if (!movement_) { Stop(); return; }
        const auto position_ = owner_->transform->GetWorldPosition();
        const float dx_ = rangedStageCenter.x - position_.x;
        const float dz_ = rangedStageCenter.z - position_.z;
        const float distance_ = std::sqrt(dx_ * dx_ + dz_ * dz_);
        if (distance_ <= 1.25f)
        {
            movement_->StopHorizontalMovementImmediately();
            execution.NotifyPhaseEnd();
        }
        else
        {
            // Transformを直接動かさず、CCTの壁・Playerとの衝突を維持して中央へ進む。
            movement_->SetFacingDirection({ dx_, 0.0f, dz_ }, true);
            movement_->SetHorizontalVelocity({ dx_ / distance_ * ragnarokAdvanceSpeed, 0.0f, dz_ / distance_ * ragnarokAdvanceSpeed });
        }
        return;
    }
    if (rangedSkill == "SHADOW_CHARGE" && phase_->animationState == "Run_Attack_01_Seq_0")
    {
        const auto warp_ = owner_->GetComponent<MotionWarpComponent>();
        const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_);
        // 経路が途中で塞がった場合、補正終了後の自然移動へ戻って突進を継続しない。
        if (warp_ && animation_ && !warp_->IsWarpActive() && animation_->GetPlaybackDuration() > 0.0f &&
            animation_->GetPlaybackTime() / animation_->GetPlaybackDuration() < 0.74f)
        { status = "Charge interrupted: Warp path blocked"; Stop(); return; }
        if (execution.GetPhaseTime() >= shadowEffectTimer)
        { SpawnShadowEffect(); shadowEffectTimer = execution.GetPhaseTime() + shadowEffectInterval; }
    }
    if (!danger_) return;
    if (rangedSkill == "SHADOW_CHARGE" && execution.GetPhaseIndex() == 0)
    {
        // GPUとObjectの開始準備が終わってから予兆の時計を進める。
        if (!danger_->IsHierarchyReady()) return;
        if (chargeWarningVisibleStartTime < 0.0f) chargeWarningVisibleStartTime = execution.GetPhaseTime();
        const float visibleTime_ = execution.GetPhaseTime() - chargeWarningVisibleStartTime;
        float rate_ = std::clamp(visibleTime_ / chargeGrowthTime, 0.0f, 1.0f);
        if (!chargeGrowthCurve.empty() && rate_ < 1.0f)
            if (const auto curve_ = owner_->GetComponent<CurveComponent>(); curve_ && curve_->HasTrack(chargeGrowthCurve))
                rate_ = std::clamp(curve_->GetValue(chargeGrowthCurve, rate_), 0.0f, 1.0f);
        const float half_ = chargeLength * rate_ * 0.5f;
        const float yaw_ = DirectX::XMConvertToRadians(chargeYaw);
        const DirectX::XMFLOAT3 center_ = { chargeStart.x + std::sin(yaw_) * half_, chargeGroundStart.y, chargeStart.z + std::cos(yaw_) * half_ };
        owner_->GetComponent<DangerAreaComponent>()->UpdateAt(danger_, center_, chargeYaw, rate_);
        chargeWarningReady = visibleTime_ >= chargeGrowthTime;
        rangedProgress = chargeWarningReady ? "SHADOW_CHARGE: warning complete" : "SHADOW_CHARGE: warning growing";
        // 完成した予兆を追加の0.2秒表示してから、次のPhaseへ移す。
        if (chargeWarningReady && visibleTime_ >= chargeGrowthTime + 0.2f)
            execution.NotifyPhaseEnd();
    }
    if (rangedSkill == "SHADOW_RAGNAROK" && phase_->sequence == "HumanBoss_SHADOW_RAGNAROK_Sequence")
    {
        // 回転終了後の猶予中は角度を保持し、そのままHitBoxへ渡す。
        ragnarokYaw = ragnarokRoundStartYaw + ragnarokRotationSpeed * (std::min)(execution.GetPhaseTime(), ragnarokRotationTime);
        owner_->GetComponent<DangerAreaComponent>()->UpdateAt(danger_, ragnarokGroundCenter, ragnarokYaw);
    }
}

void HumanBossActionComponent::SpawnShadowOrbs()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::SpawnShadowOrbs");
    const auto owner_ = GetGameObject();
    const auto target_ = attackTarget.lock();
    if (!owner_ || !owner_->transform || !target_ || rangedSkill != "SHADOW_ORB") return;
    shadowOrbs.erase(std::remove_if(shadowOrbs.begin(), shadowOrbs.end(), [](const std::weak_ptr<GameObject>& weak_)
        { const auto object_ = weak_.lock(); return !object_ || object_->IsPendingDestroy(); }), shadowOrbs.end());
    const auto attack_ = AttackTemplateManager::Instance().FindTemplate("HumanBossShadowOrb");
    if (!attack_ || attack_->attackEffects.empty()) return;
    const int count_ = (std::min)(orbCount, orbMaximumCount - static_cast<int>(shadowOrbs.size()));
    const auto start_ = owner_->transform->GetWorldPosition();
    const float yaw_ = DirectX::XMConvertToRadians(owner_->transform->GetWorldRotation().y);
    for (int index_ = 0; index_ < count_; ++index_)
    {
        auto effect_ = attack_->attackEffects[0];
        const float delay_ = orbWaitTime + index_ * orbInterval;
        effect_.lifeTime = orbLifeTime;
        effect_.speed = 0.0f; // 移動は既存ProjectileMotionだけが引き受ける。
        for (auto& shape_ : effect_.hitShapes) { shape_.activeStartTime = delay_; shape_.activeEndTime = orbLifeTime; shape_.destroyOnHit = true; }
        const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
        const auto orb_ = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *attack_, owner_.get(), instance_);
        if (!orb_ || !orb_->transform) continue;
        const float side_ = (index_ - (count_ - 1) * 0.5f) * orbSideOffset;
        const DirectX::XMFLOAT3 position_ = { start_.x + std::cos(yaw_) * side_ - std::sin(yaw_) * orbBackOffset,
            start_.y + orbHeight + (index_ == count_ / 2 ? 0.4f : 0.0f),
            start_.z - std::sin(yaw_) * side_ - std::cos(yaw_) * orbBackOffset };
        orb_->transform->ApplyWorldMatrix(DirectX::XMMatrixTranslation(position_.x, position_.y, position_.z));
        orb_->UpdateTransform();
        const auto motion_ = orb_->GetOrAddComponent<ProjectileMotionComponent>();
        motion_->Configure({ 0.0f, 0.0f, 1.0f }, orbSpeed, orbAcceleration, 0.0f, orbSpeed + 1.0f);
        motion_->ConfigureHoming(target_, owner_, delay_, orbTurnSpeed, effect_.hitShapes.empty() ? 0.65f : effect_.hitShapes[0].radius);
        shadowOrbs.push_back(orb_);
    }
}

void HumanBossActionComponent::ShowRagnarokArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::ShowRagnarokArea");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform || rangedSkill != "SHADOW_RAGNAROK") return;
    const auto position_ = owner_->transform->GetWorldPosition();
    const float dx_ = position_.x - rangedStageCenter.x;
    const float dz_ = position_.z - rangedStageCenter.z;
    if (dx_ * dx_ + dz_ * dz_ > 1.5f * 1.5f) { status = "Ragnarok center not reached"; Stop(); return; }
    ClearRangedArea();
    ragnarokRoundStartYaw = ragnarokRound == 0 ? owner_->transform->GetWorldRotation().y : ragnarokYaw + 90.0f;
    ragnarokYaw = ragnarokRoundStartYaw;
    ragnarokBlockReason.clear();
    ++ragnarokRound;
    DangerAreaTemplate danger_;
    danger_.shapeType = DangerAreaShapeType::FAN;
    danger_.materialPath = shadowDangerMaterial;
    danger_.length = (std::min)(ragnarokRadius, rangedStageRadius);
    danger_.fanAngleDegrees = ragnarokAngle;
    danger_.lifeTime = execution.GetCurrentPhase() ? execution.GetCurrentPhase()->duration + 0.05f : 1.5f;
    DirectX::XMFLOAT3 ground_{};
    if (!ResolveRangedGround(rangedStageCenter, ground_)) { status = "Ragnarok warning ground unavailable"; Stop(); return; }
    ragnarokGroundCenter = ground_;
    rangedDanger = owner_->GetOrAddComponent<DangerAreaComponent>()->ShowAt(danger_, ragnarokGroundCenter, ragnarokYaw);
    if (rangedDanger.expired()) { status = "Ragnarok warning creation failed"; Stop(); return; }
    ragnarokUsed = true;
    rangedProgress = "SHADOW_RAGNAROK: rotating warning " + std::to_string(ragnarokRound);
}

void HumanBossActionComponent::StrikeRagnarokArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::StrikeRagnarokArea");
    const auto owner_ = GetGameObject();
    if (!owner_ || rangedSkill != "SHADOW_RAGNAROK" || ragnarokRound <= 0) return;
    const auto attack_ = AttackTemplateManager::Instance().FindTemplate("HumanBossShadowRagnarok");
    if (!attack_) { Stop(); return; }
    rangedProgress = "SHADOW_RAGNAROK: strike " + std::to_string(ragnarokRound);
    ragnarokYaw = ragnarokRoundStartYaw + ragnarokRotationSpeed * ragnarokRotationTime;
    ClearRangedArea();
    const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
    for (auto effect_ : attack_->attackEffects)
    {
        for (auto& shape_ : effect_.hitShapes)
        { shape_.range = (std::min)(ragnarokRadius, rangedStageRadius); shape_.angle = ragnarokAngle; shape_.activeStartTime = 0.0f; shape_.activeEndTime = ragnarokHitDuration; }
        effect_.lifeTime = ragnarokHitDuration + 0.25f;
        const auto object_ = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *attack_, owner_.get(), instance_);
        if (object_ && object_->transform)
        {
            // 回転停止時の中心・角度を固定し、各回だけ別の攻撃IDを使用する。
            object_->transform->ApplyWorldMatrix(DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(ragnarokYaw)) *
                DirectX::XMMatrixTranslation(ragnarokGroundCenter.x, ragnarokGroundCenter.y, ragnarokGroundCenter.z));
            object_->UpdateTransform();
            if (const auto mesh_ = object_->GetComponent<MeshVFXComponent>())
            {
                for (auto& part_ : mesh_->GetAsset().parts)
                { part_.shapeCylinderRadius = (std::min)(ragnarokRadius, rangedStageRadius); part_.internalMeshSettings.endAngleDegrees = ragnarokAngle; }
                // Editorで変えた半径と角度を内部生成Meshにも反映する。
                mesh_->RebuildRuntimeResources();
            }
        }
    }
}

void HumanBossActionComponent::ClearRangedArea()
{
    if (const auto owner_ = GetGameObject(); owner_ && owner_->manager)
        if (const auto danger_ = owner_->GetComponent<DangerAreaComponent>()) danger_->StopAll();
    rangedDanger.reset();
}

void HumanBossActionComponent::DrawRangedGui()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::DrawRangedGui");
    if (!ImGui::TreeNode("Ranged / Special attacks")) return;
    ImGui::DragFloat("ChargeMaximumDistance", &chargeMaximumDistance, 0.05f, 1.0f, 40.0f);
    ImGui::DragFloat("ChargeWidth", &chargeWidth, 0.05f, 0.2f, 10.0f);
    ImGui::DragFloat("ChargeGrowthTime", &chargeGrowthTime, 0.05f, 0.1f, 5.0f);
    ImGui::DragInt("OrbCount", &orbCount, 1.0f, 1, 6);
    ImGui::DragInt("OrbMaximumCount", &orbMaximumCount, 1.0f, 1, 12);
    ImGui::DragFloat("OrbSideOffset", &orbSideOffset, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("OrbHeight", &orbHeight, 0.05f, 1.0f, 10.0f);
    ImGui::DragFloat("OrbBackOffset", &orbBackOffset, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("OrbWaitTime", &orbWaitTime, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("OrbInterval", &orbInterval, 0.05f, 0.0f, 2.0f);
    ImGui::DragFloat("OrbSpeed", &orbSpeed, 0.05f, 0.1f, 10.0f);
    ImGui::DragFloat("OrbAcceleration", &orbAcceleration, 0.05f, 0.0f, 5.0f);
    ImGui::DragFloat("OrbTurnSpeed", &orbTurnSpeed, 0.05f, 0.0f, 90.0f);
    ImGui::DragFloat("OrbLifeTime", &orbLifeTime, 0.05f, 2.0f, 20.0f);
    ImGui::DragFloat("RagnarokHpThreshold", &ragnarokHpThreshold, 0.05f, 0.01f, 1.0f);
    ImGui::DragInt("RagnarokAttackCount", &ragnarokAttackCount, 1.0f, 1, 8);
    ImGui::DragFloat("RagnarokAdvanceSpeed", &ragnarokAdvanceSpeed, 0.1f, 1.0f, 16.0f);
    ImGui::DragFloat("RagnarokRadius", &ragnarokRadius, 0.05f, 1.0f, 100.0f);
    ImGui::DragFloat("RagnarokAngle", &ragnarokAngle, 0.05f, 1.0f, 360.0f);
    ImGui::DragFloat("RagnarokRotationSpeed", &ragnarokRotationSpeed, 0.05f, -360.0f, 360.0f);
    ImGui::DragFloat("RagnarokRotationTime", &ragnarokRotationTime, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("RagnarokHitDuration", &ragnarokHitDuration, 0.05f, 0.05f, 1.0f);
    ImGui::DragFloat("RangedStageRadius", &rangedStageRadius, 0.05f, 5.0f, 100.0f);
    ImGui::Checkbox("RagnarokEnabled", &ragnarokEnabled);
    ImGui::InputText("ChargeGrowthCurve", &chargeGrowthCurve);
    ImGui::InputText("Ranged Attack SE Path", &rangedAttackSoundPath);
    ImGui::Text("Ragnarok start conditions: %s", CanUseRagnarok() ? "AVAILABLE" : "BLOCKED");
    ImGui::TextWrapped("Ragnarok block reason: %s", ragnarokBlockReason.empty() ? "None" : ragnarokBlockReason.c_str());
    ImGui::DragFloat3("Ranged Stage Center", &rangedStageCenter.x, 0.1f);
    int orbCount_ = 0;
    for (const auto& weak_ : shadowOrbs)
        if (const auto orb_ = weak_.lock(); orb_ && !orb_->IsPendingDestroy()) ++orbCount_;
    ImGui::Text("Independent Orbs: %d / %d", orbCount_, orbMaximumCount);
    ImGui::Text("Charge length %.2f / yaw %.1f", chargeLength, chargeYaw);
    ImGui::Text("Charge warning ready: %s / visible start %.2f", chargeWarningReady ? "YES" : "NO", chargeWarningVisibleStartTime);
    ImGui::TextWrapped("Ranged progress: %s", rangedProgress.c_str());
    ImGui::Text("Ragnarok used %s / round %d / yaw %.1f", ragnarokUsed ? "YES" : "NO", ragnarokRound, ragnarokYaw);
    ImGui::TreePop();
    // GUIからも読み込みと同じ制限を適用し、0秒や負の寿命を実行へ渡さない。
    nlohmann::json settings_;
    SerializeRanged(settings_);
    DeserializeRanged(settings_);
}

void HumanBossActionComponent::SerializeRanged(nlohmann::json& json_) const
{
    json_["ChargeMaximumDistance"] = chargeMaximumDistance;
    json_["ChargeWidth"] = chargeWidth;
    json_["ChargeGrowthTime"] = chargeGrowthTime;
    json_["OrbCount"] = orbCount;
    json_["OrbMaximumCount"] = orbMaximumCount;
    json_["OrbSideOffset"] = orbSideOffset;
    json_["OrbHeight"] = orbHeight;
    json_["OrbBackOffset"] = orbBackOffset;
    json_["OrbWaitTime"] = orbWaitTime;
    json_["OrbInterval"] = orbInterval;
    json_["OrbSpeed"] = orbSpeed;
    json_["OrbAcceleration"] = orbAcceleration;
    json_["OrbTurnSpeed"] = orbTurnSpeed;
    json_["OrbLifeTime"] = orbLifeTime;
    json_["RagnarokHpThreshold"] = ragnarokHpThreshold;
    json_["RagnarokAttackCount"] = ragnarokAttackCount;
    json_["RagnarokAdvanceSpeed"] = ragnarokAdvanceSpeed;
    json_["RagnarokRadius"] = ragnarokRadius;
    json_["RagnarokAngle"] = ragnarokAngle;
    json_["RagnarokRotationSpeed"] = ragnarokRotationSpeed;
    json_["RagnarokRotationTime"] = ragnarokRotationTime;
    json_["RagnarokHitDuration"] = ragnarokHitDuration;
    json_["RangedStageRadius"] = rangedStageRadius;
    json_["ChargeGrowthCurve"] = chargeGrowthCurve;
    json_["RangedAttackSoundPath"] = rangedAttackSoundPath;
    json_["RagnarokEnabled"] = ragnarokEnabled;
    json_["RangedStageCenter"] = { rangedStageCenter.x, rangedStageCenter.y, rangedStageCenter.z };
}

void HumanBossActionComponent::DeserializeRanged(const nlohmann::json& json_)
{
    chargeMaximumDistance = std::clamp(json_.value("ChargeMaximumDistance", chargeMaximumDistance), 1.0f, 40.0f);
    chargeWidth = std::clamp(json_.value("ChargeWidth", chargeWidth), 0.2f, 10.0f);
    chargeGrowthTime = std::clamp(json_.value("ChargeGrowthTime", chargeGrowthTime), 0.1f, 5.0f);
    orbCount = std::clamp(json_.value("OrbCount", orbCount), 1, 6);
    orbMaximumCount = std::clamp(json_.value("OrbMaximumCount", orbMaximumCount), 1, 12);
    orbSideOffset = std::clamp(json_.value("OrbSideOffset", orbSideOffset), 0.0f, 5.0f);
    orbHeight = std::clamp(json_.value("OrbHeight", orbHeight), 1.0f, 10.0f);
    orbBackOffset = std::clamp(json_.value("OrbBackOffset", orbBackOffset), 0.0f, 5.0f);
    orbWaitTime = std::clamp(json_.value("OrbWaitTime", orbWaitTime), 0.1f, 5.0f);
    orbInterval = std::clamp(json_.value("OrbInterval", orbInterval), 0.0f, 2.0f);
    orbSpeed = std::clamp(json_.value("OrbSpeed", orbSpeed), 0.1f, 10.0f);
    orbAcceleration = std::clamp(json_.value("OrbAcceleration", orbAcceleration), 0.0f, 5.0f);
    orbTurnSpeed = std::clamp(json_.value("OrbTurnSpeed", orbTurnSpeed), 0.0f, 90.0f);
    orbLifeTime = std::clamp(json_.value("OrbLifeTime", orbLifeTime), 2.0f, 20.0f);
    ragnarokHpThreshold = std::clamp(json_.value("RagnarokHpThreshold", ragnarokHpThreshold), 0.01f, 1.0f);
    ragnarokAttackCount = std::clamp(json_.value("RagnarokAttackCount", ragnarokAttackCount), 1, 8);
    ragnarokAdvanceSpeed = std::clamp(json_.value("RagnarokAdvanceSpeed", ragnarokAdvanceSpeed), 1.0f, 16.0f);
    ragnarokRadius = std::clamp(json_.value("RagnarokRadius", ragnarokRadius), 1.0f, 100.0f);
    ragnarokAngle = std::clamp(json_.value("RagnarokAngle", ragnarokAngle), 1.0f, 360.0f);
    ragnarokRotationSpeed = std::clamp(json_.value("RagnarokRotationSpeed", ragnarokRotationSpeed), -360.0f, 360.0f);
    ragnarokRotationTime = std::clamp(json_.value("RagnarokRotationTime", ragnarokRotationTime), 0.1f, 5.0f);
    ragnarokHitDuration = std::clamp(json_.value("RagnarokHitDuration", ragnarokHitDuration), 0.05f, 1.0f);
    rangedStageRadius = std::clamp(json_.value("RangedStageRadius", rangedStageRadius), 5.0f, 100.0f);
    orbCount = (std::min)(orbCount, orbMaximumCount);
    orbLifeTime = (std::max)(orbLifeTime, orbWaitTime + (orbCount - 1) * orbInterval + 1.0f);
    chargeGrowthCurve = json_.value("ChargeGrowthCurve", chargeGrowthCurve);
    rangedAttackSoundPath = json_.value("RangedAttackSoundPath", rangedAttackSoundPath);
    ragnarokEnabled = json_.value("RagnarokEnabled", ragnarokEnabled);
    if (json_.contains("RangedStageCenter") && json_["RangedStageCenter"].is_array() && json_["RangedStageCenter"].size() == 3)
        rangedStageCenter = { json_["RangedStageCenter"][0].get<float>(), json_["RangedStageCenter"][1].get<float>(), json_["RangedStageCenter"][2].get<float>() };
}
