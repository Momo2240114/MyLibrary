#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <unordered_map>

class HitBox;

class HurtBox : public CapsuleCollider
{
public:
    float damageMultiplier = 1.0f;
    std::string hurtTag = "Body";

    HurtBox();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }
    void Start() override;
    void Update(float deltaTime_) override;
    void SetInvincible(bool invincible_);
    bool IsInvincible() const { return invincible; }
    bool CanReceiveHit(const HitBox& hitBox_) const;
    void RecordHit(const HitBox& hitBox_);
    void ResetHitHistory();
    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

private:
    void EnforceHurtBoxSettings();
    void RegisterActions();

private:
    float invincibilityDuration = 0.08f;
    float invincibilityTimer = 0.0f;
    bool invincible = false;
    std::unordered_map<const HitBox*, uint64_t> receivedActivationIds;
};
