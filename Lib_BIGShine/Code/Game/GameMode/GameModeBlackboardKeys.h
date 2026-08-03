#pragma once

namespace GameModeBlackboardKeys
{
    // ゲーム進行や入力制御の共有状態を保存するカテゴリ。
    constexpr const char* CATEGORY_GAME = "Game";
    // Gameplay全体を止める必要があるかを示す。
    constexpr const char* GAMEPLAY_LOCKED = "GameplayLocked";
    // Player入力やPlayer制御を止める必要があるかを示す。
    constexpr const char* PLAYER_CONTROL_LOCKED = "PlayerControlLocked";
    // 敵AIの判断更新を止める必要があるかを示す。
    constexpr const char* ENEMY_AI_LOCKED = "EnemyAILocked";
    // 演出再生中として扱い、通常操作を抑制するかを示す。
    constexpr const char* PRESENTATION_PLAYING = "PresentationPlaying";
    // 戦闘処理全体を一時停止するかを示す。
    constexpr const char* BATTLE_PAUSED = "BattlePaused";

    // 戦闘フェーズやHP割合など、バトル進行値を保存するカテゴリ。
    constexpr const char* CATEGORY_BATTLE = "Battle";
    // 現在の戦闘フェーズ番号。
    constexpr const char* CURRENT_BATTLE_PHASE = "CurrentBattlePhase";
    // Playerの現在HP割合。
    constexpr const char* PLAYER_HP_RATE = "PlayerHpRate";
    // Bossの現在HP割合。
    constexpr const char* BOSS_HP_RATE = "BossHpRate";

    // AIDirectorが攻撃選択の傾向を共有するカテゴリ。
    constexpr const char* CATEGORY_AI_DIRECTOR = "AIDirector";
    // 遅延攻撃を優先する方針。
    constexpr const char* PREFER_DELAYED_ATTACK = "PreferDelayedAttack";
    // フェイント攻撃を優先する方針。
    constexpr const char* PREFER_FEINT_ATTACK = "PreferFeintAttack";
    // ガード崩し攻撃を優先する方針。
    constexpr const char* PREFER_GUARD_BREAK = "PreferGuardBreak";
    // 攻撃頻度を落とす方針。
    constexpr const char* REDUCE_AGGRESSION = "ReduceAggression";
    // 攻撃開始までの遅延倍率。
    constexpr const char* ATTACK_DELAY_RATE = "AttackDelayRate";
    // フェイント選択率。
    constexpr const char* FEINT_RATE = "FeintRate";
    // ガード崩し選択率。
    constexpr const char* GUARD_BREAK_RATE = "GuardBreakRate";
    // 攻撃積極性の倍率。
    constexpr const char* AGGRESSION_RATE = "AggressionRate";
}
