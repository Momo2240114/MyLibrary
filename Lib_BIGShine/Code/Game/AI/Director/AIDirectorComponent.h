#pragma once
#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "AIDirectorTypes.h"

#include <string>

class GameModeManager;

// 戦闘全体の傾向を軽く観測し、GameModeManagerのGlobalBlackboardへAI方針値を流すComponent
// 実際の移動や攻撃,EffectCameraなどの制御は行わない
class AIDirectorComponent : public Component
{
public:
    AIDirectorComponent() = default;

    const std::string& GetName() const override
    {
        static std::string name_ = "AIDirectorComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void ResetStats();

    void NotifyEnemyAttackUsed(const std::string& attackName_);
    void NotifyEnemyAttackHit(const std::string& attackName_);
    void NotifyEnemyAttackDodged(const std::string& attackName_);
    void NotifyEnemyAttackJustDodged(const std::string& attackName_);
    void NotifyEnemyAttackGuarded(const std::string& attackName_);
    void NotifyEnemyAttackJustGuarded(const std::string& attackName_);
    void NotifyPlayerDamaged(float damage_);

    const AIDirectorStats& GetStats() const { return stats; }
    const AIDirectorRates& GetRates() const { return rates; }
    const AIDirectorPolicy& GetPolicy() const { return policy; }

private:
    GameModeManager* ResolveGameModeManager();
    void UpdateRates();
    void UpdatePolicy();
    void ApplyPolicyToGameMode();

    float SafeRate(int value_, int total_) const;

private:
    AIDirectorStats stats;
    AIDirectorRates rates;
    AIDirectorPolicy policy;

    bool isRunning = true;
    bool applyToGameMode = true;

    float highJustDodgeRateThreshold = 0.5f;
    float highGuardRateThreshold = 0.5f;
    float lowHitRateThreshold = 0.25f;

    int playerDamagedCountThreshold = 5;

    float maxAttackDelayRate = 1.35f;
    float maxFeintRate = 0.35f;
    float maxGuardBreakRate = 0.4f;
    float aggressionMin = 0.65f;
    float aggressionDefault = 1.0f;
};