#include "PrecompiledHeader.h"

#include "CombatStatsComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

const std::string& CombatStatsComponent::GetName() const
{
    static const std::string name_ = "CombatStatsComponent";
    return name_;
}

void CombatStatsComponent::Start()
{
    PublishToBlackboard();
}

void CombatStatsComponent::Gui()
{
    BS_PROFILE_SCOPE("CombatStatsComponent::Gui");

    bool changed_ = false;
    changed_ |= ImGui::DragFloat("Attack Power", &attackPower, 1.0f, 0.0f, 999999.0f);
    changed_ |= ImGui::DragFloat("Damage Rate Modifier", &damageRateModifier, 0.01f, 0.0f, 100.0f);
    changed_ |= ImGui::DragFloat("Fixed Damage Modifier", &fixedDamageModifier, 0.1f, -999999.0f, 999999.0f);
    if (changed_)
    {
        attackPower = (std::max)(attackPower, 0.0f);
        damageRateModifier = (std::max)(damageRateModifier, 0.0f);
        PublishToBlackboard();
    }
}

void CombatStatsComponent::Serialize(nlohmann::json& json_) const
{
    // Scene保存時に攻撃者側の基礎攻撃力と補正値を保持する。
    json_["AttackPower"] = attackPower;
    json_["DamageRateModifier"] = damageRateModifier;
    json_["FixedDamageModifier"] = fixedDamageModifier;
}

void CombatStatsComponent::Deserialize(const nlohmann::json& json_)
{
    attackPower = (std::max)(json_.value("AttackPower", 100.0f), 0.0f);
    damageRateModifier = (std::max)(json_.value("DamageRateModifier", 1.0f), 0.0f);
    fixedDamageModifier = json_.value("FixedDamageModifier", 0.0f);
}

void CombatStatsComponent::PublishToBlackboard()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

    // Damage計算やDebug表示が同じ値を読めるよう、Combatカテゴリへ公開する。
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        return;
    }

    blackboard_->SetFloat("Combat", "AttackPower", attackPower);
    blackboard_->SetFloat("Combat", "DamageRateModifier", damageRateModifier);
    blackboard_->SetFloat("Combat", "FixedDamageModifier", fixedDamageModifier);
}
