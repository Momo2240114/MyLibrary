#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

// 攻撃者側の基礎戦闘ステータスを管理するComponent。
class CombatStatsComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    float GetAttackPower() const
    {
        return attackPower;
    }

    float GetDamageRateModifier() const
    {
        return damageRateModifier;
    }

    float GetFixedDamageModifier() const
    {
        return fixedDamageModifier;
    }

private:
    // 現在の攻撃力をBlackboardへ公開し、AIやDebug表示から参照できるようにする。
    void PublishToBlackboard();

    // 技倍率を掛ける前の基礎攻撃力。
    float attackPower = 100.0f;
    // バフやデバフで使う全体ダメージ倍率。
    float damageRateModifier = 1.0f;
    // バフやデバフで使う固定加算ダメージ。
    float fixedDamageModifier = 0.0f;
};
