#pragma once

#include "CapsuleCollider.h"

class HurtBox : public CapsuleCollider
{
public:
    float damageMultiplier = 1.0f;
    std::string hurtTag = "Body";

    HurtBox();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }
    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

private:
    void EnforceHurtBoxSettings();
};
