#pragma once
#include <array>
#include <nlohmann/json.hpp>

// 状況を読む目的を先に決め、BehaviorTree内で具体的な行動へ落とす。
enum class HumanBossIntent
{
    NONE, OBSERVE, PRESSURE, BAIT, PUNISH, DEFEND, REPOSITION, DISENGAGE,
    CHASE, BREAK_GUARD, PUNISH_DODGE, COUNTER_ATTACK
};

// IntentをEditorとBlackboardへ表示する名称を返す。
const char* GetHumanBossIntentName(HumanBossIntent intent_);

// 駆け引きの調整値。知覚Debugの上書きや戦闘中の履歴は保存しない。
struct HumanBossMindSettings
{
    float reactionDelay = 0.18f;
    float dodgeDelayMultiplier = 1.0f;
    float guardDelayMultiplier = 0.85f;
    float justGuardDelayMultiplier = 1.2f;
    // 強攻撃は短い反応待ちで受け流しを狙い、通常の防御抽選より優先する。
    float heavyReactionDelayMultiplier = 0.65f;
    float repeatPredictionDiscount = 0.4f;
    float reactionBudgetMax = 3.0f;
    float reactionBudgetRecovery = 0.7f;
    float dodgeCost = 1.0f;
    float guardCost = 0.65f;
    float justGuardCost = 1.5f;
    float justGuardCounterDelay = 0.2f; // 弾かれたプレイヤーのひるみを見せてから専用反撃へ移る。
    float sameReactionPenalty = 0.35f;
    float readErrorChance = 0.15f;
    float decisionLockTime = 0.3f;
    // Phase補正で待機が短くなっても、攻撃後に相手の出方を見る時間を残す。
    float minimumObservationTime = 1.0f;
    float guardReadThreshold = 0.35f;
    float guardHoldThreshold = 0.45f;
    // この時間ガードを継続した近距離の相手へ、予兆付きの拘束攻撃を仕掛ける。
    float guardExecutionHoldThreshold = 1.2f;
    float dodgeReadThreshold = 0.25f;
    float weakReadBoost = 3.0f;
    float guardReadBoost = 2.0f;
    float dodgeReadBoost = 2.0f;
    float punishChance = 0.55f;
    float lightCounterChance = 0.45f;
    float whiffWindow = 0.8f;
    float baitChance = 0.25f;
    float baitWait = 0.4f;
    float feintChance = 0.18f;
    float feintCooldown = 8.0f;
    float feintRecentPenalty = 0.2f;
    // 回避を見た段間の追加待機は短くし、通常コンボ全体の速度を保つ。
    float dodgePunishDelay = 0.1f;
    float maximumComboDelay = 1.0f;
    float hitEscapeRecovery = 0.15f;
    // 連続被弾からの復帰中は弱い衝撃で待ち時間を再設定させない。
    float hitEscapeProtection = 0.6f;
    // 振りの遅い強攻撃も連続被弾として数え、毎回最初のひるみへ戻ることを防ぐ。
    float hitEscapeChainWindow = 2.5f;
    // 強攻撃の衝撃値400まで復帰を保護し、連続ひるみを断ち切る。
    float hitEscapeArmor = 400.0f;
    float postHitRepositionTime = 0.8f;
    // 空中強攻撃の短い予兆と、攻撃後の離脱・着地へ対応する調整値。
    float airHeavyReactionDelayMultiplier = 0.55f;
    float airHeavyThreatHeightTolerance = 3.5f;
    float airHeavyPressureWindow = 1.4f;
    float airHeavyMemoryWindow = 4.0f;
    float airLandingPressureDistance = 2.3f;
    float airHeavyChaseSpeedMultiplier = 1.35f;
    float airHeavyRecoveryArmor = 16.0f;
    std::array<float, 3> phaseDelay{ 1.0f, 0.85f, 0.75f };
    std::array<float, 3> phaseObserve{ 1.2f, 1.0f, 0.7f };
    std::array<float, 3> phaseAggression{ 0.8f, 1.0f, 1.25f };
    // Inspectorから秒数・確率・消費量とPhase補正を編集する。
    void Gui();
    void Serialize(nlohmann::json& json_) const;
    void Deserialize(const nlohmann::json& json_);
};
