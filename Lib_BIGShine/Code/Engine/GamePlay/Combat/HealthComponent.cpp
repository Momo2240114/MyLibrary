#include "PrecompiledHeader.h"

#include "HealthComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

void HealthComponent::Start()
{
    currentHealth = (std::clamp)(currentHealth, 0.0f, maxHealth);
    RegisterActions();
    PublishToBlackboard();
}

void HealthComponent::ApplyHit()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

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
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_) return;

    blackboard_->SetFloat("Health", "Current", currentHealth);
    blackboard_->SetFloat("Health", "Max", maxHealth);
    blackboard_->SetBool("Health", "Dead", IsDead());
}

void HealthComponent::Gui()
{
    ImGui::Text("Health: %.1f / %.1f", currentHealth, maxHealth);
    if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 999999.0f))
    {
        currentHealth = (std::min)(currentHealth, maxHealth);
        PublishToBlackboard();
    }
    if (ImGui::Button("Reset Health")) ResetHealth();
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
