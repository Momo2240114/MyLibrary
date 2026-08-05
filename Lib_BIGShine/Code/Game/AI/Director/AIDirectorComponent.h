#pragma once
#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "AIDirectorTypes.h"

#include <string>

class GameModeManager;

// 戦闘中のPlayer反応を観測し、GameModeManagerのGlobalBlackboardへAI方針値を流すComponent。
// 実際の移動や攻撃、Effect、Cameraなどの制御は行わず、攻撃傾向の調整だけを担当する。
class AIDirectorComponent : public Component
{
public:
    AIDirectorComponent() = default;

    const std::string& GetName() const override
    {
        static std::string name_ = "AIDirectorComponent";
        return name_;
    }

    // 起動時に現在の統計からRateとPolicyを作り、必要ならGameModeへ反映する。
    void Start() override;
    // 実行中なら毎フレーム統計RateとAI方針を再計算する。
    void Update(float deltaTime_) override;
    // 統計、Rate、Policy、しきい値を確認・調整するDebug UIを描画する。
    void Gui() override;
    // AIDirectorの調整値をJSONへ保存する。
    void Serialize(nlohmann::json& json_) const override;
    // JSONから調整値を復元し、復元後の方針を再計算する。
    void Deserialize(const nlohmann::json& json_) override;

    // 戦闘統計と方針を初期値へ戻す。
    void ResetStats();

    // 敵攻撃の使用回数を記録する。
    void NotifyEnemyAttackUsed(const std::string& attackName_);
    // 敵攻撃がPlayerへ命中した回数を記録する。
    void NotifyEnemyAttackHit(const std::string& attackName_);
    // 敵攻撃が通常回避された回数を記録する。
    void NotifyEnemyAttackDodged(const std::string& attackName_);
    // 敵攻撃がジャスト回避された回数を記録する。
    void NotifyEnemyAttackJustDodged(const std::string& attackName_);
    // 敵攻撃がガードされた回数を記録する。
    void NotifyEnemyAttackGuarded(const std::string& attackName_);
    // 敵攻撃がジャストガードされた回数を記録する。
    void NotifyEnemyAttackJustGuarded(const std::string& attackName_);
    // Playerが受けたダメージ回数と合計ダメージを記録する。
    void NotifyPlayerDamaged(float damage_);

    const AIDirectorStats& GetStats() const { return stats; }
    const AIDirectorRates& GetRates() const { return rates; }
    const AIDirectorPolicy& GetPolicy() const { return policy; }

private:
    // OwnerのManagerからGameModeManagerを取得する。
    GameModeManager* ResolveGameModeManager();
    // 統計値から命中率、回避率、ガード率を計算する。
    void UpdateRates();
    // Rateとしきい値から、次にGameModeへ流すAI方針を決める。
    void UpdatePolicy();
    // 計算済みPolicyをGameModeManagerへ渡す。
    void ApplyPolicyToGameMode();

    // 分母が0の場合でも安全に割合を計算する。
    float SafeRate(int value_, int total_) const;

private:
    // 戦闘中に蓄積した生の統計値。
    AIDirectorStats stats;
    // 統計値から計算した割合。
    AIDirectorRates rates;
    // AIの攻撃傾向へ反映する最終方針。
    AIDirectorPolicy policy;

    // falseなら統計からPolicyを更新しない。
    bool isRunning = true;
    // trueなら計算したPolicyをGameModeManagerへ反映する。
    bool applyToGameMode = true;

    // Playerが攻撃を読めていると判定するジャスト回避率。
    float highJustDodgeRateThreshold = 0.5f;
    // GuardBreak寄りへ切り替えるガード率。
    float highGuardRateThreshold = 0.5f;
    // 命中率が低すぎるときに攻撃を読まれていると判定するしきい値。
    float lowHitRateThreshold = 0.25f;

    // Playerが被弾しすぎたときに攻撃性を下げる回数しきい値。
    int playerDamagedCountThreshold = 5;

    // 遅延攻撃を優先する場合の最大遅延倍率。
    float maxAttackDelayRate = 1.35f;
    // フェイントを優先する場合の最大選択率。
    float maxFeintRate = 0.35f;
    // ガード崩しを優先する場合の最大選択率。
    float maxGuardBreakRate = 0.4f;
    // Player被弾が多い場合に下げる攻撃性の下限。
    float aggressionMin = 0.65f;
    // 通常時の攻撃性。
    float aggressionDefault = 1.0f;
};
