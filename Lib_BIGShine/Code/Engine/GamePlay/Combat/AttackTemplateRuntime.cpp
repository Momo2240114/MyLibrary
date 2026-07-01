#include "AttackTemplateRuntime.h"

#include "AttackTemplateManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationState.h"
#include "Engine/Physics/HitBox.h"

#include <cstdio>

namespace
{
    std::shared_ptr<GameObject> GetRoot(std::shared_ptr<GameObject> object_)
    {
        while (object_ && object_->transform)
        {
            const std::shared_ptr<Transform> parentTransform_ = object_->transform->parent.lock();
            if (!parentTransform_) break;
            const std::shared_ptr<GameObject> parentObject_ = parentTransform_->gameObject.lock();
            if (!parentObject_) break;
            object_ = parentObject_;
        }
        return object_;
    }

    std::shared_ptr<HitBox> FindHitBox(
        const std::shared_ptr<GameObject>& object_,
        const std::string& hitBoxName_)
    {
        if (!object_) return nullptr;
        for (const std::shared_ptr<Component>& component_ : object_->colliderComponents)
        {
            const std::shared_ptr<HitBox> hitBox_ = std::dynamic_pointer_cast<HitBox>(component_);
            if (hitBox_ && (hitBox_->GetActionPrefix() == hitBoxName_ || hitBox_->GetName() == hitBoxName_))
                return hitBox_;
        }
        if (!object_->transform) return nullptr;
        for (const std::shared_ptr<Transform>& childTransform_ : object_->transform->children)
        {
            if (!childTransform_) continue;
            if (const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
                childTransform_->gameObject.lock(), hitBoxName_)) return hitBox_;
        }
        return nullptr;
    }

    const AttackTemplate* GetTemplate(const AnimEvent& event_)
    {
        AttackTemplateManager& manager_ = AttackTemplateManager::Instance();
        if (manager_.GetTemplates().empty()) manager_.LoadAll();
        return manager_.FindTemplate(event_.attackTemplateId);
    }

    void ConfigureWeaponHitBox(
        const std::shared_ptr<GameObject>& owner_,
        const AttackTemplate& attackTemplate_,
        uint32_t attackInstanceId_)
    {
        if (!attackTemplate_.weaponHitBox.enabled) return;
        const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
            GetRoot(owner_), attackTemplate_.weaponHitBox.hitBoxName);
        if (!hitBox_)
        {
            std::printf("AttackTemplate WeaponHitBox not found: %s\n",
                attackTemplate_.weaponHitBox.hitBoxName.c_str());
            return;
        }
        hitBox_->SetOwner(owner_.get());
        hitBox_->SetAttackInstanceId(attackInstanceId_);
        hitBox_->SetHitGroupId(attackTemplate_.hitGroupId);
        hitBox_->SetDamage(attackTemplate_.damage);
        hitBox_->hitStopDuration = attackTemplate_.hitStopDuration;
        hitBox_->hitStopScale = attackTemplate_.hitStopScale;
        hitBox_->SetKnockBackPower(attackTemplate_.knockBackPower);
        hitBox_->hitParticleAssetPath = attackTemplate_.hitEffect.hitParticleAssetPath;
        hitBox_->hitParticleEmitterName = attackTemplate_.hitEffect.hitParticleEmitterName;
        hitBox_->hitParticleLifeTime = attackTemplate_.hitEffect.hitParticleLifeTime;
        hitBox_->hitParticleNormalOffset = attackTemplate_.hitEffect.hitParticleNormalOffset;
        hitBox_->hitMeshVFXAssetPath = attackTemplate_.hitEffect.hitMeshVfxAssetPath;
        hitBox_->hitMeshVFXLifeTime = attackTemplate_.hitEffect.hitMeshVfxLifeTime;
        hitBox_->hitMeshVFXNormalOffset = attackTemplate_.hitEffect.hitMeshVfxNormalOffset;
        hitBox_->canHitOwner = attackTemplate_.canHitOwner;
        hitBox_->allowMultiHit = attackTemplate_.allowMultiHit;
        hitBox_->startNodeName = attackTemplate_.weaponHitBox.startNodeName;
        hitBox_->endNodeName = attackTemplate_.weaponHitBox.endNodeName;
        hitBox_->radius = attackTemplate_.weaponHitBox.radius;
        hitBox_->Deactivate();
        hitBox_->RecreatePhysXActor();
    }
}

void AttackTemplateRuntime::Start(
    const std::shared_ptr<GameObject>& owner_,
    AnimEvent& event_)
{
    if (!owner_) return;
    const AttackTemplate* attackTemplate_ = GetTemplate(event_);
    if (!attackTemplate_) return;
    event_.attackTemplateRuntimeActive = true;
    event_.attackTemplateInstanceId = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
    event_.spawnedAttackEffects.assign(attackTemplate_->attackEffects.size(), false);
    ConfigureWeaponHitBox(owner_, *attackTemplate_, event_.attackTemplateInstanceId);

    for (const SlashEffectTemplate& slash_ : attackTemplate_->slashEffects)
    {
        if (!slash_.enabled || slash_.vfxPath.empty()) continue;
        AttackEffectSpawnRequest request_{};
        request_.ownerObject = owner_.get();
        request_.attackInstanceId = event_.attackTemplateInstanceId;
        request_.setting.vfxPath = slash_.vfxPath;
        request_.setting.spawnTime = slash_.spawnTimeOffset;
        request_.setting.lifeTime = slash_.lifeTime;
        request_.setting.localOffset = slash_.localOffset;
        request_.setting.localRotation = slash_.localRotation;
        request_.setting.localScale = slash_.localScale;
        request_.setting.followOwner = slash_.followOwner;
        request_.setting.hasHitBox = slash_.hitBox.enabled;
        request_.setting.hitBoxName = slash_.hitBox.hitBoxName;
        request_.setting.hitStartTime = slash_.hitBox.activeStartTime;
        request_.setting.hitEndTime = slash_.hitBox.activeEndTime;
        request_.setting.hitBoxRadius = slash_.hitBox.radius;
        request_.setting.hitBoxHalfHeight = slash_.hitBox.halfHeight;
        request_.setting.damage = attackTemplate_->damage;
        request_.setting.knockBackPower = attackTemplate_->knockBackPower;
        request_.setting.hitStopDuration = attackTemplate_->hitStopDuration;
        request_.setting.hitStopScale = attackTemplate_->hitStopScale;
        request_.setting.hitEffectPath = attackTemplate_->hitEffect.hitMeshVfxAssetPath;
        request_.setting.hitGroupId = attackTemplate_->hitGroupId;
        request_.setting.allowMultiHit = attackTemplate_->allowMultiHit;
        owner_->GetManager().GetEffectManager().SpawnSlashEffect(request_);
    }
}

void AttackTemplateRuntime::Update(
    const std::shared_ptr<GameObject>& owner_,
    AnimEvent& event_,
    float elapsedTime_)
{
    if (!owner_ || !event_.attackTemplateRuntimeActive) return;
    const AttackTemplate* attackTemplate_ = GetTemplate(event_);
    if (!attackTemplate_) return;
    if (attackTemplate_->weaponHitBox.enabled)
    {
        const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
            GetRoot(owner_), attackTemplate_->weaponHitBox.hitBoxName);
        if (hitBox_)
        {
            const bool active_ = elapsedTime_ >= attackTemplate_->weaponHitBox.activeStartTime &&
                elapsedTime_ <= attackTemplate_->weaponHitBox.activeEndTime;
            if (active_ && !hitBox_->IsEnabled()) hitBox_->Activate();
            if (!active_ && hitBox_->IsEnabled()) hitBox_->Deactivate();
        }
    }
    if (event_.spawnedAttackEffects.size() != attackTemplate_->attackEffects.size())
        event_.spawnedAttackEffects.assign(attackTemplate_->attackEffects.size(), false);
    for (size_t effectIndex_ = 0; effectIndex_ < attackTemplate_->attackEffects.size(); ++effectIndex_)
    {
        const AttackEffectSpawnTemplate& effect_ = attackTemplate_->attackEffects[effectIndex_];
        if (!event_.spawnedAttackEffects[effectIndex_] && effect_.enabled && elapsedTime_ >= effect_.spawnTimeOffset)
        {
            owner_->GetManager().GetEffectManager().SpawnAttackEffect(
                effect_, *attackTemplate_, owner_.get(), event_.attackTemplateInstanceId);
            event_.spawnedAttackEffects[effectIndex_] = true;
        }
    }
}

void AttackTemplateRuntime::End(
    const std::shared_ptr<GameObject>& owner_,
    AnimEvent& event_)
{
    if (!owner_) return;
    const AttackTemplate* attackTemplate_ = GetTemplate(event_);
    if (attackTemplate_ && attackTemplate_->weaponHitBox.enabled)
    {
        if (const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
            GetRoot(owner_), attackTemplate_->weaponHitBox.hitBoxName)) hitBox_->Deactivate();
    }
    event_.attackTemplateRuntimeActive = false;
    event_.spawnedAttackEffects.clear();
}
