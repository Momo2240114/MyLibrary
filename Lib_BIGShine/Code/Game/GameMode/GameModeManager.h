#pragma once

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

#include <algorithm>

// ゲーム全体で共有する進行状態とGlobalBlackboardを管理する軽量Manager。
// 移動、攻撃、演出の実処理は各System側に任せ、ここではロック状態やAI方針だけを集約する。
class GameModeManager
{
public:
    GameModeManager() = default;
    ~GameModeManager() = default;

    void Start();
    void Update(float deltaTime_);
    void ResetBattleState();

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

    void WriteBlackboard();

private:
    float ClampRate(float value_) const;

private:
    BlackboardComponent globalBlackboard;

    bool gameplayLocked = false;
    bool playerControlLocked = false;
    bool enemyAILocked = false;
    bool presentationPlaying = false;
    bool battlePaused = false;

    int currentBattlePhase = 0;
    float playerHpRate = 1.0f;
    float bossHpRate = 1.0f;

    bool preferDelayedAttack = false;
    bool preferFeintAttack = false;
    bool preferGuardBreak = false;
    bool reduceAggression = false;

    float attackDelayRate = 1.0f;
    float feintRate = 0.0f;
    float guardBreakRate = 0.0f;
    float aggressionRate = 1.0f;
};
