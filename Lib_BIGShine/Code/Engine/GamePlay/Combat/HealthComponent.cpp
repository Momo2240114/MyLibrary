#include "PrecompiledHeader.h"

#include "HealthComponent.h"
#include "Game/AI/Human/HumanBossAIComponent.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <cmath>
#include "Engine/Debug/TracyProfiler.h"
#include <cstdio>
#include <imgui.h>
#include <string>

namespace
{
    // GUI の ProgressBar 用に、現在 HP を 0.0 ～ 1.0 の表示比率へ変換する。
    float CalculateHealthRatio(float currentHealth_, float maxHealth_)
    {
        if (maxHealth_ <= 0.0f)
        {
            return 0.0f;
        }

        return (std::clamp)(currentHealth_ / maxHealth_, 0.0f, 1.0f);
    }

    // ImGui::ProgressBar に出す "現在値 / 最大値" の短い表示文字列を作る。
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
    if (!owner_)
    {
        return;
    }

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
    const auto actor_ = GetGameObject();
    if (actor_ && actor_->manager &&
        (actor_->GetManager().IsBossFinishActive() || actor_->GetManager().IsPlayerDeathActive()))
    {
        return;
    }
    if (const auto bossAI_ = actor_ ? actor_->GetComponent<HumanBossAIComponent>() : nullptr)
    {
        damage_ = bossAI_->ConstrainBattleDamage(damage_);
    }
    const bool wasDead_ = IsDead();

    // 0未満にならないようHPを減らし、死亡した瞬間だけDeath Actionを呼ぶ。
    currentHealth = (std::max)(currentHealth - (std::max)(damage_, 0.0f), 0.0f);
    PublishToBlackboard();

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!wasDead_ && IsDead() && owner_)
    {
        // 既存の死亡Actionを維持し、Playerだけは衝突走査後のコンテニュー演出も予約する。
        if (owner_->manager)
        {
            owner_->GetManager().RequestPlayerDeath(owner_);
        }
        owner_->ExecuteAction("Health_Death");
    }
}

void HealthComponent::Heal(float amount_)
{
    // 回復量は負値を無視し、最大 HP を超えないように丸める。
    currentHealth = (std::min)(currentHealth + (std::max)(amount_, 0.0f), maxHealth);
    PublishToBlackboard();
}

void HealthComponent::ResetHealth()
{
    if (const auto actor_ = GetGameObject())
    {
        if (const auto bossAI_ = actor_->GetComponent<HumanBossAIComponent>())
        {
            bossAI_->ResetBerserk();
        }
    }
    // デバッグやリトライ時に、死亡フラグも含めて正常な生存状態へ戻す。
    currentHealth = maxHealth;
    PublishToBlackboard();
}

void HealthComponent::ConfirmPresentationDeath()
{
    if (IsDead())
    {
        return;
    }
    currentHealth = 0.0f;
    PublishToBlackboard();
    if (const auto actor_ = GetGameObject())
    {
        actor_->ExecuteAction("Health_Death");
    }
}

void HealthComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }

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
    // 回復量はイベントのBlackboard書き込みから受け取り、Skill名や固定値へ依存しない。
    owner_->RegisterAction("Health", "Health_Heal", [weakHealth_]()
    {
        BS_PROFILE_SCOPE("HealthComponent::HealAction");
        const auto health_ = weakHealth_.lock();
        const auto actor_ = health_ ? health_->GetGameObject() : nullptr;
        const auto blackboard_ = actor_ ? actor_->GetComponent<BlackboardComponent>() : nullptr;
        float amount_ = 0.0f;
        if (health_ && !health_->IsDead() && blackboard_ &&
            blackboard_->GetFloat("Health", "HealAmount", amount_) && std::isfinite(amount_))
        {
            health_->Heal(amount_);
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
    if (!owner_)
    {
        return;
    }

    // UI、AI、演出が同じHP状態を読めるよう、Healthカテゴリへ現在値を公開する。
    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetOrAddComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        return;
    }

    blackboard_->SetFloat("Health", "Current", currentHealth);
    blackboard_->SetFloat("Health", "Max", maxHealth);
    blackboard_->SetBool("Health", "Dead", IsDead());

    if (owner_->manager && owner_->GetComponent<PlayerControllerComponent>())
    {
        const float rate_ = CalculateHealthRatio(currentHealth, maxHealth);
        if (const auto mode_ = owner_->GetManager().GetGameModeManager(); mode_ && mode_->GetPlayerHpRate() != rate_)
        {
            mode_->SetPlayerHpRate(rate_);
        }
    }
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
        // 最大値を下げた時に現在 HP が上限を超えたまま残らないようにする。
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
    if (ImGui::Button("Reset Health"))
    {
        ResetHealth();
    }
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
    // Scene 保存では、最大値と現在値の両方を持たせて途中状態も復元できるようにする。
    json_["MaxHealth"] = maxHealth;
    json_["CurrentHealth"] = currentHealth;
}

void HealthComponent::Deserialize(const nlohmann::json& json_)
{
    // 壊れた JSON や古いデータでも、最大 HP は 1 以上、現在 HP は有効範囲へ補正する。
    maxHealth = (std::max)(json_.value("MaxHealth", 100.0f), 1.0f);
    currentHealth = (std::clamp)(
        json_.value("CurrentHealth", maxHealth),
        0.0f,
        maxHealth);
}
