#pragma once
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>

// 入力ではなく、再生中の攻撃イベントから区別した攻撃の進行段階。
enum class ObservedAttackPhase { NONE, STARTUP, ACTIVE, RECOVERY };
enum class ObservedDodgeDirection { NONE, LEFT, RIGHT, BACK, FORWARD };

// 直近の戦闘行動を、シーン内のAIが共有して参照するための記録。
struct ObservedPlayerAction
{
    float time = 0.0f;
    int action = -1;
    bool attack = false;
    bool weakAttack = false;
    bool heavyAttack = false;
    bool guard = false;
    bool dodge = false;
    ObservedDodgeDirection dodgeDirection = ObservedDodgeDirection::NONE;
};

// GameObjectManagerが各Frameの更新前に確定する、ゲーム上のPlayer行動情報。
struct PlayerActionObservation
{
    bool valid = false;
    bool alive = true;
    int action = -1;
    int comboIndex = 0;
    std::string actionName;
    std::string animationState;
    DirectX::XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward{ 0.0f, 0.0f, 1.0f };
    ObservedAttackPhase attackPhase = ObservedAttackPhase::NONE;
    ObservedDodgeDirection dodgeDirection = ObservedDodgeDirection::NONE;
    bool weakAttack = false;
    bool heavyAttack = false;
    bool guarding = false;
    bool dodging = false;
    bool attackMadeContact = false;
    bool airborne = false; // 接地状態と空中Actionの文脈から、着地待ちの追跡を区別する。
    float animationTime = 0.0f;
    float attackAge = 0.0f;
    float timeUntilHit = 0.0f;
    float guardHoldTime = 0.0f;
    float attackFrequency = 0.0f;
    float weakAttackRate = 0.0f;
    float heavyAttackRate = 0.0f;
    float guardFrequency = 0.0f;
    float dodgeFrequency = 0.0f;
    int sameActionCount = 0;
    int consecutiveDodges = 0;
    std::uint64_t serial = 0;
    std::vector<ObservedPlayerAction> history;
};
