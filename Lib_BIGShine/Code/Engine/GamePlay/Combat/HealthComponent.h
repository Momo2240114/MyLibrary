#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

class HealthComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "HealthComponent";
        return name_;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void ApplyHit();
    void ApplyDamage(float damage_);
    void Heal(float amount_);
    void ResetHealth();

    float GetCurrentHealth() const { return currentHealth; }
    float GetMaxHealth() const { return maxHealth; }
    bool IsDead() const { return currentHealth <= 0.0f; }

private:
    void RegisterActions();
    void PublishToBlackboard();

    float maxHealth = 100.0f;
    float currentHealth = 100.0f;
};
