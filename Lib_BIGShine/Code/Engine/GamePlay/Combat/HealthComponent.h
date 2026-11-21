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

    // BlackboardのHit/Damageを読み取り、HPへ反映する。
    void ApplyHit();

    // 直接指定したダメージ量をHPから減らす。
    void ApplyDamage(float damage_);

    // HPを回復する。最大HPは超えない。
    void Heal(float amount_);

    // 現在HPを最大HPへ戻す。
    void ResetHealth();
    // 最終スタン後の決め絵が完成した時だけ、保留していた撃破を確定する。
    void ConfirmPresentationDeath();

    float GetCurrentHealth() const
    {
        return currentHealth;
    }

    float GetMaxHealth() const
    {
        return maxHealth;
    }

    bool IsDead() const
    {
        return currentHealth <= 0.0f;
    }

private:
    // Health_Hit/Health_ResetなどのActionを登録する。
    void RegisterActions();

    // UIやAIが読めるよう、現在HP状態をBlackboardへ公開する。
    void PublishToBlackboard();

    // HPの最大値と現在値。
    float maxHealth = 100.0f;
    float currentHealth = 100.0f;

    // GUIのデバッグDamage/Heal入力値。
    float debugHealthAmount = 100.0f;
};
