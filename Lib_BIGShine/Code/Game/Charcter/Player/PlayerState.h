#pragma once
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>

class PlayerControllerComponent;

// PlayerActionは、Playerの入力とミラーAIで共有される予定の行動種別。
// この列挙定義からは、入力バインディングを直接参照しない。
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

    NONE,
    SKILL_C
};

// Actionを移動、攻撃、ガードなどの大きな分類で扱うための種別。
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

// 攻撃ActionをコンボやTimeline側で扱いやすい攻撃種別へ分類する。
enum class PLAYER_ATTACK_TYPE
{
    NONE,
    WEAK,
    STRONG,
    STRONG_CHARGE,
    COUNTER,
    SKILL_A,
    SKILL_B,
    SKILL_C
};

// 移動Action内でAnimation遷移に使用する詳細フェーズ。
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
    // Player状態や入力値を保存するカテゴリ。
    inline constexpr const char* PLAYER = "Player";
    // AnimationStateMachineへ渡すパラメータを保存するカテゴリ。
    inline constexpr const char* ANIM = "Anim";
}

namespace PlayerBlackboardKey
{
    // 共通SkillStateの実行状況。行動種別は既存ActionとIsSkillを再利用する。
    inline constexpr const char* CURRENT_SKILL = "CurrentSkill";
    inline constexpr const char* SKILL_PHASE = "SkillPhase";
    inline constexpr const char* IS_SKILL_ACTIVE = "IsSkillActive";
    // 現在のPlayerAction名。
    inline constexpr const char* ACTION = "Action";
    // 現在Actionの大分類。
    inline constexpr const char* ACTION_GROUP = "ActionGroup";
    // 攻撃Actionの場合に使用する攻撃種別。
    inline constexpr const char* ATTACK_TYPE = "AttackType";
    // コンボ中の現在段数。
    inline constexpr const char* COMBO_INDEX = "ComboIndex";
    // 通常移動入力ベクトル。
    inline constexpr const char* MOVE_INPUT = "MoveInput";
    // ガード中の移動入力ベクトル。
    inline constexpr const char* GUARD_MOVE_INPUT = "GuardMoveInput";
    // 回避方向の入力ベクトル。
    inline constexpr const char* DODGE_INPUT = "DodgeInput";
    // ガード入力が押されているかを示す。
    inline constexpr const char* GUARD_INPUT = "GuardInput";
    // 移動Action内の詳細フェーズ。
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
    inline constexpr const char* IS_JUST_GUARD = "IsJustGuard";
    inline constexpr const char* IS_DODGE = "IsDodge";
    inline constexpr const char* IS_AIR_ACTION = "IsAirAction";
    inline constexpr const char* IS_AIR_ATTACK = "IsAirAttack";
    inline constexpr const char* IS_AIR_WEAK_ATTACK = "IsAirWeakAttack";
    inline constexpr const char* IS_AIR_STRONG_ATTACK = "IsAirStrongAttack";
    inline constexpr const char* IS_AIR_STRONG_CHARGE_ATTACK = "IsAirStrongChargeAttack";
    inline constexpr const char* IS_AIR_DODGE = "IsAirDodge";
    inline constexpr const char* IS_SKILL = "IsSkill";
    inline constexpr const char* CAN_CANCEL = "CanCancel";
    inline constexpr const char* CAN_DODGE_CANCEL = "CanDodgeCancel";
    inline constexpr const char* CAN_ACTION_BRANCH = "CanActionBranch";
    inline constexpr const char* CAN_BUFFER_INPUT = "CanBufferInput";
    inline constexpr const char* CAN_COMBO = "CanCombo";
    inline constexpr const char* ACTION_ENDED = "ActionEnded";
    inline constexpr const char* ATTACK_ENTER = "AttackEnter";

    inline constexpr const char* HIT_TRIGGER = "HitTrigger";
    inline constexpr const char* HIT_DIRECTION = "HitDirection";
    inline constexpr const char* HIT_IMPACT_POWER = "HitImpactPower";
    inline constexpr const char* HIT_ARMOR = "HitArmor";
    inline constexpr const char* HIT_REACTION_LEVEL = "HitReactionLevel";
    inline constexpr const char* IS_HIT_LIGHT = "IsHitLight";
    inline constexpr const char* IS_HIT_MEDIUM = "IsHitMedium";
    inline constexpr const char* IS_HIT_HEAVY = "IsHitHeavy";
    inline constexpr const char* IS_AIR_HIT_REACTION = "IsAirHitReaction";
    inline constexpr const char* IS_AIR_HIT_RECOVERABLE = "IsAirHitRecoverable";
    inline constexpr const char* IS_AIR_HIT_KNOCKDOWN = "IsAirHitKnockdown";
    // ジャスト回避に関係するBlackboardキー。
    inline constexpr const char* JUST_DODGE_SUCCEEDED = "JustDodgeSucceeded";
    inline constexpr const char* JUST_DODGE_TRIGGER = "JustDodgeTrigger";
    // カウンターに関係するBlackboardキー。
    inline constexpr const char* CAN_COUNTER = "CanCounter";
    inline constexpr const char* COUNTER_TAG = "CounterTag";
    inline constexpr const char* COUNTER_TAG_SOURCE = "CounterTagSource";
    inline constexpr const char* DODGED_COUNTER_TAG = "DodgedCounterTag";
    inline constexpr const char* GUARDED_COUNTER_TAG = "GuardedCounterTag";
    inline constexpr const char* JUST_GUARD_COUNTER_TAG = "JustGuardCounterTag";
}

// PlayerActionをログやBlackboardへ書き込む表示名へ変換する。
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
    case PLAYER_ACTION::SKILL_C: return "SkillC";
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

// PlayerActionを、移動・攻撃・ガードなどの大分類へ変換する。
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
    case PLAYER_ACTION::SKILL_C:
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

// 攻撃Actionを弱攻撃、強攻撃、スキルなどの攻撃種別へ変換する。
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
    case PLAYER_ACTION::SKILL_C:
        return PLAYER_ATTACK_TYPE::SKILL_C;
    default:
        return PLAYER_ATTACK_TYPE::NONE;
    }
}

// TimelineやActionSequenceの終了通知で進行するActionかを判定する。
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
    case PLAYER_ACTION::SKILL_C:
    case PLAYER_ACTION::COUNTER_ATTACK:
    case PLAYER_ACTION::HIT:
        return true;
    default:
        return false;
    }
}

// PlayerControllerから実行されるPlayer用Stateの基底クラス。
// Owner参照、Controller参照、Component取得キャッシュを共通化する。
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

    // Stateへ入った瞬間に、AnimationやBlackboardなどを初期化する。
    virtual void Enter() = 0;
    // State中の毎フレーム処理。
    virtual void Execute(float elapsedTime_) = 0;
    virtual void LateExecute(float et_) {}
    virtual void Exit() {}

protected:
    // Ownerが破棄済みの場合はnullptrを返す。
    std::shared_ptr<GameObject> GetOwner() const { return owner.lock(); }
    PlayerControllerComponent* GetPlayerController() const { return playerController; }

    template<class T>
    std::shared_ptr<T> GetComponent() const
    {
        // 同じComponentを何度も検索しないよう、型ごとにweak参照でキャッシュする。
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

        // キャッシュが無効な場合だけGameObjectから取り直し、結果を次回用に保存する。
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
    // Stateから入力状態やAction制御を参照するためのController。
    PlayerControllerComponent* playerController = nullptr;
    // 型ごとのComponent取得結果を保持する弱参照キャッシュ。
    mutable std::unordered_map<std::type_index, std::weak_ptr<Component>> componentCache;
};
