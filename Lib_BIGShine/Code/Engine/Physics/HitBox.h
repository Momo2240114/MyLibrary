#pragma once

#include "BoxCollider.h"

class HitBox : public BoxCollider
{
public:
    float damage = 1.0f;
    bool hitOncePerActivation = true;
    std::string attackTag = "Attack";

    HitBox();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }
    void Activate();
    void Deactivate();
    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

private:
    void EnforceHitBoxSettings();
};
