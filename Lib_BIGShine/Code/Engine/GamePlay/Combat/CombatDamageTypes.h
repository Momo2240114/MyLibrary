#pragma once

// 攻撃ダメージをどのように適用するかを表す種別。
enum class DAMAGE_APPLY_TYPE
{
    SINGLE_HIT,
    MULTI_HIT,
    DAMAGE_OVER_TIME
};
