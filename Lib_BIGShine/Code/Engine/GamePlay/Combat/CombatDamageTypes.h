#pragma once

#include <string>
#include <nlohmann/json.hpp>

// 攻撃開始と命中時の振動、およびカメラを基準に再生する攻撃音を保持する。
struct AttackFeedbackSettings
{
    bool rumbleEnabled = true;
    float startStrength = 0.15f;
    float attackStrength = 0.3f;
    float receiveStrength = 0.65f;
    float duration = 0.12f;
    std::string startSoundPath;
    std::string hitSoundPath;
    float volume = 1.0f;
    float minDistance = 3.0f;
    float maxDistance = 40.0f;
};

// 不足項目は既定値で補い、追加前のJSONも復元できるようにする。
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AttackFeedbackSettings,
    rumbleEnabled, startStrength, attackStrength, receiveStrength, duration,
    startSoundPath, hitSoundPath, volume, minDistance, maxDistance)


// 攻撃ダメージをどのように適用するかを表す種別。
enum class DAMAGE_APPLY_TYPE
{
    SINGLE_HIT,
    MULTI_HIT,
    DAMAGE_OVER_TIME
};
