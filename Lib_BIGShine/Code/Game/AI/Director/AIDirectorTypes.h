#pragma once

// AIDirectorが戦闘中に蓄積する生の統計値。
struct AIDirectorStats
{
    // 敵攻撃を実行した総回数。
    int totalEnemyAttackUsed = 0;
    // 敵攻撃がPlayerへ命中した回数。
    int totalEnemyAttackHit = 0;
    // 敵攻撃が通常回避された回数。
    int totalEnemyAttackDodged = 0;
    // 敵攻撃がジャスト回避された回数。
    int totalEnemyAttackJustDodged = 0;
    // 敵攻撃がガードされた回数。
    int totalEnemyAttackGuarded = 0;
    // 敵攻撃がジャストガードされた回数。
    int totalEnemyAttackJustGuarded = 0;
    // Playerがダメージを受けた回数。
    int playerDamagedCount = 0;

    // Playerが受けた合計ダメージ。
    float totalPlayerDamage = 0.0f;
};

// 統計値から計算した、AI方針判定用の割合。
struct AIDirectorRates
{
    // 敵攻撃の命中率。
    float hitRate = 0.0f;
    // 敵攻撃が通常回避された割合。
    float dodgeRate = 0.0f;
    // 敵攻撃がジャスト回避された割合。
    float justDodgeRate = 0.0f;
    // 敵攻撃がガードされた割合。
    float guardRate = 0.0f;
    // 敵攻撃がジャストガードされた割合。
    float justGuardRate = 0.0f;
};

// GameModeManagerへ渡す、AI攻撃選択の調整方針。
struct AIDirectorPolicy
{
    // 攻撃タイミングを遅らせる行動を優先するか。
    bool preferDelayedAttack = false;
    // フェイント攻撃を優先するか。
    bool preferFeintAttack = false;
    // ガード崩し攻撃を優先するか。
    bool preferGuardBreak = false;
    // Player被弾が多い場合に攻撃性を下げるか。
    bool reduceAggression = false;

    // 攻撃開始の遅延倍率。
    float attackDelayRate = 1.0f;
    // フェイント攻撃の選択率。
    float feintRate = 0.0f;
    // ガード崩し攻撃の選択率。
    float guardBreakRate = 0.0f;
    // 攻撃頻度や積極性の倍率。
    float aggressionRate = 1.0f;
};
