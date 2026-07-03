#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <string>
#include <unordered_map>

class HitBox;

class JustDodgeCollider : public CapsuleCollider
{
public:
    JustDodgeCollider();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Activate();
    void Deactivate();

    bool CanReactToAttack(const HitBox& hitBox_) const;
    void RecordDodgedAttack(const HitBox& hitBox_);
    void ClearDodgedAttackHistory();

    std::string GetActivateActionName() const;
    std::string GetDeactivateActionName() const;
    std::string GetActionPrefix() const;

private:
    void EnforceJustDodgeSettings();
    void RegisterActions();

private:
    std::string actionPrefix = "JustDodge";
    std::string activateActionName = "EnableJustDodgeCollider";
    std::string deactivateActionName = "DisableJustDodgeCollider";

    bool canReactToOwnerAttack = false;
    bool oncePerAttackActivation = true;
    int teamId = 0;

    std::unordered_map<const HitBox*, uint64_t> dodgedAttackActivationIds;
};