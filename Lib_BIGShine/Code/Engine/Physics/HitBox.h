#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <unordered_set>

enum class ATTACK_SOURCE_TYPE
{
    MELEE,
    PROJECTILE,
    REMOTE
};

class HitBox : public CapsuleCollider
{
public:
    float damage = 1.0f;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    uint32_t hitStopLayerMask = 3u;
    GameObject* ownerObject = nullptr;
    std::string sourceAction;
    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    bool allowMultiHit = false;
    bool canHitOwner = false;
    int teamId = 0;
    float impactPower = 1.0f;
    float knockBackPower = 0.0f;
    ATTACK_SOURCE_TYPE attackSourceType = ATTACK_SOURCE_TYPE::MELEE;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;
    bool hitOncePerActivation = true;
    std::string attackTag = "Attack";
    std::string hitParticleAssetPath;
    std::string hitParticleEmitterName;
    float hitParticleLifeTime = 1.0f;
    float hitParticleNormalOffset = 0.0f;
    bool hitParticleUseImpactAxis = false;
    float hitParticleAxisToAttackerBlend = 0.0f;
    bool hitParticleUseTargetAttraction = false;
    float hitParticleTargetAttractionStrength = 0.0f;
    float hitParticleTargetAttractionRange = 0.0f;
    float hitParticleTargetAttractionFalloff = 1.0f;
    std::string hitMeshVFXAssetPath;
    float hitMeshVFXLifeTime = 1.0f;
    float hitMeshVFXNormalOffset = 0.0f;
    std::string hitSoundPath;
    float hitSoundVolume = 1.0f;
    bool hitSoundMuffled = false;
    float hitSoundMuffledCutoff = 1200.0f;
    bool cameraShakeEnabled = true;
    float cameraShakeReceiveDuration = 0.12f;
    float cameraShakeReceivePositionStrength = 0.025f;
    float cameraShakeReceiveRotationStrength = 0.18f;
    float cameraShakeAttackScale = 0.5f;
    float cameraShakeFrequency = 24.0f;
    int cameraShakePriority = 5;

    // 空なら GameObject 名を使う
    std::string actionPrefix = "";
    // Action種別名なので固定扱い
    std::string activateActionName = "EnableHitBox";
    std::string deactivateActionName = "DisableHitBox";

    std::string blackboardCategory = "Attack";
    std::string damageKey = "Damage";
    std::string hitStopDurationKey = "HitStopDuration";
    std::string knockBackPowerKey = "KnockBackPower";
    std::string hitSoundPathKey = "HitSoundPath";

    HitBox();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }
    void Start() override;
    void Activate();
    void Deactivate();
    uint64_t GetActivationId() const { return activationId; }
    void SetOwner(GameObject* owner_);
    void SetAttackInstanceId(uint32_t attackInstanceId_);
    void SetHitGroupId(uint32_t hitGroupId_);
    void SetDamage(float damage_);
    void SetImpactPower(float impactPower_);
    void SetKnockBackPower(float knockBackPower_);
    bool ShouldNotifyAttackerOnGuard() const;
    void SetAirLiftPower(float targetAirLiftPower_, float attackerAirLiftPower_);
    void SetHitEffectPath(const std::string& path_);
    void SetHitEffectBlackboardOverrideEnabled(bool enabled_)
    {
        hitEffectBlackboardOverrideEnabled = enabled_;
    }
    void SetAttackDataBlackboardOverrideEnabled(bool enabled_)
    {
        attackDataBlackboardOverrideEnabled = enabled_;
    }
    void ClearHitTargets();
    bool CanHit(GameObject* target_) const;
    void RegisterHit(GameObject* target_);
    std::shared_ptr<GameObject> GetAttackOwner();
    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;
    std::string GetActivateActionName() const;
    std::string GetDeactivateActionName() const;
    std::string GetActionPrefix()const;
private:
    void EnforceHitBoxSettings();
    void ApplyBlackboardValues();
    void RegisterActions();

private:
    uint64_t activationId = 0;
    bool hitEffectBlackboardOverrideEnabled = true;
    bool attackDataBlackboardOverrideEnabled = true;
    std::unordered_set<GameObject*> alreadyHitTargets;
};

