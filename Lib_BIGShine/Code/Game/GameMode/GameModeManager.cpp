#include "PrecompiledHeader.h"

#include "GameModeManager.h"

#include "GameModeBlackboardKeys.h"

// 現在の初期値をGlobalBlackboardへ流し込み、他Systemが参照できる状態にする。
void GameModeManager::Start()
{
    WriteBlackboard();
}

// Manager自身の状態を正として、毎フレームGlobalBlackboardへ同期する。
void GameModeManager::Update(float deltaTime_)
{
    (void)deltaTime_;

    WriteBlackboard();
}

// 戦闘開始前の状態へ戻し、ロックやAI方針が前回戦闘から残らないようにする。
void GameModeManager::ResetBattleState()
{
    gameplayLocked = false;
    playerControlLocked = false;
    enemyAILocked = false;
    presentationPlaying = false;
    battlePaused = false;

    currentBattlePhase = 0;
    playerHpRate = 1.0f;
    bossHpRate = 1.0f;

    preferDelayedAttack = false;
    preferFeintAttack = false;
    preferGuardBreak = false;
    reduceAggression = false;

    attackDelayRate = 1.0f;
    feintRate = 0.0f;
    guardBreakRate = 0.0f;
    aggressionRate = 1.0f;

    WriteBlackboard();
}

void GameModeManager::SetGameplayLocked(bool locked_)
{
    gameplayLocked = locked_;
    WriteBlackboard();
}

void GameModeManager::SetPlayerControlLocked(bool locked_)
{
    playerControlLocked = locked_;
    WriteBlackboard();
}

void GameModeManager::SetEnemyAILocked(bool locked_)
{
    enemyAILocked = locked_;
    WriteBlackboard();
}

void GameModeManager::SetPresentationPlaying(bool playing_)
{
    presentationPlaying = playing_;
    WriteBlackboard();
}

void GameModeManager::SetBattlePaused(bool paused_)
{
    battlePaused = paused_;
    WriteBlackboard();
}

void GameModeManager::SetCurrentBattlePhase(int phase_)
{
    currentBattlePhase = phase_;
    WriteBlackboard();
}

void GameModeManager::SetPlayerHpRate(float rate_)
{
    // HP割合はUIやAI条件でそのまま使うため、0から1の範囲へ丸めて保存する。
    playerHpRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetBossHpRate(float rate_)
{
    // Boss側も同じ正規化割合として扱い、Blackboardの利用側に補正を持たせない。
    bossHpRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorPreferDelayedAttack(bool enabled_)
{
    preferDelayedAttack = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorPreferFeintAttack(bool enabled_)
{
    preferFeintAttack = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorPreferGuardBreak(bool enabled_)
{
    preferGuardBreak = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorReduceAggression(bool enabled_)
{
    reduceAggression = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorAttackDelayRate(float rate_)
{
    // 攻撃遅延率は倍率として扱うため、上限は設けず負値だけを防ぐ。
    attackDelayRate = (std::max)(0.0f, rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorFeintRate(float rate_)
{
    feintRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorGuardBreakRate(float rate_)
{
    guardBreakRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorAggressionRate(float rate_)
{
    aggressionRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::WriteBlackboard()
{
    // Gameカテゴリには操作や演出のロック状態を集約する。
    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::GAMEPLAY_LOCKED,
        gameplayLocked);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PLAYER_CONTROL_LOCKED,
        playerControlLocked);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::ENEMY_AI_LOCKED,
        enemyAILocked);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PRESENTATION_PLAYING,
        presentationPlaying);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::BATTLE_PAUSED,
        battlePaused);

    // Battleカテゴリには現在フェーズとHP割合を保存し、UIやAIが同じ値を参照できるようにする。
    globalBlackboard.SetInt(
        GameModeBlackboardKeys::CATEGORY_BATTLE,
        GameModeBlackboardKeys::CURRENT_BATTLE_PHASE,
        currentBattlePhase);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_BATTLE,
        GameModeBlackboardKeys::PLAYER_HP_RATE,
        playerHpRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_BATTLE,
        GameModeBlackboardKeys::BOSS_HP_RATE,
        bossHpRate);

    // AIDirectorカテゴリには攻撃選択へ影響する方針フラグと重みを保存する。
    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::PREFER_DELAYED_ATTACK,
        preferDelayedAttack);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::PREFER_FEINT_ATTACK,
        preferFeintAttack);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::PREFER_GUARD_BREAK,
        preferGuardBreak);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::REDUCE_AGGRESSION,
        reduceAggression);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::ATTACK_DELAY_RATE,
        attackDelayRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::FEINT_RATE,
        feintRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::GUARD_BREAK_RATE,
        guardBreakRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::AGGRESSION_RATE,
        aggressionRate);
}

float GameModeManager::ClampRate(float value_) const
{
    // 割合として扱う値は、Blackboardへ書く前に有効範囲へ収める。
    return std::clamp(value_, 0.0f, 1.0f);
}
