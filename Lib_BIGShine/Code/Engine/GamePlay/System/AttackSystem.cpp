#include "PrecompiledHeader.h"
#include "Game/AI/Human/HumanBossAIComponent.h"

#include "AttackSystem.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Combat/CombatStatsComponent.h"
#include "Engine/GamePlay/Camera/CameraShakeComponent.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/HurtBox.h"
#include "Game/GameMode/GameModeManager.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool TryReadAttackerAttackPower(
        const std::shared_ptr<GameObject>& attacker_,
        float& attackPower_)
    {
        if (!attacker_)
        {
            return false;
        }
        if (const std::shared_ptr<CombatStatsComponent> stats_ =
            attacker_->GetComponent<CombatStatsComponent>())
        {
            attackPower_ = stats_->GetAttackPower();
            return true;
        }
        if (const std::shared_ptr<BlackboardComponent> blackboard_ =
            attacker_->GetComponent<BlackboardComponent>())
        {
            if (blackboard_->GetFloat("Combat", "AttackPower", attackPower_))
            {
                attackPower_ = (std::max)(attackPower_, 0.0f);
                return true;
            }
        }
        return false;
    }

    float ReadAttackerDamageRateModifier(const std::shared_ptr<GameObject>& attacker_)
    {
        if (!attacker_)
        {
            return 1.0f;
        }
        if (const std::shared_ptr<CombatStatsComponent> stats_ =
            attacker_->GetComponent<CombatStatsComponent>())
        {
            return stats_->GetDamageRateModifier();
        }
        if (const std::shared_ptr<BlackboardComponent> blackboard_ =
            attacker_->GetComponent<BlackboardComponent>())
        {
            float rate_ = 1.0f;
            if (blackboard_->GetFloat("Combat", "DamageRateModifier", rate_))
            {
                return (std::max)(rate_, 0.0f);
            }
        }
        return 1.0f;
    }

    float ReadAttackerFixedDamageModifier(const std::shared_ptr<GameObject>& attacker_)
    {
        if (!attacker_)
        {
            return 0.0f;
        }
        if (const std::shared_ptr<CombatStatsComponent> stats_ =
            attacker_->GetComponent<CombatStatsComponent>())
        {
            return stats_->GetFixedDamageModifier();
        }
        if (const std::shared_ptr<BlackboardComponent> blackboard_ =
            attacker_->GetComponent<BlackboardComponent>())
        {
            float fixedDamage_ = 0.0f;
            if (blackboard_->GetFloat("Combat", "FixedDamageModifier", fixedDamage_))
            {
                return fixedDamage_;
            }
        }
        return 0.0f;
    }

    float CalculateDamage(
        const std::shared_ptr<GameObject>& attacker_,
        const HitBox& hitBox_,
        const HurtBox& hurtBox_,
        float& attackerAttackPower_)
    {
        // CombatStatsComponentがある攻撃者は「攻撃力×技倍率」、ない場合は旧Damageを直接使う。
        float rawDamage_ = hitBox_.damage;
        attackerAttackPower_ = 0.0f;
        if (TryReadAttackerAttackPower(attacker_, attackerAttackPower_))
        {
            const float rateModifier_ = ReadAttackerDamageRateModifier(attacker_);
            rawDamage_ =
                attackerAttackPower_ * (std::max)(hitBox_.damageRate, 0.0f) * rateModifier_;
        }
        rawDamage_ += hitBox_.fixedDamage + ReadAttackerFixedDamageModifier(attacker_);
        return (std::max)(rawDamage_, 0.0f) * hurtBox_.damageMultiplier;
    }

    bool IsPlayerObject(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_)
        {
            return false;
        }
        if (object_->GetLayer() == LayerType::Player)
        {
            return true;
        }

        std::shared_ptr<GameObject> current_ = object_;
        while (current_)
        {
            const std::string& name_ = current_->GetName();
            if (name_.find("Player") != std::string::npos)
            {
                return true;
            }
            if (!current_->transform)
            {
                break;
            }

            const std::shared_ptr<Transform> parentTransform_ =
                current_->transform->parent.lock();
            if (!parentTransform_)
            {
                break;
            }
            current_ = parentTransform_->gameObject.lock();
        }
        return false;
    }
}

void AttackSystem::BeginFrame()
{
    BS_PROFILE_SCOPE("AttackSystem::BeginFrame");
    BS_PROFILE_PLOT("AttackSystem::FrameHitEventCount", frameHitEvents.size());

    frameHitEvents.clear();
}

float AttackSystem::CalculateGuardDamage(HitBox& hitBox_, const HurtBox& hurtBox_)
{
    float attackPower_ = 0.0f;
    return CalculateDamage(hitBox_.GetAttackOwner(), hitBox_, hurtBox_, attackPower_);
}

void AttackSystem::Clear()
{
    BS_PROFILE_SCOPE("AttackSystem::Clear");

    frameHitEvents.clear();
    registeredHitTargets.clear();
}

uint32_t AttackSystem::CreateAttackInstance()
{
    BS_PROFILE_SCOPE("AttackSystem::CreateAttackInstance");

    // 1回の攻撃単位を識別するID。多段ヒットや同一対象への再ヒット制御に使う。
    const uint32_t attackInstanceId_ = nextAttackInstanceId++;
    if (nextAttackInstanceId == 0u)
    {
        nextAttackInstanceId = 1u;
    }
    return attackInstanceId_;
}

void AttackSystem::ClearAttackInstance(uint32_t attackInstanceId_)
{
    BS_PROFILE_SCOPE("AttackSystem::ClearAttackInstance");

    if (attackInstanceId_ == 0u)
    {
        return;
    }

    // 攻撃が終わったら、その攻撃IDに紐づくヒット履歴をまとめて破棄する。
    for (auto iterator_ = registeredHitTargets.begin(); iterator_ != registeredHitTargets.end();)
    {
        if (static_cast<uint32_t>(iterator_->first >> 32u) == attackInstanceId_)
        {
            iterator_ = registeredHitTargets.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }
}

void AttackSystem::ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_)
{
    BS_PROFILE_SCOPE("AttackSystem::ClearAttackHitGroup");

    if (attackInstanceId_ == 0u)
    {
        return;
    }
    const uint64_t hitKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);
    registeredHitTargets.erase(hitKey_);
}

void AttackSystem::HandleHit(HitBox& hitBox_, HurtBox& hurtBox_)
{
    AttackHitContact contact_{};
    HandleHit(hitBox_, hurtBox_, contact_);
}

void AttackSystem::HandleHit(
    HitBox& hitBox_,
    HurtBox& hurtBox_,
    const AttackHitContact& contact_)
{
    BS_PROFILE_SCOPE("AttackSystem::HandleHit");

    // HitBox/HurtBoxの状態、同一階層、ガード済み履歴などを通過したものだけHitEvent化する。
    const std::shared_ptr<GameObject> hitBoxObject_ = hitBox_.GetGameObject();
    const std::shared_ptr<GameObject> attacker_ = hitBox_.GetAttackOwner();
    const std::shared_ptr<GameObject> hurtBoxObject_ = hurtBox_.GetGameObject();
    std::shared_ptr<GameObject> target_ = hurtBox_.ResolveCollisionOwnerObject();
    if (!target_)
    {
        target_ = hurtBoxObject_;
    }
    if (!hitBoxObject_ || !attacker_ || !target_ || !hurtBoxObject_)
    {
        return;
    }
    // 撃破直後に残ったProjectileも含め、シネマティック中の通常ダメージを受け付けない。
    if (attacker_->manager)
    {
        const auto mode_ = attacker_->GetManager().GetGameModeManager();
        if (mode_ && mode_->IsPresentationPlaying())
        {
            return;
        }
    }
    if (!hitBox_.canHitOwner && IsSameHierarchy(attacker_, target_))
    {
        return;
    }
    if (!hitBox_.IsEnabled() || !hurtBox_.IsEnabled())
    {
        return;
    }
    // 暴走の無敵はヒット成立前に判定し、HPだけでなくHitStop・被弾Action・押し出しも発生させない。
    // 防御側プレイヤーのジャストガード通知と、スタン中のとどめ受付には影響しない。
    if (const auto bossAI_ = target_->GetComponent<HumanBossAIComponent>(); bossAI_ && bossAI_->IsBerserkInvulnerable())
    {
        return;
    }
    if (!hitBox_.CanHit(target_.get()))
    {
        return;
    }
    if (!hurtBox_.CanReceiveHit(hitBox_))
    {
        return;
    }
    if (const auto bossAI_ = target_->GetComponent<HumanBossAIComponent>(); bossAI_ &&
        (hitBox_.damage > 0.0f || hitBox_.damageRate > 0.0f || hitBox_.fixedDamage > 0.0f) && bossAI_->TryBeginFinalFinish(attacker_))
    {
        return;
    }
    if (!RegisterAttackHit(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        target_.get(),
        hitBox_.allowMultiHit))
    {
        return;
    }

    // ここから先は「ヒット成立」。反応ComponentやVFXが使う情報をAttackHitEventへ詰める。
    hitBox_.RegisterHit(target_.get());
    hurtBox_.RecordHit(hitBox_);

    AttackHitEvent event_;
    event_.attacker = attacker_;
    event_.target = target_;
    event_.hitBox = &hitBox_;
    event_.hurtBox = &hurtBox_;
    event_.damage = CalculateDamage(attacker_, hitBox_, hurtBox_, event_.attackerAttackPower);
    event_.damageRate = hitBox_.damageRate;
    event_.fixedDamage = hitBox_.fixedDamage;
    event_.damageApplyType = hitBox_.damageApplyType;
    event_.damageHitInterval = hitBox_.damageHitInterval;
    event_.maxHitCount = hitBox_.maxHitCount;
    event_.hitStopDuration = hitBox_.hitStopDuration;
    event_.hitStopScale = hitBox_.hitStopScale;
    event_.hitStopLayerMask = hitBox_.hitStopLayerMask;
    event_.impactPower = hitBox_.impactPower;
    event_.knockBackPower = hitBox_.knockBackPower;
    event_.targetAirLiftPower = hitBox_.targetAirLiftPower;
    event_.attackerAirLiftPower = hitBox_.attackerAirLiftPower;
    event_.hitPosition = contact_.position;
    event_.hitNormal = contact_.normal;
    event_.hasHitContact = contact_.isValid;
    event_.attackInstanceId = hitBox_.attackInstanceId;
    event_.hitGroupId = hitBox_.hitGroupId;
    event_.sourceAction = hitBox_.sourceAction;
    event_.teamId = hitBox_.teamId;
    event_.attackTag = hitBox_.attackTag;
    event_.hurtTag = hurtBox_.hurtTag;
    event_.hitParticleAssetPath = hitBox_.hitParticleAssetPath;
    if (event_.damage > 0.0f && event_.hitParticleAssetPath.empty() && hitBox_.hitParticleEmitterName.empty() &&
        hitBox_.hitImpactEffectAssetPath.empty() && (IsPlayerObject(target_) || IsPlayerObject(attacker_)))
    {
        event_.hitParticleAssetPath = "Data/Particle/LargeHitSparks_Weak.json";
    }
    event_.hitParticleEmitterName = hitBox_.hitParticleEmitterName;
    event_.hitParticleLifeTime = hitBox_.hitParticleLifeTime;
    event_.hitParticleNormalOffset = hitBox_.hitParticleNormalOffset;
    event_.hitParticleUseImpactAxis = hitBox_.hitParticleUseImpactAxis;
    event_.hitParticleAxisToAttackerBlend = hitBox_.hitParticleAxisToAttackerBlend;
    event_.hitParticleUseTargetAttraction = hitBox_.hitParticleUseTargetAttraction;
    event_.hitParticleTargetAttractionStrength = hitBox_.hitParticleTargetAttractionStrength;
    event_.hitParticleTargetAttractionRange = hitBox_.hitParticleTargetAttractionRange;
    event_.hitParticleTargetAttractionFalloff = hitBox_.hitParticleTargetAttractionFalloff;
    event_.hitMeshVFXAssetPath = hitBox_.hitMeshVFXAssetPath;
    event_.hitMeshVFXLifeTime = hitBox_.hitMeshVFXLifeTime;
    event_.hitMeshVFXNormalOffset = hitBox_.hitMeshVFXNormalOffset;
    event_.hitImpactEffectAssetPath = hitBox_.hitImpactEffectAssetPath;
    // テンプレートに音声が指定された場合だけ優先し、既存HitBoxの音声設定も維持する。
    event_.hitSoundPath = hitBox_.feedback.hitSoundPath.empty() ? hitBox_.hitSoundPath : hitBox_.feedback.hitSoundPath;
    event_.hitSoundVolume = hitBox_.feedback.hitSoundPath.empty() ? hitBox_.hitSoundVolume : hitBox_.feedback.volume;
    event_.hitSoundMuffled = hitBox_.hitSoundMuffled;
    event_.hitSoundMuffledCutoff = hitBox_.hitSoundMuffledCutoff;
    event_.feedback = hitBox_.feedback;
    event_.cameraShakeEnabled = hitBox_.cameraShakeEnabled;
    event_.cameraShakeReceiveDuration = hitBox_.cameraShakeReceiveDuration;
    event_.cameraShakeReceivePositionStrength =
        hitBox_.cameraShakeReceivePositionStrength;
    event_.cameraShakeReceiveRotationStrength =
        hitBox_.cameraShakeReceiveRotationStrength;
    event_.cameraShakeAttackScale = hitBox_.cameraShakeAttackScale;
    event_.cameraShakeFrequency = hitBox_.cameraShakeFrequency;
    event_.cameraShakePriority = hitBox_.cameraShakePriority;

    frameHitEvents.push_back(event_);
    // 受け手のBlackboardとCameraShakeへ公開し、Health/HitReactionなどはAction側で処理する。
    PublishHitToBlackboard(event_);
    PlayCameraShake(event_);
    // 命中が確定した経路だけで振動し、敵同士の接触ではPadへ通知しない。
    if (event_.feedback.rumbleEnabled && (IsPlayerObject(target_) || IsPlayerObject(attacker_)))
    {
        const float strength_ = IsPlayerObject(target_)
            ? event_.feedback.receiveStrength : event_.feedback.attackStrength;
        // 既存の技ごとの強弱と無効設定を保ち、命中・被弾の両モーターを強める。
        if (strength_ > 0.0f && event_.feedback.duration > 0.0f)
        {
            const bool receiving_ = IsPlayerObject(target_);
            const float duration_ = (std::max)(event_.feedback.duration * 1.25f, receiving_ ? 0.18f : 0.12f);
            ControllerInput::Instance().PlayRumble(strength_ * (receiving_ ? 2.1f : 1.9f), strength_ * 1.25f,
                duration_, receiving_ ? 3 : 2);
        }
    }

    if (event_.attackerAirLiftPower > 0.0f)
    {
        float attackerMassScale_ = 1.0f;
        if (const std::shared_ptr<ColliderComponent> collider_ =
            attacker_->GetComponent<ColliderComponent>())
        {
            attackerMassScale_ = 1.0f / (std::max)(collider_->GetMass(), 1.0f);
        }

        const float airLiftImpulse_ = event_.attackerAirLiftPower * attackerMassScale_;
        if (const std::shared_ptr<CharacterMovementComponent> movement_ =
            attacker_->GetComponent<CharacterMovementComponent>())
        {
            movement_->ResetVerticalVelocity();
            movement_->AddImpulse({ 0.0f, airLiftImpulse_, 0.0f });
        }
        else if (const std::shared_ptr<ColliderComponent> collider_ =
            attacker_->GetComponent<ColliderComponent>())
        {
            collider_->velocity.y = airLiftImpulse_;
        }
    }

    for (const HitCallback& callback_ : hitCallbacks)
    {
        if (callback_)
        {
            callback_(event_);
        }
    }
}

bool AttackSystem::RegisterAttackHit(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_,
    const GameObject* target_,
    bool allowMultiHit_)
{
    BS_PROFILE_SCOPE("AttackSystem::RegisterAttackHit");

    if (allowMultiHit_ || attackInstanceId_ == 0u || target_ == nullptr)
    {
        return true;
    }

    // attackInstanceId + hitGroupIdごとに対象を記録し、同じ攻撃の重複ヒットを防ぐ。
    const uint64_t hitKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);
    return registeredHitTargets[hitKey_].insert(target_).second;
}

void AttackSystem::AddHitCallback(HitCallback callback_)
{
    if (callback_)
    {
        hitCallbacks.push_back(std::move(callback_));
    }
}

bool AttackSystem::IsSameHierarchy(
    const std::shared_ptr<GameObject>& objectA_,
    const std::shared_ptr<GameObject>& objectB_) const
{
    return GetHierarchyRoot(objectA_) == GetHierarchyRoot(objectB_);
}

std::shared_ptr<GameObject> AttackSystem::GetHierarchyRoot(
    std::shared_ptr<GameObject> object_) const
{
    while (object_ && object_->transform)
    {
        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_)
        {
            break;
        }

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_)
        {
            break;
        }
        object_ = parentObject_;
    }
    return object_;
}

void AttackSystem::PublishHitToBlackboard(const AttackHitEvent& event_)
{
    const std::shared_ptr<GameObject> target_ = event_.target.lock();
    if (!target_)
    {
        return;
    }

    // HurtBoxが子Objectにある場合も、親のBlackboardへヒット情報を届けられるよう上へたどる。
    std::shared_ptr<GameObject> receiver_ = target_;
    std::shared_ptr<BlackboardComponent> blackboard_;
    while (receiver_)
    {
        blackboard_ = receiver_->GetComponent<BlackboardComponent>();
        if (blackboard_ || !receiver_->transform)
        {
            break;
        }

        const std::shared_ptr<Transform> parentTransform_ =
            receiver_->transform->parent.lock();
        if (!parentTransform_)
        {
            break;
        }
        receiver_ = parentTransform_->gameObject.lock();
    }

    if (!receiver_)
    {
        return;
    }
    if (!blackboard_)
    {
        blackboard_ = receiver_->GetOrAddComponent<BlackboardComponent>();
    }
    if (!blackboard_)
    {
        return;
    }

    blackboard_->SetFloat("Hit", "Damage", event_.damage);
    blackboard_->SetFloat("Hit", "AttackerAttackPower", event_.attackerAttackPower);
    blackboard_->SetFloat("Hit", "DamageRate", event_.damageRate);
    blackboard_->SetFloat("Hit", "FixedDamage", event_.fixedDamage);
    blackboard_->SetInt("Hit", "DamageApplyType", static_cast<int>(event_.damageApplyType));
    blackboard_->SetFloat("Hit", "DamageHitInterval", event_.damageHitInterval);
    blackboard_->SetInt("Hit", "MaxHitCount", event_.maxHitCount);
    blackboard_->SetFloat("Hit", "HitStopDuration", event_.hitStopDuration);
    blackboard_->SetFloat("Hit", "HitStopScale", event_.hitStopScale);
    blackboard_->SetFloat("Hit", "ImpactPower", event_.impactPower);
    blackboard_->SetFloat("Hit", "KnockBackPower", event_.knockBackPower);
    blackboard_->SetFloat("Hit", "TargetAirLiftPower", event_.targetAirLiftPower);
    blackboard_->SetFloat("Hit", "AttackerAirLiftPower", event_.attackerAirLiftPower);
    blackboard_->SetVector3("Hit", "Position", event_.hitPosition);
    blackboard_->SetVector3("Hit", "Normal", event_.hitNormal);
    const std::shared_ptr<GameObject> hitBoxObject_ =
        event_.hitBox ? event_.hitBox->GetGameObject() : nullptr;
    const std::shared_ptr<GameObject> hurtBoxObject_ =
        event_.hurtBox ? event_.hurtBox->GetGameObject() : target_;
    if (hitBoxObject_ && hurtBoxObject_ && event_.hitBox && event_.hurtBox)
    {
        const Capsule hitCapsule_ = event_.hitBox->GetCapsuleData();
        const Capsule hurtCapsule_ = event_.hurtBox->GetCapsuleData();
        DirectX::XMFLOAT3 hitBoxPosition_{};
        DirectX::XMFLOAT3 hurtBoxPosition_{};
        DirectX::XMStoreFloat3(
            &hitBoxPosition_,
            DirectX::XMVectorScale(
                DirectX::XMVectorAdd(hitCapsule_.p0, hitCapsule_.p1),
                0.5f));
        DirectX::XMStoreFloat3(
            &hurtBoxPosition_,
            DirectX::XMVectorScale(
                DirectX::XMVectorAdd(hurtCapsule_.p0, hurtCapsule_.p1),
                0.5f));
        const float centerDirectionX_ =
            hurtBoxPosition_.x - hitBoxPosition_.x;
        const float centerDirectionZ_ =
            hurtBoxPosition_.z - hitBoxPosition_.z;
        float directionX_ = centerDirectionX_;
        float directionZ_ = centerDirectionZ_;

        const float contactHorizontalLength_ = std::sqrt(
            event_.hitNormal.x * event_.hitNormal.x +
            event_.hitNormal.z * event_.hitNormal.z);
        if (event_.hasHitContact && contactHorizontalLength_ > 0.0001f)
        {
            directionX_ = event_.hitNormal.x / contactHorizontalLength_;
            directionZ_ = event_.hitNormal.z / contactHorizontalLength_;
            const float alignment_ =
                directionX_ * centerDirectionX_ +
                directionZ_ * centerDirectionZ_;
            if (alignment_ < 0.0f)
            {
                directionX_ = -directionX_;
                directionZ_ = -directionZ_;
            }
        }

        const float length_ = std::sqrt(
            directionX_ * directionX_ + directionZ_ * directionZ_);
        if (length_ > 0.0001f)
        {
            blackboard_->SetVector2(
                "Hit",
                "Direction",
                DirectX::SimpleMath::Vector2(
                    directionX_ / length_,
                    directionZ_ / length_));
        }
    }
    blackboard_->SetTrigger("Hit", "Received");

    // Blackboardへ値を書いたあとでActionを呼び、受け手側が最新Hit情報を読める順序にする。
    receiver_->ExecuteAction("OnHit");
}

void AttackSystem::PlayCameraShake(const AttackHitEvent& event_) const
{
    BS_PROFILE_SCOPE("AttackSystem::PlayCameraShake");
    if (!event_.cameraShakeEnabled)
    {
        return;
    }

    // Playerが攻撃側か被弾側にいるときだけ、見た目の手応えとしてCameraShakeを再生する。
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    const std::shared_ptr<GameObject> target_ = event_.target.lock();
    if (!attacker_ || !target_)
    {
        return;
    }

    const bool playerIsTarget_ = IsPlayerObject(target_);
    const bool playerIsAttacker_ = IsPlayerObject(attacker_);
    if (!playerIsTarget_ && !playerIsAttacker_)
    {
        return;
    }

    // 技ごとの設定を基準に振幅を増やし、攻撃命中時も短すぎない衝撃を残す。
    const float hitShakeGain_ = playerIsTarget_ ? 2.6f : 2.0f;
    const float scale_ = (playerIsTarget_
        ? 1.0f
        : event_.cameraShakeAttackScale) * hitShakeGain_;
    const float durationScale_ = playerIsTarget_
        ? 1.15f
        : std::sqrt((std::max)(event_.cameraShakeAttackScale, 0.0f));
    std::shared_ptr<CameraShakeComponent> shake_;
    auto& manager_ = target_->GetManager();
    if (const auto cameras_ = manager_.GetCameraManager())
    {
        if (const auto camera_ = cameras_->GetSourceCamera())
        {
            if (const auto owner_ = camera_->GetGameObject())
            {
                shake_ = owner_->GetComponent<CameraShakeComponent>();
            }
        }
    }
    if (!shake_)
    {
        const auto cameraShakes_ = manager_.FindComponents<CameraShakeComponent>();
        if (cameraShakes_.empty())
        {
            return;
        }
        shake_ = cameraShakes_.front();
    }

    shake_->PlayShake(
        event_.cameraShakeReceiveDuration * durationScale_,
        event_.cameraShakeReceivePositionStrength * scale_,
        event_.cameraShakeReceiveRotationStrength * scale_,
        event_.cameraShakeFrequency,
        event_.cameraShakePriority);
}

