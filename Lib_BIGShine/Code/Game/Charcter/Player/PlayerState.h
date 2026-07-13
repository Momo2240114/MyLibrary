#pragma once
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

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
    HIT,
    JUMP,
    STRONG_CHARGE_ATTACK,
    AIR_WEAK_ATTACK,
    AIR_STRONG_ATTACK,
    AIR_STRONG_CHARGE_ATTACK,
    AIR_DODGE,

    NONE
};

enum class PLAYER_ACTION_GROUP
{
    LOCOMOTION,
    ATTACK,
    GUARD,
    DODGE,
    JUMP,
    SKILL,
    NONE
};

enum class PLAYER_ATTACK_TYPE
{
    NONE,
    WEAK,
    STRONG,
    STRONG_CHARGE,
    COUNTER,
    SKILL_A,
    SKILL_B
};

enum class PLAYER_LOCOMOTION_PHASE
{
    IDLE,
    RUN_START,
    RUN_LOOP,
    RUN_END,
    JUMP,
    JUMP_START,
    JUMP_LOOP,
    JUMP_END,
    JUMP_DOUBLE
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
    inline constexpr const char* DODGE_INPUT = "DodgeInput";
    inline constexpr const char* LOCOMOTION_PHASE = "LocomotionPhase";
    inline constexpr const char* IS_RUN_START = "IsRunStart";
    inline constexpr const char* IS_RUN_LOOP = "IsRunLoop";
    inline constexpr const char* IS_RUN_END = "IsRunEnd";
    inline constexpr const char* IS_JUMP = "IsJump";
    inline constexpr const char* IS_JUMP_START = "IsJumpStart";
    inline constexpr const char* IS_JUMP_DOUBLE = "IsJumpDouble";
    inline constexpr const char* IS_JUMP_LOOP = "IsJumpLoop";
    inline constexpr const char* IS_JUMP_END = "IsJumpEnd";
    inline constexpr const char* IS_GROUNDED = "IsGrounded";
    inline constexpr const char* IS_IDLE = "IsIdle";
    inline constexpr const char* IS_MOVE = "IsMove";
    inline constexpr const char* IS_LOCOMOTION = "IsLocomotion";
    inline constexpr const char* IS_ATTACK = "IsAttack";
    inline constexpr const char* IS_GUARD = "IsGuard";
    inline constexpr const char* IS_DODGE = "IsDodge";
    inline constexpr const char* IS_AIR_ACTION = "IsAirAction";
    inline constexpr const char* IS_AIR_ATTACK = "IsAirAttack";
    inline constexpr const char* IS_AIR_WEAK_ATTACK = "IsAirWeakAttack";
    inline constexpr const char* IS_AIR_STRONG_ATTACK = "IsAirStrongAttack";
    inline constexpr const char* IS_AIR_STRONG_CHARGE_ATTACK = "IsAirStrongChargeAttack";
    inline constexpr const char* IS_AIR_DODGE = "IsAirDodge";
    inline constexpr const char* IS_SKILL = "IsSkill";
    inline constexpr const char* CAN_CANCEL = "CanCancel";
    inline constexpr const char* CAN_BUFFER_INPUT = "CanBufferInput";
    inline constexpr const char* CAN_COMBO = "CanCombo";
    inline constexpr const char* ACTION_ENDED = "ActionEnded";
    inline constexpr const char* ATTACK_ENTER = "AttackEnter";

    inline constexpr const char* HIT_TRIGGER = "HitTrigger";
    inline constexpr const char* HIT_DIRECTION = "HitDirection";
    //ジャスト回避関係
    inline constexpr const char* JUST_DODGE_SUCCEEDED = "JustDodgeSucceeded";
    inline constexpr const char* JUST_DODGE_TRIGGER = "JustDodgeTrigger";
    //カウンター関係
    inline constexpr const char* CAN_COUNTER = "CanCounter";
    inline constexpr const char* DODGED_COUNTER_TAG = "DodgedCounterag";
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
    case PLAYER_ACTION::JUMP: return "Jump";
    case PLAYER_ACTION::SKILL_A: return "SkillA";
    case PLAYER_ACTION::SKILL_B: return "SkillB";
    case PLAYER_ACTION::STRONG_CHARGE_ATTACK: return "StrongChargeAttack";
    case PLAYER_ACTION::AIR_WEAK_ATTACK: return "AirWeakAttack";
    case PLAYER_ACTION::AIR_STRONG_ATTACK: return "AirStrongAttack";
    case PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK: return "AirStrongChargeAttack";
    case PLAYER_ACTION::AIR_DODGE: return "AirDodge";
    case PLAYER_ACTION::COUNTER_ATTACK: return "CounterAttack";
    case PLAYER_ACTION::HIT: return "Hit";
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
    case PLAYER_ACTION::AIR_WEAK_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK:
    case PLAYER_ACTION::COUNTER_ATTACK:
    case PLAYER_ACTION::SKILL_A:
    case PLAYER_ACTION::SKILL_B:
        return PLAYER_ACTION_GROUP::ATTACK;
    case PLAYER_ACTION::HIT:
        return PLAYER_ACTION_GROUP::NONE;
    case PLAYER_ACTION::GUARD:
    case PLAYER_ACTION::JUST_GUARD:
        return PLAYER_ACTION_GROUP::GUARD;
    case PLAYER_ACTION::DODGE:
    case PLAYER_ACTION::AIR_DODGE:
        return PLAYER_ACTION_GROUP::DODGE;
    case PLAYER_ACTION::JUMP:
        return PLAYER_ACTION_GROUP::JUMP;
    default:
        return PLAYER_ACTION_GROUP::NONE;
    }
}

inline PLAYER_ATTACK_TYPE GetPlayerAttackType(PLAYER_ACTION action_)
{
    switch (action_)
    {
    case PLAYER_ACTION::WEAK_ATTACK:
    case PLAYER_ACTION::AIR_WEAK_ATTACK:
        return PLAYER_ATTACK_TYPE::WEAK;
    case PLAYER_ACTION::STRONG_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_ATTACK:
        return PLAYER_ATTACK_TYPE::STRONG;
    case PLAYER_ACTION::STRONG_CHARGE_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK:
        return PLAYER_ATTACK_TYPE::STRONG_CHARGE;
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
    case PLAYER_ACTION::STRONG_CHARGE_ATTACK:
    case PLAYER_ACTION::AIR_WEAK_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_ATTACK:
    case PLAYER_ACTION::AIR_STRONG_CHARGE_ATTACK:
    case PLAYER_ACTION::JUST_GUARD:
    case PLAYER_ACTION::DODGE:
    case PLAYER_ACTION::AIR_DODGE:
    case PLAYER_ACTION::SKILL_A:
    case PLAYER_ACTION::SKILL_B:
    case PLAYER_ACTION::COUNTER_ATTACK:
    case PLAYER_ACTION::HIT:
        return true;
    default:
        return false;
    }
}

class PlayerState
{
public:
    PlayerState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_)
        : owner(owner_),
        playerController(&playerController_)
    {
    }

    virtual ~PlayerState() = default;

    virtual const char* GetName() const = 0;

    virtual void Enter() = 0;
    virtual void Execute(float elapsedTime_) = 0;
    virtual void LateExecute(float et_) {}
    virtual void Exit() {}

protected:
    std::shared_ptr<GameObject> GetOwner() const { return owner.lock(); }
    PlayerControllerComponent* GetPlayerController() const { return playerController; }

    template<class T>
    std::shared_ptr<T> GetComponent() const
    {
        const std::type_index type_ = std::type_index(typeid(T));
        const auto cachedIterator_ = componentCache.find(type_);
        if (cachedIterator_ != componentCache.end())
        {
            const std::shared_ptr<Component> cachedComponent_ =
                cachedIterator_->second.lock();
            if (cachedComponent_)
            {
                return std::dynamic_pointer_cast<T>(cachedComponent_);
            }
        }

        const std::shared_ptr<GameObject> owner_ = GetOwner();
        if (!owner_) return nullptr;

        const std::shared_ptr<T> component_ = owner_->GetComponent<T>();
        if (component_)
        {
            componentCache[type_] = component_;
        }
        else
        {
            componentCache.erase(type_);
        }
        return component_;
    }

    std::weak_ptr<GameObject> owner;
    PlayerControllerComponent* playerController = nullptr;
    mutable std::unordered_map<std::type_index, std::weak_ptr<Component>> componentCache;
};
