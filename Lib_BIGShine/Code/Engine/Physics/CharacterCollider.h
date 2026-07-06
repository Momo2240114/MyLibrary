#pragma once

#include "CapsuleCollider.h"

class CharacterCollider : public CapsuleCollider
{
public:
    CharacterCollider();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& j_) override;

    float GetStepOffset() const { return stepOffset; }
    float GetContactOffset() const { return contactOffset; }
    float GetGroundSnapDistance() const { return groundSnapDistance; }
    bool ShouldLimitHorizontalLift() const { return limitHorizontalLift; }
    float GetMaxHorizontalLift() const { return maxHorizontalLift; }

private:
    void EnforceCharacterSettings();

    float stepOffset = 0.5f;
    float contactOffset = 0.02f;
    float groundSnapDistance = 0.9f;
    bool limitHorizontalLift = true;
    float maxHorizontalLift = 0.05f;
};
