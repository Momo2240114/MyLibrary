#pragma once

namespace AIDirectorBlackboardKeys
{
    // AIDirector方針をBlackboardに保存するカテゴリ名。
    constexpr const char* CATEGORY = "AIDirector";

    // 敵攻撃がPlayerへ命中した割合。
    constexpr const char* HIT_RATE = "HitRate";
    // 敵攻撃が回避された割合。
    constexpr const char* DODGE_RATE = "DodgeRate";
    // 敵攻撃がジャスト回避された割合。
    constexpr const char* JUST_DODGE_RATE = "JustDodgeRate";
    // 敵攻撃がガードされた割合。
    constexpr const char* GUARD_RATE = "GuardRate";
    // 敵攻撃がジャストガードされた割合。
    constexpr const char* JUST_GUARD_RATE = "JustGuardRate";

    // 遅延攻撃を優先するか。
    constexpr const char* PREFER_DELAYED_ATTACK = "PreferDelayedAttack";
    // フェイント攻撃を優先するか。
    constexpr const char* PREFER_FEINT_ATTACK = "PreferFeintAttack";
    // ガード崩し攻撃を優先するか。
    constexpr const char* PREFER_GUARD_BREAK = "PreferGuardBreak";
    // 攻撃性を下げるか。
    constexpr const char* REDUCE_AGGRESSION = "ReduceAggression";

    // 攻撃開始遅延の倍率。
    constexpr const char* ATTACK_DELAY_RATE = "AttackDelayRate";
    // フェイント選択率。
    constexpr const char* FEINT_RATE = "FeintRate";
    // ガード崩し選択率。
    constexpr const char* GUARD_BREAK_RATE = "GuardBreakRate";
    // 攻撃積極性の倍率。
    constexpr const char* AGGRESSION_RATE = "AggressionRate";
}
