#include "PrecompiledHeader.h"
#include "HumanBossActionComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Common/DebugLog.h"
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
#include "Engine/Graphics/Particle/ParticleComponent.h"
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
    // コンポーネント自体の破棄では、行動をまたいで残した弾も終了する。
    if (const auto owner_ = GetGameObject(); owner_ && owner_->manager)
        owner_->GetManager().GetEffectManager().StopAttackEffects(owner_);
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
    // 対象方向の予兆と突進を同じ倍率で伸ばし、上限距離と既存の経路検証を維持する。
    const float length_ = (std::min)(chargeMaximumDistance, (std::max)(0.0f, distance_ - chargeWidth) * chargeDistanceMultiplier);
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
    if (skill_.name == "COUNTER_ATTACK" && !result_.phases.empty())
    {
        // 拘束本体だけを固定時間へ揃え、先頭に追加したひるみ段階の待ち時間は維持する。
        for (auto& phase_ : result_.phases)
        {
            if (phase_.enterAction != "HumanBoss_ExecutionCapture") continue;
            phase_.duration = executionHoldTime;
            phase_.maximumDuration = executionHoldTime + 1.0f;
        }
    }
    if (skill_.name == "SHADOW_CHARGE" && !result_.phases.empty())
    {
        // 保存済みの古い攻撃姿勢も、突進のチャージ中だけガード姿勢へ置き換える。
        result_.phases[0].animationState = "SS_Guard";
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
            advance_.maximumDuration = std::sqrt(dx_ * dx_ + dz_ * dz_) / (ragnarokAdvanceSpeed * ragnarokAdvanceMultiplier) + 3.0f;
            advance_.forwardSpeed = 0.0f; // 中央方向の速度は実行担当だけが予約する。
            result_.phases.push_back(advance_);
        }
        for (int round_ = 0; round_ < ragnarokAttackCount; ++round_)
        {
            auto tell_ = skill_.phases[1];
            tell_.duration = (std::max)(ragnarokTellTime, ragnarokRotationTime + 0.2f);
            result_.phases.push_back(tell_);
            auto strike_ = skill_.phases[2];
            strike_.duration = ragnarokHitDuration;
            result_.phases.push_back(strike_);
        }
        auto recovery_ = skill_.phases.back();
        recovery_.duration = ragnarokRecoveryTime;
        result_.phases.push_back(recovery_);
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
    // 予兆の向きを維持し、待機中の姿勢変化で接触判定の向きがずれないようにする。
    const float yaw_ = DirectX::XMConvertToRadians(chargeYaw);
    FaceTarget({ std::sin(yaw_), 0.0f, std::cos(yaw_) }, true);
    DirectX::XMFLOAT3 local_{};
    float duration_ = 0.0f;
    if (!animation_->EstimateRootMotion(phase_.animationState, 0.0f, CHARGE_WARP_END_NORMALIZED_TIME, local_, duration_)) return false;
    DirectX::XMFLOAT3 planned_{};
    DirectX::XMStoreFloat3(&planned_, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&local_), DirectX::XMMatrixRotationY(yaw_)));
    planned_.y = 0.0f;
    const std::weak_ptr<HumanBossActionComponent> weak_ = owner_->GetComponent<HumanBossActionComponent>();
    return warp_->PlayForTarget("HumanBoss.ShadowCharge", attackTarget.lock(), planned_, 0.0f, 0.0f, CHARGE_WARP_END_NORMALIZED_TIME, &end_,
        [weak_](const DirectX::XMFLOAT3& from_, const DirectX::XMFLOAT3& to_)
        { const auto self_ = weak_.lock(); return self_ && self_->CanWarpPath(from_, to_); }, 0.0f, &chargeStart);
}

void HumanBossActionComponent::BeginRangedPhase(const SkillPhase& phase_)
{
    if (!IsRangedSkill(rangedSkill)) return;
    BS_PROFILE_SCOPE("HumanBossActionComponent::BeginRangedPhase");
    if (rangedSkill == "SHADOW_ORB")
    {
        // 詠唱開始時に弾を確定し、後の行動切り替えで生成予約ごと失われるのを防ぐ。
        SpawnShadowOrbs();
        if (!orbSpawnCommitted) { status = "Shadow Orb creation failed"; Stop(); return; }
    }
    if (rangedSkill == "SHADOW_RAGNAROK" && phase_.animationState != "HumanBossRagnarokAdvance")
    {
        // 移動Phaseから通知で切り替わった場合も、予兆開始前にモデルと停止状態を復元する。
        SetShadowVisible(true);
        if (const auto owner_ = GetGameObject())
            if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovementImmediately();
    }
    if (phase_.animationState == "Run_Attack_01_Seq_0")
    {
        // 予兆生成・伸長が完了していない状態では突進へ進ませない。
        if (!chargeWarningReady) { status = "Charge warning did not complete"; Stop(); return; }
        if (!StartFixedWarp(phase_, chargeEnd)) { status = "Charge Warp failed"; Stop(); return; }
        const auto owner_ = GetGameObject();
        const auto attack_ = AttackTemplateManager::Instance().FindTemplate("HumanBossShadowCharge");
        if (!owner_ || !owner_->transform || !attack_) { Stop(); return; }
        const auto scale_ = owner_->transform->GetWorldScale();
        if (std::abs(scale_.x) < 0.0001f || std::abs(scale_.y) < 0.0001f || std::abs(scale_.z) < 0.0001f) { Stop(); return; }
        // 武器ボーンの振れ幅から独立させ、予兆と同じ半径の判定を突進に追従させる。
        AttackEffectSpawnTemplate effect_;
        effect_.effectType = ATTACK_EFFECT_TYPE::AREA_OVER_TIME;
        effect_.followOwner = true;
        effect_.localOffset = { 0.0f, 0.0f, 0.0f };
        effect_.localScale = { 1.0f / scale_.x, 1.0f / scale_.y, 1.0f / scale_.z };
        effect_.lifeTime = phase_.maximumDuration;
        AttackEffectHitShapeTemplate shape_;
        shape_.hitBoxName = "ShadowChargeHit";
        shape_.shape = ATTACK_HIT_SHAPE::SPHERE;
        shape_.radius = chargeAreaWidth * 0.5f;
        shape_.localOffset = { 0.0f, shape_.radius, shape_.radius };
        shape_.activeEndTime = effect_.lifeTime;
        effect_.hitShapes.push_back(shape_);
        const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
        chargeHit = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *attack_, owner_.get(), instance_);
        if (chargeHit.expired()) { status = "Charge hit creation failed"; Stop(); return; }
        rangedProgress = "SHADOW_CHARGE: running";
        SetWeaponTrails(true, true);
        shadowEffectTimer = shadowEffectInterval;
        if (!rangedAttackSoundPath.empty())
            AudioManager::Instance().PlaySpatialAttackSound(rangedAttackSoundPath,
                owner_->transform->GetWorldPosition(), 0.45f, 8.0f, 80.0f);
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
        {
            if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>()) warp_->Stop();
            if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>()) movement_->StopHorizontalMovementImmediately();
        }
        // モデルだけを影へ隠し、被弾判定と物理衝突を保ったまま中央へ高速移動する。
        SpawnShadowEffect();
        SetShadowVisible(false);
        shadowEffectTimer = shadowEffectInterval;
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
    chargeElectricStruck = false;
    chargeWarningVisibleStartTime = -1.0f;
    rangedProgress = "SHADOW_CHARGE: warning preparation";
    if (!PlanCharge(attackTarget.lock(), chargeEnd)) { status = "Charge path unavailable"; Stop(); return; }
    chargeAreaWidth = chargeWidth;
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
    danger_.radius = chargeAreaWidth * 0.5f;
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
            SetShadowVisible(true);
            SpawnShadowEffect();
            execution.NotifyPhaseEnd();
        }
        else
        {
            // 残り距離で速度を制限し、高速移動でも中央を通り越して往復しないようにする。
            const float speed_ = (std::min)(ragnarokAdvanceSpeed * ragnarokAdvanceMultiplier,
                (std::max)(0.0f, distance_ - 0.75f) / (std::max)(assistFrameDeltaTime, 0.0001f));
            movement_->SetFacingDirection({ dx_, 0.0f, dz_ }, true);
            movement_->SetHorizontalVelocity({ dx_ / distance_ * speed_, 0.0f, dz_ / distance_ * speed_ });
            if (execution.GetPhaseTime() >= shadowEffectTimer)
            {
                SpawnShadowEffect();
                shadowEffectTimer = execution.GetPhaseTime() + shadowEffectInterval;
            }
        }
        return;
    }
    if (rangedSkill == "SHADOW_CHARGE" && phase_->animationState == "Run_Attack_01_Seq_0")
    {
        const auto warp_ = owner_->GetComponent<MotionWarpComponent>();
        const auto animation_ = AnimationGraphBridgeComponent::Acquire(owner_);
        // 経路が途中で塞がった場合、補正終了後の自然移動へ戻って突進を継続しない。
        if (warp_ && animation_ && !warp_->IsWarpActive() && animation_->GetPlaybackDuration() > 0.0f &&
            animation_->GetPlaybackTime() / animation_->GetPlaybackDuration() < CHARGE_WARP_END_NORMALIZED_TIME - 0.01f)
        { status = "Charge interrupted: Warp path blocked"; Stop(); return; }
        // 振り終わり以降の硬直には接触判定を残さない。
        if (animation_ && animation_->GetActiveState() && animation_->GetActiveState()->name == phase_->animationState &&
            animation_->GetPlaybackDuration() > 0.0f &&
            animation_->GetPlaybackTime() / animation_->GetPlaybackDuration() >= CHARGE_WARP_END_NORMALIZED_TIME)
        {
            StopChargeHit();
            if (!chargeElectricStruck)
            {
                // 最終フレームの物理同期を待ち、未到達の経路全体へ電撃を発生させない。
                const auto position_ = owner_->transform->GetWorldPosition();
                const float dx_ = position_.x - chargeEnd.x;
                const float dz_ = position_.z - chargeEnd.z;
                constexpr float CHARGE_ARRIVAL_TOLERANCE = 0.1f;
                if (dx_ * dx_ + dz_ * dz_ <= CHARGE_ARRIVAL_TOLERANCE * CHARGE_ARRIVAL_TOLERANCE)
                    StrikeChargeArea();
                else if (animation_->GetPlaybackTime() / animation_->GetPlaybackDuration() >
                    CHARGE_WARP_END_NORMALIZED_TIME + 0.1f)
                { status = "Charge interrupted: warning endpoint not reached"; Stop(); return; }
            }
        }
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
        // 判定中心の前方オフセットも含め、実際に通過する球の包絡範囲を表示する。
        const float half_ = chargeLength * rate_ * 0.5f + chargeAreaWidth * 0.5f;
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
        // 総回転角を保ったまま、開始時を速くし、攻撃直前に向けて滑らかに減速する。
        const float rotationRate_ = std::clamp(execution.GetPhaseTime() / (std::max)(ragnarokRotationTime, 0.0001f), 0.0f, 1.0f);
        const float easedRotationRate_ = rotationRate_ * (2.0f - rotationRate_);
        ragnarokYaw = ragnarokRoundStartYaw;
        RotateRagnarokGrid(ragnarokRotationSpeed * ragnarokRotationTime * easedRotationRate_);
        if (const auto mesh_ = danger_->GetComponent<MeshVFXComponent>())
        {
            auto& parts_ = mesh_->GetAsset().parts;
            for (std::size_t index_ = 0; index_ < (std::min)(parts_.size(),ragnarokGrid.size()); ++index_)
            {
                parts_[index_].position = ragnarokGrid[index_].position;
                parts_[index_].rotation.y = ragnarokGrid[index_].yaw;
            }
        }
        owner_->GetComponent<DangerAreaComponent>()->UpdateAt(danger_, ragnarokGroundCenter, ragnarokRoundStartYaw);
    }
}

void HumanBossActionComponent::SpawnShadowOrbs()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::SpawnShadowOrbs");
    const auto owner_ = GetGameObject();
    const auto target_ = attackTarget.lock();
    if (!owner_ || !owner_->transform || !target_ || rangedSkill != "SHADOW_ORB" || orbSpawnCommitted) return;
    shadowOrbs.erase(std::remove_if(shadowOrbs.begin(), shadowOrbs.end(), [](const std::weak_ptr<GameObject>& weak_)
        { const auto object_ = weak_.lock(); return !object_ || object_->IsPendingDestroy(); }), shadowOrbs.end());
    const auto attack_ = AttackTemplateManager::Instance().FindTemplate("HumanBossShadowOrb");
    if (!attack_ || attack_->attackEffects.empty()) return;
    const int count_ = (std::min)(orbCount, orbMaximumCount - static_cast<int>(shadowOrbs.size()));
    // 周回弾の個数だけ音を重ねず、一回の生成要求で一度だけ鳴らす。
    if (count_ > 0)
    {
        const auto& feedback_ = attack_->feedback;
        AudioManager::Instance().PlaySpatialAttackSound(feedback_.startSoundPath,
            owner_->transform->GetWorldPosition(), feedback_.volume, feedback_.minDistance, feedback_.maxDistance);
    }
    const float yaw_ = owner_->transform->GetWorldRotation().y;
    for (int index_ = 0; index_ < count_; ++index_)
    {
        auto effect_ = attack_->attackEffects[0];
        const float delay_ = (std::max)(orbWaitTime, orbOrbitDuration) + index_ * orbInterval;
        const float lifeTime_ = (std::max)(orbLifeTime, delay_ + 1.0f);
        effect_.lifeTime = lifeTime_;
        effect_.speed = 0.0f; // 移動は既存ProjectileMotionだけが引き受ける。
        // 周回中は攻撃判定を無効にし、発射した弾だけがプレイヤーへ命中する。
        for (auto& shape_ : effect_.hitShapes) { shape_.activeStartTime = delay_; shape_.activeEndTime = lifeTime_; shape_.destroyOnHit = true; }
        const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
        const auto orb_ = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *attack_, owner_.get(), instance_);
        if (!orb_ || !orb_->transform) continue;
        const auto motion_ = orb_->GetOrAddComponent<ProjectileMotionComponent>();
        motion_->Configure({ 0.0f, 0.0f, 1.0f }, orbSpeed * orbLaunchSpeedMultiplier,
            orbAcceleration * orbLaunchSpeedMultiplier, 0.0f, (orbSpeed + 1.0f) * orbLaunchSpeedMultiplier);
        motion_->ConfigureHoming(target_, owner_, delay_, orbTurnSpeed, effect_.hitShapes.empty() ? 0.65f : effect_.hitShapes[0].radius);
        // 円周上へ等間隔に配置し、発射までは移動中の敵を中心に周回させる。
        motion_->ConfigureOrbit(orbOrbitRadius, orbHeight, orbOrbitTilt, orbOrbitSpeed,
            360.0f * static_cast<float>(index_) / static_cast<float>(count_), yaw_);
        shadowOrbs.push_back(orb_);
        orbSpawnCommitted = true;
    }
}

void HumanBossActionComponent::BuildRagnarokGrid()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::BuildRagnarokGrid");
    ragnarokGrid.clear();
    const float radius_ = (std::min)(ragnarokRadius, rangedStageRadius);
    const int count_ = std::clamp(ragnarokGridLineCount, 3, 12);
    // 小さいステージでも帯同士を接続せず、必ず回避できる隙間を残す。
    const float spacing_ = (std::min)(ragnarokGridSpacing, radius_ * 1.5f / static_cast<float>(count_ - 1));
    const float halfWidth_ = (std::min)(ragnarokGridWidth * 0.5f, spacing_ * 0.4f);
    for (int axis_ = 0; axis_ < 2; ++axis_)
    {
        for (int index_ = 0; index_ < count_; ++index_)
        {
            // 偶数本でも中心線を残し、密着だけで全ラウンドを回避できる穴を作らない。
            const float offset_ = (index_ - count_ / 2) * spacing_;
            const float edge_ = std::abs(offset_) + halfWidth_;
            if (edge_ >= radius_) continue;
            RagnarokGridStrip strip_;
            strip_.position = axis_ == 0 ? DirectX::XMFLOAT3{ offset_, 0.0f, 0.0f } : DirectX::XMFLOAT3{ 0.0f, 0.0f, offset_ };
            strip_.yaw = axis_ == 0 ? 0.0f : 90.0f;
            strip_.offset = offset_;
            strip_.axis = axis_;
            strip_.halfWidth = halfWidth_;
            // 四隅が円形範囲を越えない長さにそろえ、回転中もステージ内へ収める。
            strip_.length = 2.0f * std::sqrt((std::max)(0.0f, radius_ * radius_ - edge_ * edge_));
            ragnarokGrid.push_back(strip_);
        }
    }
}

void HumanBossActionComponent::RotateRagnarokGrid(float angle_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::RotateRagnarokGrid");
    for (auto& strip_ : ragnarokGrid)
    {
        const float rotation_ = strip_.axis == 0 ? angle_ : -angle_;
        const auto base_ = strip_.axis == 0 ? DirectX::XMFLOAT3{strip_.offset,0,0} : DirectX::XMFLOAT3{0,0,strip_.offset};
        DirectX::XMStoreFloat3(&strip_.position,DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&base_),
            DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(rotation_))));
        strip_.yaw = (strip_.axis == 0 ? 0.0f : 90.0f) + rotation_;
    }
}

// 予兆のカプセル形状を中央の箱と両端の丸い判定で覆い、重なる部分の多重Hitを防ぐ。
void HumanBossActionComponent::StrikeChargeArea()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::StrikeChargeArea");
    const auto owner_ = GetGameObject();
    const auto attack_ = AttackTemplateManager::Instance().FindTemplate("HumanBossShadowCharge");
    if (!owner_ || !owner_->manager || !attack_ || chargeElectricStruck) return;
    chargeElectricStruck = true;
    const float radius_ = chargeAreaWidth * 0.5f;
    const float yaw_ = DirectX::XMConvertToRadians(chargeYaw);
    const DirectX::XMFLOAT3 start_ = { chargeGroundStart.x + std::sin(yaw_) * radius_,
        chargeGroundStart.y + 0.65f, chargeGroundStart.z + std::cos(yaw_) * radius_ };
    const DirectX::XMFLOAT3 end_ = { start_.x + std::sin(yaw_) * chargeLength,
        start_.y, start_.z + std::cos(yaw_) * chargeLength };
    SpawnElectricStrip(start_, end_, chargeLength, chargeElectricDuration);

    AttackEffectSpawnTemplate effect_;
    effect_.effectType = ATTACK_EFFECT_TYPE::AREA_OVER_TIME;
    effect_.followOwner = false;
    effect_.lifeTime = chargeElectricDuration;
    AttackEffectHitShapeTemplate shape_;
    shape_.hitBoxName = "ShadowChargeElectricHit";
    shape_.shape = ATTACK_HIT_SHAPE::BOX;
    shape_.radius = radius_;
    shape_.halfHeight = 1.0f;
    shape_.range = chargeLength;
    shape_.localOffset = { 0.0f, 1.0f, radius_ + chargeLength * 0.5f };
    shape_.activeEndTime = chargeElectricDuration;
    effect_.hitShapes.push_back(shape_);
    shape_.shape = ATTACK_HIT_SHAPE::CAPSULE;
    shape_.halfHeight = 0.0f;
    for (int index_ = 0; index_ < 2; ++index_)
    {
        shape_.localOffset.z = radius_ + chargeLength * static_cast<float>(index_);
        effect_.hitShapes.push_back(shape_);
    }
    // 接触攻撃とは別の攻撃IDを使い、電撃中に範囲へ入った対象も一度だけ被弾できる。
    const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
    const auto object_ = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *attack_, owner_.get(), instance_);
    if (object_ && object_->transform)
    {
        object_->transform->ApplyWorldMatrix(DirectX::XMMatrixRotationY(yaw_) *
            DirectX::XMMatrixTranslation(chargeGroundStart.x, chargeGroundStart.y, chargeGroundStart.z));
        object_->UpdateTransform();
    }
    if (const auto danger_ = owner_->GetComponent<DangerAreaComponent>()) danger_->StopAll();
    rangedDanger.reset();
    rangedProgress = "SHADOW_CHARGE: electric ground active";
}

// 格子と突進跡で同じ発生量補正を使い、電撃だけを指定倍率へ増やす。
void HumanBossActionComponent::SpawnElectricStrip(const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_,
    float length_, float sweepTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::SpawnElectricStrip");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    auto& effects_ = owner_->GetManager().GetEffectManager();
    RagnarokParticleSweep sweep_;
    sweep_.start = start_;
    sweep_.end = end_;
    sweep_.duration = sweepTime_;
    ParticleSpawnRequest request_;
    request_.assetPath = ragnarokParticleAsset;
    request_.position = start_;
    request_.lifeTime = sweepTime_ + 0.7f;
    const auto object_ = effects_.SpawnParticle(request_);
    if (!object_)
    {
        BS_LOG_INFO(LOG_CATEGORY::GAME, "Electric particle unavailable: " + ragnarokParticleAsset);
        return;
    }
    effects_.RegisterAttackEffectOwner(object_, owner_);
    if (const auto particle_ = object_->GetComponent<ParticleComponent>())
    {
        // 全長へのBurstと移動するEmitterを併用し、遠い端まで電撃を表示する。
        particle_->SetEmitterLine("", start_, end_);
        particle_->SetEmitterLine("GridElectricSweep", start_, start_);
        particle_->SetEmitterLine("GridMudSweep", start_, start_);
        for (auto& track_ : particle_->GetTracks())
        {
            if (track_.name == "GridElectric")
                track_.settings.burstCount = static_cast<unsigned int>(std::clamp(static_cast<int>(length_ * ragnarokParticleDensity), 16, 96) * electricDensityMultiplier);
            if (track_.name == "GridElectricSweep")
            {
                // Pool再利用時に前回の倍率へ再乗算しないよう、標準Assetの基準発生数から求める。
                constexpr float ELECTRIC_SWEEP_BASE_RATE = 240.0f;
                track_.settings.burstCount = static_cast<unsigned int>(ELECTRIC_SWEEP_BASE_RATE * electricDensityMultiplier);
            }
            if (track_.name == "GridMud")
                track_.settings.burstCount = std::clamp(static_cast<int>(length_ * ragnarokParticleDensity * 0.5f), 8, 48);
        }
        particle_->SetEmissionScale(std::clamp(length_ * ragnarokParticleDensity / 48.0f, 0.1f, 1.0f));
        // 通常Updateを待たずBurstを渡し、地面によるBillboardの隠れも避ける。
        particle_->Update(1.0f / 60.0f);
    }
    sweep_.object = object_;
    ragnarokParticleSweeps.push_back(sweep_);
}

void HumanBossActionComponent::SpawnRagnarokParticles()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::SpawnRagnarokParticles");
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;
    const auto world_ = DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(ragnarokRoundStartYaw)) *
        DirectX::XMMatrixTranslation(ragnarokGroundCenter.x,ragnarokGroundCenter.y+0.65f,ragnarokGroundCenter.z);
    const int limit_ = std::clamp(ragnarokParticleLineLimit,2,24);
    for (std::size_t index_ = 0; index_ < ragnarokGrid.size() && index_ < static_cast<std::size_t>(limit_); ++index_)
    {
        const auto& strip_ = ragnarokGrid[index_];
        const float yaw_ = DirectX::XMConvertToRadians(strip_.yaw);
        const DirectX::XMFLOAT3 direction_ = {std::sin(yaw_)*strip_.length*0.5f,0,std::cos(yaw_)*strip_.length*0.5f};
        const DirectX::XMFLOAT3 start_ = {strip_.position.x-direction_.x,0,strip_.position.z-direction_.z};
        const DirectX::XMFLOAT3 end_ = {strip_.position.x+direction_.x,0,strip_.position.z+direction_.z};
        DirectX::XMFLOAT3 worldStart_{};
        DirectX::XMFLOAT3 worldEnd_{};
        DirectX::XMStoreFloat3(&worldStart_,DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&start_),world_));
        DirectX::XMStoreFloat3(&worldEnd_,DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&end_),world_));
        SpawnElectricStrip(worldStart_, worldEnd_, strip_.length, ragnarokParticleSweepTime);
    }
}

void HumanBossActionComponent::UpdateRagnarokParticles(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::UpdateRagnarokParticles");
    for (auto& sweep_ : ragnarokParticleSweeps)
    {
        const auto object_ = sweep_.object.lock();
        const auto particle_ = object_ ? object_->GetComponent<ParticleComponent>() : nullptr;
        if (!particle_ || object_->IsPendingDestroy()) continue;
        // 初回更新で終了時間を跨いでも、最低一度は通常のParticle更新へBurstを渡す。
        if (sweep_.time >= sweep_.duration) { particle_->Stop(); continue; }
        const float previousProgress_ = std::clamp(sweep_.time/(std::max)(sweep_.duration,0.01f),0.0f,1.0f);
        sweep_.time += (std::max)(deltaTime_,0.0f);
        const float progress_ = std::clamp(sweep_.time/(std::max)(sweep_.duration,0.01f),0.0f,1.0f);
        const auto sample_ = [&](float rate_)
        {
            return DirectX::XMFLOAT3{sweep_.start.x+(sweep_.end.x-sweep_.start.x)*rate_,
                sweep_.start.y+(sweep_.end.y-sweep_.start.y)*rate_,sweep_.start.z+(sweep_.end.z-sweep_.start.z)*rate_};
        };
        // 前フレームから進んだ区間を共有し、低FPSでも途中の線分を飛ばさない。
        particle_->SetEmitterLine("GridElectricSweep",sample_(previousProgress_),sample_(progress_));
        particle_->SetEmitterLine("GridMudSweep",sample_(previousProgress_),sample_(progress_));
    }
    ragnarokParticleSweeps.erase(std::remove_if(ragnarokParticleSweeps.begin(),ragnarokParticleSweeps.end(),
        [](const RagnarokParticleSweep& sweep_){ const auto object_ = sweep_.object.lock(); return !object_ || object_->IsPendingDestroy(); }),ragnarokParticleSweeps.end());
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
    danger_.shapeType = DangerAreaShapeType::CAPSULE;
    danger_.materialPath = shadowDangerMaterial;
    danger_.length = (std::min)(ragnarokRadius, rangedStageRadius);
    danger_.fanAngleDegrees = ragnarokAngle;
    danger_.lifeTime = execution.GetCurrentPhase() ? execution.GetCurrentPhase()->duration + 0.05f : 1.5f;
    DirectX::XMFLOAT3 ground_{};
    if (!ResolveRangedGround(rangedStageCenter, ground_)) { status = "Ragnarok warning ground unavailable"; Stop(); return; }
    ragnarokGroundCenter = ground_;
    ragnarokWarningDuration = execution.GetCurrentPhase() ? execution.GetCurrentPhase()->duration : ragnarokRotationTime;
    BuildRagnarokGrid();
    rangedDanger = owner_->GetOrAddComponent<DangerAreaComponent>()->ShowAt(danger_, ragnarokGroundCenter, ragnarokYaw);
    if (rangedDanger.expired()) { status = "Ragnarok warning creation failed"; Stop(); return; }
    const auto warning_ = rangedDanger.lock();
    const auto mesh_ = warning_ ? warning_->GetComponent<MeshVFXComponent>() : nullptr;
    if (!mesh_ || mesh_->GetAsset().parts.empty()) { status = "Ragnarok warning mesh unavailable"; Stop(); return; }
    if (mesh_)
    {
        auto asset_ = mesh_->GetAsset();
        const auto basePart_ = asset_.parts.front();
        asset_.parts.clear();
        for (const auto& strip_ : ragnarokGrid)
        {
            auto part_ = basePart_;
            part_.modelPath = "__InternalMeshDangerBox__";
            part_.position = strip_.position;
            part_.rotation = { 0.0f, strip_.yaw, 0.0f };
            part_.shapeCylinderRadius = strip_.halfWidth;
            part_.internalMeshSettings.capsuleLength = strip_.length;
            asset_.parts.push_back(part_);
        }
        mesh_->SetAsset(asset_);
        mesh_->Reset();
        mesh_->Play();
    }
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
    // 格子の帯数に関係なく、攻撃一回につき中心位置で発動音を鳴らす。
    const auto& feedback_ = attack_->feedback;
    AudioManager::Instance().PlaySpatialAttackSound(feedback_.startSoundPath, ragnarokGroundCenter,
        feedback_.volume, feedback_.minDistance, feedback_.maxDistance);
    SpawnRagnarokParticles();
    // 最後に表示した予兆の角度を引き継ぎ、Phase切替時の角度の飛びを防ぐ。
    ClearRangedArea();
    const auto instance_ = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
    for (auto effect_ : attack_->attackEffects)
    {
        const auto baseShape_ = effect_.hitShapes.empty() ? AttackEffectHitShapeTemplate{} : effect_.hitShapes.front();
        effect_.hitShapes.clear();
        for (const auto& strip_ : ragnarokGrid)
        {
            auto shape_ = baseShape_;
            shape_.shape = ATTACK_HIT_SHAPE::BOX;
            shape_.localOffset = { strip_.position.x, baseShape_.localOffset.y, strip_.position.z };
            shape_.localRotation = { 0.0f, strip_.yaw, 0.0f };
            shape_.localScale = { 1.0f, 1.0f, 1.0f };
            shape_.radius = strip_.halfWidth;
            shape_.range = strip_.length;
            shape_.activeStartTime = 0.0f;
            shape_.activeEndTime = ragnarokHitDuration;
            effect_.hitShapes.push_back(shape_);
        }
        effect_.followOwner = false;
        effect_.followNode = false;
        effect_.localScale = { 1.0f, 1.0f, 1.0f };
        effect_.lifeTime = ragnarokHitDuration + 0.25f;
        const auto object_ = owner_->GetManager().GetEffectManager().SpawnAttackEffect(effect_, *attack_, owner_.get(), instance_);
        if (object_ && object_->transform)
        {
            // 同じ格子の全帯で攻撃IDを共有し、交差部分でダメージを重複させない。
            object_->transform->ApplyWorldMatrix(DirectX::XMMatrixRotationY(DirectX::XMConvertToRadians(ragnarokYaw)) *
                DirectX::XMMatrixTranslation(ragnarokGroundCenter.x, ragnarokGroundCenter.y, ragnarokGroundCenter.z));
            object_->UpdateTransform();
            if (const auto mesh_ = object_->GetComponent<MeshVFXComponent>())
            {
                auto asset_ = mesh_->GetAsset();
                const auto baseParts_ = asset_.parts;
                asset_.parts.clear();
                for (const auto& strip_ : ragnarokGrid)
                {
                    for (auto part_ : baseParts_)
                    {
                        part_.modelPath = "__InternalMeshDangerBox__";
                        part_.position = { strip_.position.x, part_.position.y, strip_.position.z };
                        part_.rotation = { 0.0f, strip_.yaw, 0.0f };
                        part_.scale = { 1.0f, 1.0f, 1.0f };
                        part_.animateScale = false;
                        part_.meshRotationSpeed = { 0.0f, 0.0f, 0.0f };
                        part_.shapeCylinderRadius = strip_.halfWidth;
                        part_.internalMeshSettings.capsuleLength = strip_.length;
                        part_.duration = effect_.lifeTime;
                        asset_.parts.push_back(part_);
                    }
                }
                mesh_->SetAsset(asset_);
                mesh_->Reset();
                mesh_->Play();
            }
        }
    }
}

void HumanBossActionComponent::ClearRangedArea()
{
    StopChargeHit();
    if (const auto owner_ = GetGameObject(); owner_ && owner_->manager)
        if (const auto danger_ = owner_->GetComponent<DangerAreaComponent>()) danger_->StopAll();
    rangedDanger.reset();
}

void HumanBossActionComponent::StopChargeHit()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::StopChargeHit");
    if (const auto object_ = chargeHit.lock())
    {
        if (const auto hit_ = object_->GetComponent<AttackEffectHitComponent>()) hit_->Stop();
        object_->GetManager().Destroy(object_);
    }
    chargeHit.reset();
}

void HumanBossActionComponent::DrawRangedGui()
{
    BS_PROFILE_SCOPE("HumanBossActionComponent::DrawRangedGui");
    if (!ImGui::TreeNode("Ranged / Special attacks")) return;
    ImGui::DragFloat("ExecutionHoldTime", &executionHoldTime, 0.01f, 0.1f, 10.0f);
    ImGui::DragFloat("ExecutionTargetDistance", &executionTargetDistance, 0.05f, 0.8f, 4.0f);
    ImGui::DragFloat("ChargeMaximumDistance", &chargeMaximumDistance, 0.05f, 1.0f, 40.0f);
    ImGui::DragFloat("ChargeDistanceMultiplier", &chargeDistanceMultiplier, 0.05f, 1.0f, 2.0f);
    ImGui::DragFloat("ChargeElectricDuration", &chargeElectricDuration, 0.05f, 0.1f, 3.0f);
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
    ImGui::DragFloat("OrbOrbitRadius", &orbOrbitRadius, 0.05f, 1.0f, 8.0f);
    ImGui::DragFloat("OrbOrbitTilt", &orbOrbitTilt, 0.5f, -65.0f, 65.0f);
    ImGui::DragFloat("OrbOrbitSpeed", &orbOrbitSpeed, 1.0f, -360.0f, 360.0f);
    ImGui::DragFloat("OrbOrbitDuration", &orbOrbitDuration, 0.05f, 0.5f, 5.0f);
    ImGui::DragFloat("OrbLaunchSpeedMultiplier", &orbLaunchSpeedMultiplier, 0.1f, 3.0f, 5.0f);
    ImGui::DragFloat("RagnarokHpThreshold", &ragnarokHpThreshold, 0.05f, 0.01f, 1.0f);
    ImGui::DragInt("RagnarokAttackCount", &ragnarokAttackCount, 1.0f, 1, 8);
    ImGui::DragFloat("RagnarokAdvanceSpeed", &ragnarokAdvanceSpeed, 0.1f, 1.0f, 16.0f);
    ImGui::DragFloat("RagnarokAdvanceMultiplier", &ragnarokAdvanceMultiplier, 0.1f, 1.0f, 8.0f);
    ImGui::DragFloat("RagnarokRadius", &ragnarokRadius, 0.05f, 1.0f, 100.0f);
    ImGui::DragFloat("RagnarokAngle", &ragnarokAngle, 0.05f, 1.0f, 360.0f);
    ImGui::DragInt("RagnarokGridLineCount", &ragnarokGridLineCount, 1.0f, 3, 12);
    ImGui::DragFloat("RagnarokGridWidth", &ragnarokGridWidth, 0.1f, 0.2f, 20.0f);
    ImGui::DragFloat("RagnarokGridSpacing", &ragnarokGridSpacing, 0.1f, 0.5f, 40.0f);
    ImGui::DragFloat("RagnarokRotationSpeed", &ragnarokRotationSpeed, 0.05f, -360.0f, 360.0f);
    ImGui::DragFloat("RagnarokRotationTime", &ragnarokRotationTime, 0.05f, 0.1f, 5.0f);
    ImGui::DragFloat("RagnarokHitDuration", &ragnarokHitDuration, 0.05f, 0.05f, 1.0f);
    ImGui::DragFloat("RagnarokTellTime", &ragnarokTellTime, 0.05f, 0.5f, 5.0f);
    ImGui::DragFloat("RagnarokRecoveryTime", &ragnarokRecoveryTime, 0.05f, 0.5f, 5.0f);
    ImGui::DragFloat("GridParticleSweepTime", &ragnarokParticleSweepTime, 0.01f, 0.05f, 0.3f);
    ImGui::DragFloat("GridParticleDensity", &ragnarokParticleDensity, 0.05f, 0.1f, 1.0f);
    ImGui::DragFloat("ElectricDensityMultiplier", &electricDensityMultiplier, 0.1f, 1.0f, 5.0f);
    ImGui::DragInt("GridParticleLineLimit", &ragnarokParticleLineLimit, 1, 2, 24);
    ImGui::InputText("GridParticleAsset", &ragnarokParticleAsset);
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
    json_["ChargeDistanceMultiplier"] = chargeDistanceMultiplier;
    json_["ChargeElectricDuration"] = chargeElectricDuration;
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
    json_["OrbOrbitRadius"] = orbOrbitRadius;
    json_["OrbOrbitTilt"] = orbOrbitTilt;
    json_["OrbOrbitSpeed"] = orbOrbitSpeed;
    json_["OrbOrbitDuration"] = orbOrbitDuration;
    json_["OrbLaunchSpeedMultiplier"] = orbLaunchSpeedMultiplier;
    json_["RagnarokHpThreshold"] = ragnarokHpThreshold;
    json_["RagnarokAttackCount"] = ragnarokAttackCount;
    json_["RagnarokAdvanceSpeed"] = ragnarokAdvanceSpeed;
    json_["RagnarokAdvanceMultiplier"] = ragnarokAdvanceMultiplier;
    json_["RagnarokRadius"] = ragnarokRadius;
    json_["RagnarokAngle"] = ragnarokAngle;
    json_["RagnarokGridLineCount"] = ragnarokGridLineCount;
    json_["RagnarokGridWidth"] = ragnarokGridWidth;
    json_["RagnarokGridSpacing"] = ragnarokGridSpacing;
    json_["RagnarokRotationSpeed"] = ragnarokRotationSpeed;
    json_["RagnarokRotationTime"] = ragnarokRotationTime;
    json_["RagnarokHitDuration"] = ragnarokHitDuration;
    json_["RagnarokTellTime"] = ragnarokTellTime;
    json_["RagnarokRecoveryTime"] = ragnarokRecoveryTime;
    json_["GridParticleSweepTime"] = ragnarokParticleSweepTime;
    json_["GridParticleDensity"] = ragnarokParticleDensity;
    json_["ElectricDensityMultiplier"] = electricDensityMultiplier;
    json_["GridParticleLineLimit"] = ragnarokParticleLineLimit;
    json_["GridParticleAsset"] = ragnarokParticleAsset;
    json_["RangedStageRadius"] = rangedStageRadius;
    json_["ChargeGrowthCurve"] = chargeGrowthCurve;
    json_["RangedAttackSoundPath"] = rangedAttackSoundPath;
    json_["RagnarokEnabled"] = ragnarokEnabled;
    json_["RangedStageCenter"] = { rangedStageCenter.x, rangedStageCenter.y, rangedStageCenter.z };
}

void HumanBossActionComponent::DeserializeRanged(const nlohmann::json& json_)
{
    chargeMaximumDistance = std::clamp(json_.value("ChargeMaximumDistance", chargeMaximumDistance), 1.0f, 40.0f);
    chargeDistanceMultiplier = std::clamp(json_.value("ChargeDistanceMultiplier", chargeDistanceMultiplier), 1.0f, 2.0f);
    chargeElectricDuration = std::clamp(json_.value("ChargeElectricDuration", chargeElectricDuration), 0.1f, 3.0f);
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
    orbOrbitRadius = std::clamp(json_.value("OrbOrbitRadius", orbOrbitRadius), 1.0f, 8.0f);
    orbOrbitTilt = std::clamp(json_.value("OrbOrbitTilt", orbOrbitTilt), -65.0f, 65.0f);
    orbOrbitSpeed = std::clamp(json_.value("OrbOrbitSpeed", orbOrbitSpeed), -360.0f, 360.0f);
    orbOrbitDuration = std::clamp(json_.value("OrbOrbitDuration", orbOrbitDuration), 0.5f, 5.0f);
    orbLaunchSpeedMultiplier = std::clamp(json_.value("OrbLaunchSpeedMultiplier", orbLaunchSpeedMultiplier), 3.0f, 5.0f);
    ragnarokHpThreshold = std::clamp(json_.value("RagnarokHpThreshold", ragnarokHpThreshold), 0.01f, 1.0f);
    ragnarokAttackCount = std::clamp(json_.value("RagnarokAttackCount", ragnarokAttackCount), 1, 8);
    ragnarokAdvanceSpeed = std::clamp(json_.value("RagnarokAdvanceSpeed", ragnarokAdvanceSpeed), 1.0f, 16.0f);
    ragnarokAdvanceMultiplier = std::clamp(json_.value("RagnarokAdvanceMultiplier", ragnarokAdvanceMultiplier), 1.0f, 8.0f);
    ragnarokRadius = std::clamp(json_.value("RagnarokRadius", ragnarokRadius), 1.0f, 100.0f);
    ragnarokAngle = std::clamp(json_.value("RagnarokAngle", ragnarokAngle), 1.0f, 360.0f);
    ragnarokGridLineCount = std::clamp(json_.value("RagnarokGridLineCount", ragnarokGridLineCount), 3, 12);
    ragnarokGridWidth = std::clamp(json_.value("RagnarokGridWidth", ragnarokGridWidth), 0.2f, 20.0f);
    ragnarokGridSpacing = std::clamp(json_.value("RagnarokGridSpacing", ragnarokGridSpacing), 0.5f, 40.0f);
    ragnarokRotationSpeed = std::clamp(json_.value("RagnarokRotationSpeed", ragnarokRotationSpeed), -360.0f, 360.0f);
    ragnarokRotationTime = std::clamp(json_.value("RagnarokRotationTime", ragnarokRotationTime), 0.1f, 5.0f);
    ragnarokHitDuration = std::clamp(json_.value("RagnarokHitDuration", ragnarokHitDuration), 0.05f, 1.0f);
    ragnarokTellTime = std::clamp(json_.value("RagnarokTellTime",ragnarokTellTime),0.5f,5.0f);
    ragnarokRecoveryTime = std::clamp(json_.value("RagnarokRecoveryTime",ragnarokRecoveryTime),0.5f,5.0f);
    ragnarokParticleSweepTime = std::clamp(json_.value("GridParticleSweepTime",ragnarokParticleSweepTime),0.05f,0.3f);
    ragnarokParticleDensity = std::clamp(json_.value("GridParticleDensity",ragnarokParticleDensity),0.1f,1.0f);
    electricDensityMultiplier = std::clamp(json_.value("ElectricDensityMultiplier", electricDensityMultiplier), 1.0f, 5.0f);
    ragnarokParticleLineLimit = std::clamp(json_.value("GridParticleLineLimit",ragnarokParticleLineLimit),2,24);
    ragnarokParticleAsset = json_.value("GridParticleAsset",ragnarokParticleAsset);
    rangedStageRadius = std::clamp(json_.value("RangedStageRadius", rangedStageRadius), 5.0f, 100.0f);
    orbCount = (std::min)(orbCount, orbMaximumCount);
    orbLifeTime = (std::max)(orbLifeTime, (std::max)(orbWaitTime, orbOrbitDuration) + (orbCount - 1) * orbInterval + 1.0f);
    chargeGrowthCurve = json_.value("ChargeGrowthCurve", chargeGrowthCurve);
    rangedAttackSoundPath = json_.value("RangedAttackSoundPath", rangedAttackSoundPath);
    ragnarokEnabled = json_.value("RagnarokEnabled", ragnarokEnabled);
    if (json_.contains("RangedStageCenter") && json_["RangedStageCenter"].is_array() && json_["RangedStageCenter"].size() == 3)
        rangedStageCenter = { json_["RangedStageCenter"][0].get<float>(), json_["RangedStageCenter"][1].get<float>(), json_["RangedStageCenter"][2].get<float>() };
}
