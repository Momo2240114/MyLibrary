#include "PrecompiledHeader.h"

#include "HealthComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace
{
    float CalculateHealthRatio(float currentHealth_, float maxHealth_)
    {
        if (maxHealth_ <= 0.0f)
        {
            return 0.0f;
        }

        return (std::clamp)(currentHealth_ / maxHealth_, 0.0f, 1.0f);
    }

    std::string MakeHealthProgressLabel(float currentHealth_, float maxHealth_)
    {
        char label_[64]{};
        snprintf(
            label_,
            sizeof(label_),
            "%.0f / %.0f",
            currentHealth_,
            maxHealth_);
        return label_;
    }
}

void HealthComponent::Start()
{
    // Scene保存値が範囲外でも、開始時にHPを有効範囲へ丸めてから公開する。
    currentHealth = (std::clamp)(currentHealth, 0.0f, maxHealth);
    RegisterActions();
    PublishToBlackboard();
}

void HealthComponent::ApplyHit()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // AttackSystemがBlackboardへ書いたHit/Damageを読み、Health側のダメージ処理へ変換する。
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetComponent<BlackboardComponent>();
    float damage_ = 0.0f;
    if (blackboard_)
    {
        blackboard_->GetFloat("Hit", "Damage", damage_);
    }
    ApplyDamage(damage_);
}

void HealthComponent::ApplyDamage(float damage_)
{
    const bool wasDead_ = IsDead();

    // 0未満にならないようHPを減らし、死亡した瞬間だけDeath Actionを呼ぶ。
    currentHealth = (std::max)(currentHealth - (std::max)(damage_, 0.0f), 0.0f);
    PublishToBlackboard();

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!wasDead_ && IsDead() && owner_)
    {
        owner_->ExecuteAction("Health_Death");
    }
}

void HealthComponent::Heal(float amount_)
{
    currentHealth = (std::min)(currentHealth + (std::max)(amount_, 0.0f), maxHealth);
    PublishToBlackboard();
}

void HealthComponent::ResetHealth()
{
    currentHealth = maxHealth;
    PublishToBlackboard();
}

void HealthComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // HitReactionやActionSequenceからHealth_Hitとして呼べるよう、Healthカテゴリへ登録する。
    std::weak_ptr<HealthComponent> weakHealth_;
    for (const std::shared_ptr<Component>& component_ : owner_->components)
    {
        const std::shared_ptr<HealthComponent> health_ =
            std::dynamic_pointer_cast<HealthComponent>(component_);
        if (health_.get() == this)
        {
            weakHealth_ = health_;
            break;
        }
    }

    owner_->RegisterAction("Health", "Health_Hit", [weakHealth_]()
    {
        if (const std::shared_ptr<HealthComponent> health_ = weakHealth_.lock())
        {
            health_->ApplyHit();
        }
    });
    owner_->RegisterAction("Health", "Health_Reset", [weakHealth_]()
    {
        if (const std::shared_ptr<HealthComponent> health_ = weakHealth_.lock())
        {
            health_->ResetHealth();
        }
    });
}

void HealthComponent::PublishToBlackboard()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    // UI、AI、演出が同じHP状態を読めるよう、Healthカテゴリへ現在値を公開する。
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetFloat("Health", "Current", currentHealth);
    blackboard_->SetFloat("Health", "Max", maxHealth);
    blackboard_->SetBool("Health", "Dead", IsDead());
}

void HealthComponent::Gui()
{
    BS_PROFILE_SCOPE("HealthComponent::Gui");

    ImGui::Text("Health: %.1f / %.1f", currentHealth, maxHealth);
    const ImVec4 healthColor_ = IsDead()
        ? ImVec4(0.85f, 0.18f, 0.12f, 1.0f)
        : ImVec4(0.20f, 0.72f, 0.32f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, healthColor_);
    const std::string healthLabel_ =
        MakeHealthProgressLabel(currentHealth, maxHealth);
    ImGui::ProgressBar(
        CalculateHealthRatio(currentHealth, maxHealth),
        ImVec2(-1.0f, 0.0f),
        healthLabel_.c_str());
    ImGui::PopStyleColor();

    if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 999999.0f))
    {
        currentHealth = (std::min)(currentHealth, maxHealth);
        PublishToBlackboard();
    }
    if (ImGui::DragFloat("Current Health", &currentHealth, 1.0f, 0.0f, maxHealth))
    {
        currentHealth = (std::clamp)(currentHealth, 0.0f, maxHealth);
        PublishToBlackboard();
    }
    if (ImGui::Button("Damage 100"))
    {
        ApplyDamage(100.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Heal 100"))
    {
        Heal(100.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Health")) ResetHealth();
    ImGui::InputFloat("Debug Health Amount", &debugHealthAmount, 10.0f, 100.0f);
    debugHealthAmount = (std::max)(debugHealthAmount, 0.0f);
    if (ImGui::Button("Damage Custom"))
    {
        ApplyDamage(debugHealthAmount);
    }
    ImGui::SameLine();
    if (ImGui::Button("Heal Custom"))
    {
        Heal(debugHealthAmount);
    }
    ImGui::Text("Dead: %s", IsDead() ? "Yes" : "No");
}

void HealthComponent::Serialize(nlohmann::json& json_) const
{
    json_["MaxHealth"] = maxHealth;
    json_["CurrentHealth"] = currentHealth;
}

void HealthComponent::Deserialize(const nlohmann::json& json_)
{
    maxHealth = (std::max)(json_.value("MaxHealth", 100.0f), 1.0f);
    currentHealth = (std::clamp)(
        json_.value("CurrentHealth", maxHealth),
        0.0f,
        maxHealth);
}
