#pragma once

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

#include <algorithm>

// ゲーム全体で共有する進行状態とGlobalBlackboardを管理する軽量Manager。
// 移動、演出、AI判断などのSystem側が参照するロック状態やAI方針を集約する。
class GameModeManager
{
public:
    GameModeManager() = default;
    ~GameModeManager() = default;

    // 初期状態をGlobalBlackboardへ書き込む。
    void Start();
    // 毎フレーム共有状態をGlobalBlackboardへ同期する。
    void Update(float deltaTime_);
    // 戦闘進行、HP割合、AI方針を初期値へ戻す。
    void ResetBattleState();

    // 他Systemが参照する共有Blackboardを返す。
    BlackboardComponent& GetGlobalBlackboard() { return globalBlackboard; }
    const BlackboardComponent& GetGlobalBlackboard() const { return globalBlackboard; }

    bool IsGameplayLocked() const { return gameplayLocked; }
    bool IsPlayerControlLocked() const { return playerControlLocked; }
    bool IsEnemyAILocked() const { return enemyAILocked; }
    bool IsPresentationPlaying() const { return presentationPlaying; }
    bool IsBattlePaused() const { return battlePaused; }

    void SetGameplayLocked(bool locked_);
    void SetPlayerControlLocked(bool locked_);
    void SetEnemyAILocked(bool locked_);
    void SetPresentationPlaying(bool playing_);
    void SetBattlePaused(bool paused_);

    int GetCurrentBattlePhase() const { return currentBattlePhase; }
    void SetCurrentBattlePhase(int phase_);

    float GetPlayerHpRate() const { return playerHpRate; }
    void SetPlayerHpRate(float rate_);

    float GetBossHpRate() const { return bossHpRate; }
    void SetBossHpRate(float rate_);

    void SetAIDirectorPreferDelayedAttack(bool enabled_);
    void SetAIDirectorPreferFeintAttack(bool enabled_);
    void SetAIDirectorPreferGuardBreak(bool enabled_);
    void SetAIDirectorReduceAggression(bool enabled_);

    void SetAIDirectorAttackDelayRate(float rate_);
    void SetAIDirectorFeintRate(float rate_);
    void SetAIDirectorGuardBreakRate(float rate_);
    void SetAIDirectorAggressionRate(float rate_);

    bool GetAIDirectorPreferDelayedAttack() const { return preferDelayedAttack; }
    bool GetAIDirectorPreferFeintAttack() const { return preferFeintAttack; }
    bool GetAIDirectorPreferGuardBreak() const { return preferGuardBreak; }
    bool GetAIDirectorReduceAggression() const { return reduceAggression; }

    float GetAIDirectorAttackDelayRate() const { return attackDelayRate; }
    float GetAIDirectorFeintRate() const { return feintRate; }
    float GetAIDirectorGuardBreakRate() const { return guardBreakRate; }
    float GetAIDirectorAggressionRate() const { return aggressionRate; }

    // 現在保持している状態値をカテゴリ別にGlobalBlackboardへ反映する。
    void WriteBlackboard();

private:
    // HP割合やAI重みなど、0から1の範囲で扱う値を補正する。
    float ClampRate(float value_) const;

private:
    // ゲーム全体で共有するBlackboard本体。
    BlackboardComponent globalBlackboard;

    // Gameplay全体、Player操作、Enemy AI、演出、戦闘停止のロック状態。
    bool gameplayLocked = false;
    bool playerControlLocked = false;
    bool enemyAILocked = false;
    bool presentationPlaying = false;
    bool battlePaused = false;

    // 現在の戦闘フェーズと、Player/BossのHP割合。
    int currentBattlePhase = 0;
    float playerHpRate = 1.0f;
    float bossHpRate = 1.0f;

    // AIDirectorへ渡す攻撃傾向の優先フラグ。
    bool preferDelayedAttack = false;
    bool preferFeintAttack = false;
    bool preferGuardBreak = false;
    bool reduceAggression = false;

    // AIDirectorが攻撃選択の重み付けに使用する係数。
    float attackDelayRate = 1.0f;
    float feintRate = 0.0f;
    float guardBreakRate = 0.0f;
    float aggressionRate = 1.0f;
};
