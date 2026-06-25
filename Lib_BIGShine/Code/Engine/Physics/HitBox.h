#pragma once

#include "CapsuleCollider.h"

#include <cstdint>

class HitBox : public CapsuleCollider
{
public:
    float damage = 1.0f;
    float hitStopTime = 0.0f;
    float hitStopDir = 0.0f;
    uint32_t hitStopLayerMask = 3u;
    float knockBackPower = 0.0f;
    bool hitOncePerActivation = true;
    std::string attackTag = "Attack";
    std::string hitParticleAssetPath;
    std::string hitParticleEmitterName;
    float hitParticleLifeTime = 1.0f;
    float hitParticleNormalOffset = 0.0f;
    std::string hitMeshVFXAssetPath;
    float hitMeshVFXLifeTime = 1.0f;
    float hitMeshVFXNormalOffset = 0.0f;
    std::string hitSoundPath;
    float hitSoundVolume = 1.0f;
    bool hitSoundMuffled = false;
    float hitSoundMuffledCutoff = 1200.0f;

    // ‹ó‚È‚ç GameObject –¼‚ðŽg‚¤
    std::string actionPrefix = "";
    // ActionŽí•Ê–¼‚È‚Ì‚ÅŒÅ’èˆµ‚¢
    std::string activateActionName = "EnableHitBox";
    std::string deactivateActionName = "DisableHitBox";

    std::string blackboardCategory = "Attack";
    std::string damageKey = "Damage";
    std::string hitStopTimeKey = "HitStopTime";
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
};

