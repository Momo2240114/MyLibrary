#include "PrecompiledHeader.h"

#include "AttackSystem.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Camera/CameraShakeComponent.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/HitBox.h"
#include "Engine/Physics/HurtBox.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool IsPlayerObject(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return false;
        if (object_->GetLayer() == LayerType::Player) return true;

        std::shared_ptr<GameObject> current_ = object_;
        while (current_)
        {
            const std::string& name_ = current_->GetName();
            if (name_.find("Player") != std::string::npos) return true;
            if (!current_->transform) break;

            const std::shared_ptr<Transform> parentTransform_ =
                current_->transform->parent.lock();
            if (!parentTransform_) break;
            current_ = parentTransform_->gameObject.lock();
        }
        return false;
    }
}

void AttackSystem::BeginFrame()
{
    frameHitEvents.clear();
}

void AttackSystem::Clear()
{
    frameHitEvents.clear();
    registeredHitTargets.clear();
}

uint32_t AttackSystem::CreateAttackInstance()
{
    // 1回の攻撃単位を識別するID。多段ヒットや同一対象への再ヒット制御に使う。
    const uint32_t attackInstanceId_ = nextAttackInstanceId++;
    if (nextAttackInstanceId == 0u) nextAttackInstanceId = 1u;
    return attackInstanceId_;
}

void AttackSystem::ClearAttackInstance(uint32_t attackInstanceId_)
{
    if (attackInstanceId_ == 0u) return;

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
    if (attackInstanceId_ == 0u) return;
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
    if (!target_) target_ = hurtBoxObject_;
    if (!hitBoxObject_ || !attacker_ || !target_ || !hurtBoxObject_) return;
    if (!hitBox_.canHitOwner && IsSameHierarchy(attacker_, target_)) return;
    if (!hitBox_.IsEnabled() || !hurtBox_.IsEnabled()) return;
    if (!hitBox_.CanHit(target_.get())) return;
    if (!hurtBox_.CanReceiveHit(hitBox_)) return;
    if (!RegisterAttackHit(
        hitBox_.attackInstanceId,
        hitBox_.hitGroupId,
        target_.get(),
        hitBox_.allowMultiHit)) return;

    // ここから先は「ヒット成立」。反応ComponentやVFXが使う情報をAttackHitEventへ詰める。
    hitBox_.RegisterHit(target_.get());
    hurtBox_.RecordHit(hitBox_);

    AttackHitEvent event_;
    event_.attacker = attacker_;
    event_.target = target_;
    event_.hitBox = &hitBox_;
    event_.hurtBox = &hurtBox_;
    event_.damage = hitBox_.damage * hurtBox_.damageMultiplier;
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
    event_.hitSoundPath = hitBox_.hitSoundPath;
    event_.hitSoundVolume = hitBox_.hitSoundVolume;
    event_.hitSoundMuffled = hitBox_.hitSoundMuffled;
    event_.hitSoundMuffledCutoff = hitBox_.hitSoundMuffledCutoff;
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
        if (callback_) callback_(event_);
    }
}

bool AttackSystem::RegisterAttackHit(
    uint32_t attackInstanceId_,
    uint32_t hitGroupId_,
    const GameObject* target_,
    bool allowMultiHit_)
{
    if (allowMultiHit_ || attackInstanceId_ == 0u || target_ == nullptr) return true;

    // attackInstanceId + hitGroupIdごとに対象を記録し、同じ攻撃の重複ヒットを防ぐ。
    const uint64_t hitKey_ =
        (static_cast<uint64_t>(attackInstanceId_) << 32u) |
        static_cast<uint64_t>(hitGroupId_);
    return registeredHitTargets[hitKey_].insert(target_).second;
}

void AttackSystem::AddHitCallback(HitCallback callback_)
{
    if (callback_) hitCallbacks.push_back(std::move(callback_));
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
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;
        object_ = parentObject_;
    }
    return object_;
}

void AttackSystem::PublishHitToBlackboard(const AttackHitEvent& event_)
{
    const std::shared_ptr<GameObject> target_ = event_.target.lock();
    if (!target_) return;

    // HurtBoxが子Objectにある場合も、親のBlackboardへヒット情報を届けられるよう上へたどる。
    std::shared_ptr<GameObject> receiver_ = target_;
    std::shared_ptr<BlackboardComponent> blackboard_;
    while (receiver_)
    {
        blackboard_ = receiver_->GetComponent<BlackboardComponent>();
        if (blackboard_ || !receiver_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            receiver_->transform->parent.lock();
        if (!parentTransform_) break;
        receiver_ = parentTransform_->gameObject.lock();
    }

    if (!receiver_) return;
    if (!blackboard_)
    {
        blackboard_ = receiver_->GetOrAddComponent<BlackboardComponent>();
    }
    if (!blackboard_) return;

    blackboard_->SetFloat("Hit", "Damage", event_.damage);
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
    if (!event_.cameraShakeEnabled) return;

    // Playerが攻撃側か被弾側にいるときだけ、見た目の手応えとしてCameraShakeを再生する。
    const std::shared_ptr<GameObject> attacker_ = event_.attacker.lock();
    const std::shared_ptr<GameObject> target_ = event_.target.lock();
    if (!attacker_ || !target_) return;

    const bool playerIsTarget_ = IsPlayerObject(target_);
    const bool playerIsAttacker_ = IsPlayerObject(attacker_);
    if (!playerIsTarget_ && !playerIsAttacker_) return;

    const float scale_ = playerIsTarget_
        ? 1.0f
        : event_.cameraShakeAttackScale;
    const float durationScale_ = playerIsTarget_
        ? 1.0f
        : event_.cameraShakeAttackScale;
    const std::vector<std::shared_ptr<CameraShakeComponent>> cameraShakes_ =
        target_->GetManager().FindComponents<CameraShakeComponent>();
    if (cameraShakes_.empty()) return;

    cameraShakes_.front()->PlayShake(
        event_.cameraShakeReceiveDuration * durationScale_,
        event_.cameraShakeReceivePositionStrength * scale_,
        event_.cameraShakeReceiveRotationStrength * scale_,
        event_.cameraShakeFrequency,
        event_.cameraShakePriority);
}

