#pragma once
#include <memory>
#include <string>

class GameObject;
class PlayerControllerComponent;

// PlayerAction は、プレイヤーの入力とミラーAIで共有されされる予定。このフロー定義からは、
// 入力バインディングを参照しません
enum class PLAYER_ACTION
{
    IDLE,
    MOVE,
    WEAK_ATTACK,
    STRONG_ATTACK,
    GUARD,
    JUST_GUARD,
    DODGE,
    SKILL_A,
    SKILL_B,
    COUNTER_ATTACK,

    NONE
};

enum class PLAYER_ACTION_GROUP
{
    LOCOMOTION,
    ATTACK,
    GUARD,
    DODGE,
    SKILL,
    NONE
};

enum class PLAYER_ATTACK_TYPE
{
    NONE,
    WEAK,
    STRONG,
    COUNTER,
    SKILL_A,
    SKILL_B
};

namespace PlayerBlackboardCategory
{
    inline constexpr const char* PLAYER = "Player";
    inline constexpr const char* ANIM = "Anim";
}

namespace PlayerBlackboardKey
{
    inline constexpr const char* ACTION = "Action";
    inline constexpr const char* ACTION_GROUP = "ActionGroup";
    inline constexpr const char* ATTACK_TYPE = "AttackType";
    inline constexpr const char* COMBO_INDEX = "ComboIndex";
    inline constexpr const char* MOVE_INPUT = "MoveInput";
    inline constexpr const char* IS_IDLE = "IsIdle";
    inline constexpr const char* IS_MOVE = "IsMove";
    inline constexpr const char* IS_LOCOMOTION = "IsLocomotion";
    inline constexpr const char* IS_ATTACK = "IsAttack";
    inline constexpr const char* IS_GUARD = "IsGuard";
    inline constexpr const char* IS_DODGE = "IsDodge";
    inline constexpr const char* IS_SKILL = "IsSkill";
    inline constexpr const char* CAN_CANCEL = "CanCancel";
    inline constexpr const char* CAN_BUFFER_INPUT = "CanBufferInput";
    inline constexpr const char* CAN_COMBO = "CanCombo";
    inline constexpr const char* ACTION_ENDED = "ActionEnded";
    inline constexpr const char* ATTACK_ENTER = "AttackEnter";
}

inline const char* GetPlayerActionName(PLAYER_ACTION action_)
{
    switch (action_)
    {
    case PLAYER_ACTION::IDLE: return "Idle";
    case PLAYER_ACTION::MOVE: return "Move";
    case PLAYER_ACTION::WEAK_ATTACK: return "WeakAttack";
    case PLAYER_ACTION::STRONG_ATTACK: return "StrongAttack";
    case PLAYER_ACTION::GUARD: return "Guard";
    case PLAYER_ACTION::JUST_GUARD: return "JustGuard";
    case PLAYER_ACTION::DODGE: return "Dodge";
    case PLAYER_ACTION::SKILL_A: return "SkillA";
    case PLAYER_ACTION::SKILL_B: return "SkillB";
    case PLAYER_ACTION::COUNTER_ATTACK: return "CounterAttack";
    default: return "None";
    }
}

inline PLAYER_ACTION_GROUP GetPlayerActionGroup(PLAYER_ACTION action_)
{
    switch (action_)
    {
    case PLAYER_ACTION::IDLE:
    case PLAYER_ACTION::MOVE:
        return PLAYER_ACTION_GROUP::LOCOMOTION;
    case PLAYER_ACTION::WEAK_ATTACK:
    case PLAYER_ACTION::STRONG_ATTACK:
    case PLAYER_ACTION::COUNTER_ATTACK:
    case PLAYER_ACTION::SKILL_A:
    case PLAYER_ACTION::SKILL_B:
        return PLAYER_ACTION_GROUP::ATTACK;
    case PLAYER_ACTION::GUARD:
    case PLAYER_ACTION::JUST_GUARD:
        return PLAYER_ACTION_GROUP::GUARD;
    case PLAYER_ACTION::DODGE:
        return PLAYER_ACTION_GROUP::DODGE;
    default:
        return PLAYER_ACTION_GROUP::NONE;
    }
}

inline PLAYER_ATTACK_TYPE GetPlayerAttackType(PLAYER_ACTION action_)
{
    switch (action_)
    {
    case PLAYER_ACTION::WEAK_ATTACK:
        return PLAYER_ATTACK_TYPE::WEAK;
    case PLAYER_ACTION::STRONG_ATTACK:
        return PLAYER_ATTACK_TYPE::STRONG;
    case PLAYER_ACTION::COUNTER_ATTACK:
        return PLAYER_ATTACK_TYPE::COUNTER;
    case PLAYER_ACTION::SKILL_A:
        return PLAYER_ATTACK_TYPE::SKILL_A;
    case PLAYER_ACTION::SKILL_B:
        return PLAYER_ATTACK_TYPE::SKILL_B;
    default:
        return PLAYER_ATTACK_TYPE::NONE;
    }
}

inline bool IsPlayerActionTimelineDriven(PLAYER_ACTION action_)
{
    switch (action_)
    {
    case PLAYER_ACTION::WEAK_ATTACK:
    case PLAYER_ACTION::STRONG_ATTACK:
    case PLAYER_ACTION::JUST_GUARD:
    case PLAYER_ACTION::DODGE:
    case PLAYER_ACTION::SKILL_A:
    case PLAYER_ACTION::SKILL_B:
    case PLAYER_ACTION::COUNTER_ATTACK:
        return true;
    default:
        return false;
    }
}

class PlayerState
{
public:
    virtual ~PlayerState() = default;

    virtual const char* GetName() const = 0;

    void Bind(PlayerControllerComponent& player_);

    virtual void Enter() = 0;
    virtual void Execute(float elapsedTime_) = 0;
    virtual void LateExecute(float et_) {}
    virtual void Exit() {}

protected:
    std::shared_ptr<GameObject> GetOwner() const { return owner.lock(); }
    PlayerControllerComponent* GetPlayerController() const { return playerController; }

    std::weak_ptr<GameObject> owner;
    PlayerControllerComponent* playerController = nullptr;
};
