#pragma once

struct AIDirectorStats
{
    int totalEnemyAttackUsed = 0;
    int totalEnemyAttackHit = 0;
    int totalEnemyAttackDodged = 0;
    int totalEnemyAttackJustDodged = 0;
    int totalEnemyAttackGuarded = 0;
    int totalEnemyAttackJustGuarded = 0;
    int playerDamagedCount = 0;

    float totalPlayerDamage = 0.0f;
};

struct AIDirectorRates
{
    float hitRate = 0.0f;
    float dodgeRate = 0.0f;
    float justDodgeRate = 0.0f;
    float guardRate = 0.0f;
    float justGuardRate = 0.0f;
};

struct AIDirectorPolicy
{
    bool preferDelayedAttack = false;
    bool preferFeintAttack = false;
    bool preferGuardBreak = false;
    bool reduceAggression = false;

    float attackDelayRate = 1.0f;
    float feintRate = 0.0f;
    float guardBreakRate = 0.0f;
    float aggressionRate = 1.0f;
};