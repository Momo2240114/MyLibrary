#pragma once

namespace GameModeBlackboardKeys
{
    constexpr const char* CATEGORY_GAME = "Game";
    constexpr const char* GAMEPLAY_LOCKED = "GameplayLocked";
    constexpr const char* PLAYER_CONTROL_LOCKED = "PlayerControlLocked";
    constexpr const char* ENEMY_AI_LOCKED = "EnemyAILocked";
    constexpr const char* PRESENTATION_PLAYING = "PresentationPlaying";
    constexpr const char* BATTLE_PAUSED = "BattlePaused";

    constexpr const char* CATEGORY_BATTLE = "Battle";
    constexpr const char* CURRENT_BATTLE_PHASE = "CurrentBattlePhase";
    constexpr const char* PLAYER_HP_RATE = "PlayerHpRate";
    constexpr const char* BOSS_HP_RATE = "BossHpRate";

    constexpr const char* CATEGORY_AI_DIRECTOR = "AIDirector";
    constexpr const char* PREFER_DELAYED_ATTACK = "PreferDelayedAttack";
    constexpr const char* PREFER_FEINT_ATTACK = "PreferFeintAttack";
    constexpr const char* PREFER_GUARD_BREAK = "PreferGuardBreak";
    constexpr const char* REDUCE_AGGRESSION = "ReduceAggression";
    constexpr const char* ATTACK_DELAY_RATE = "AttackDelayRate";
    constexpr const char* FEINT_RATE = "FeintRate";
    constexpr const char* GUARD_BREAK_RATE = "GuardBreakRate";
    constexpr const char* AGGRESSION_RATE = "AggressionRate";
}