#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class GameObject;
class HitBox;

class GuardCollider : public CapsuleCollider
{
public:
    GuardCollider();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Activate();
    void Deactivate();

    bool CanGuardAttack(const HitBox& hitBox_) const;
    void RecordGuardedAttack(const HitBox& hitBox_);
    void ClearGuardedAttackHistory();

    void SetGuardOwner(GameObject* guardOwner_);
    std::shared_ptr<GameObject> GetGuardOwner() const;

    bool IsJustGuardEnabled() const { return justGuardEnabled; }
    void SetJustGuardEnabled(bool enabled_);

    std::string GetActivateActionName() const;
    std::string GetDeactivateActionName() const;
    std::string GetEnableJustGuardActionName() const;
    std::string GetDisableJustGuardActionName() const;
    std::string GetActionPrefix() const;

private:
    void EnforceGuardSettings();
    void RegisterActions();
    std::shared_ptr<GameObject> ResolveGuardOwnerFromHierarchy() const;

private:
    std::string actionPrefix;
    std::string activateActionName = "EnableGuardCollider";
    std::string deactivateActionName = "DisableGuardCollider";
    std::string enableJustGuardActionName = "EnableJustGuard";
    std::string disableJustGuardActionName = "DisableJustGuard";

    bool canGuardOwnerAttack = false;
    bool oncePerAttackActivation = true;
    int teamId = 0;
    bool justGuardEnabled = false;

    GameObject* guardOwner = nullptr;
    std::unordered_map<const HitBox*, uint64_t> guardedAttackActivationIds;
};
