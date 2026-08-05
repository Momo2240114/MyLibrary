#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"

#include <DirectXMath.h>
#include <random>
#include <string>
#include <vector>

class BlackboardComponent;
class CharacterMovementComponent;
class AnimationControllerComponent;
class DragonNeckCommandIKComponent;
class DragonTailCommandIKComponent;
class GameObject;
class HealthComponent;
class ColliderComponent;

// Dragonが実行できる行動種別。
enum class DRAGON_ACTION_TYPE
{
    IDLE,
    WALK_APPROACH,
    RUN_CHARGE,
    TURN_TO_PLAYER,
    BITE,
    FIRE_BALL,
    SPREAD_BREATH,
    TURN_TAIL_SWEEP,
    SPIN_TAIL_SWEEP,
    TAKE_OFF,
    AIR_HOLD,
    AIR_FIRE_BALL,
    AIR_SPREAD_BREATH,
    LANDING_ATTACK,
    GET_HIT,
    DEATH,
    FIRE_BALL_SWEEP,
    SPREAD_BREATH_FORWARD,
    AIR_GET_HIT,
    AIR_DEATH_FALL_START,
    FALLING,
    DEATH_HIT_GROUND
};

// DragonAIの大きな実行状態。
enum class DRAGON_AI_STATE
{
    IDLE,
    TURN,
    APPROACH,
    ACTION,
    RECOVERY,
    AIR,
    DEAD
};

// HPや演出状態に応じたDragonの損傷段階。
enum class DRAGON_DAMAGE_PHASE
{
    NORMAL,
    CRACKED,
    DARK_LEAK,
    COLLAPSE
};

// Actionをどの仕組みで終了扱いにするかを示す。
enum class DRAGON_ACTION_FINISH_MODE
{
    SEQUENCER,
    AI_CONDITION,
    AI_DURATION,
    MANUAL
};

// DragonAIが判断に使う現在状況をまとめるContext。
struct DragonAIContext
{
    // 現在HP割合。
    float hpRate = 1.0f;
    // Dragon本体のWorld座標。
    DirectX::XMFLOAT3 ownerPosition = { 0.0f, 0.0f, 0.0f };
    // PlayerのWorld座標。
    DirectX::XMFLOAT3 playerPosition = { 0.0f, 0.0f, 0.0f };
    // DragonからPlayerへ向かう方向。
    DirectX::XMFLOAT3 toPlayer = { 0.0f, 0.0f, 0.0f };
    // Playerまでの3D距離。
    float playerDistance = 0.0f;
    // PlayerまでのXZ平面距離。
    float playerHorizontalDistance = 0.0f;
    // Dragon正面から見たPlayerの角度。
    float playerAngleDegrees = 0.0f;

    // Playerが正面範囲にいるか。
    bool isPlayerInFront = false;
    // Playerが側面範囲にいるか。
    bool isPlayerInSide = false;
    // Playerが背面範囲にいるか。
    bool isPlayerBehind = false;
    // Playerが胴体下にいるか。
    bool isPlayerUnderBody = false;

    // 各Actionが現在使用可能かを示す判定値。
    bool canUseBite = true;
    bool canUseFireBall = true;
    bool canUseSpreadBreath = true;
    bool canUseTailSweep = true;
    bool canUseTakeOff = true;
    bool canUseRunCharge = true;
    bool canUseSpinSweep = true;
};

// 重み付き選択で使うAction候補。
struct DragonActionCandidate
{
    // 候補Action。
    DRAGON_ACTION_TYPE action = DRAGON_ACTION_TYPE::IDLE;
    // 選択重み。
    float weight = 0.0f;
};

// 現在実行中ActionのRuntime状態。
struct DragonActionRuntime
{
    // 現在のAction。
    DRAGON_ACTION_TYPE currentAction = DRAGON_ACTION_TYPE::IDLE;
    // 次に実行するため保存中のAction。
    DRAGON_ACTION_TYPE bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    // Action開始からの経過時間。
    float actionTimer = 0.0f;
    // Recovery状態の残り時間。
    float recoveryTimer = 0.0f;
    // Action開始処理が呼ばれたか。
    bool actionStarted = false;
    // Action終了通知を受けたか。
    bool actionEnded = false;
    // 現Actionから別Actionへキャンセル可能か。
    bool canCancel = true;
    // 入力やAI要求を次ActionとしてBufferできるか。
    bool canBufferAction = false;
};

// DragonActionごとのAnimation、ActionSequence、Cooldown、終了方法をまとめる定義。
struct DragonActionDefinition
{
    // 定義対象のAction種別。
    DRAGON_ACTION_TYPE action = DRAGON_ACTION_TYPE::IDLE;
    // Debug表示やAction解決で使う名前。
    std::string actionName = "Idle";
    // 実行するActionSequence名。
    std::string actionSequenceName;
    // 直接再生するAnimationState名。
    std::string animationStateName;
    // AnimationControllerへ送るAction名。
    std::string animationActionName;
    // IK制御用Action名。
    std::string ikActionName;
    // HitBox制御用Action名。
    std::string hitBoxActionName;
    // VFX制御用Action名。
    std::string vfxActionName;
    // AI_DURATION終了で使う基準時間。
    float duration = 1.0f;
    // Action後のRecovery時間。
    float recoveryTime = 0.35f;
    // 再使用までのCooldown時間。
    float cooldownTime = 1.0f;
    // 重み付き選択で使う基本重み。
    float selectionWeight = 1.0f;
    // falseならこのActionは選択対象外。
    bool enabled = true;
    // PLAY_ACTIONノードがActionEndを待つか。
    bool waitForActionEnd = true;
    // このAction中に次ActionのBufferを許可するか。
    bool allowBuffer = true;
    // Action終了通知が来ない場合にTimeoutを使うか。
    bool useActionTimeout = true;
    // Timeoutまでの最大時間。
    float actionTimeout = 5.0f;
    // Action終了をどの方法で判定するか。
    DRAGON_ACTION_FINISH_MODE finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
};

// Dragon専用AIを制御するComponent。
// BehaviorGraphによる判断、重み付きFallback、Animation/IK/ActionSequence連携、空中状態をまとめて扱う。
class DragonAIComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    bool RequestAction(DRAGON_ACTION_TYPE action_);
    bool RequestWeightedAction();
    void RequestDamageReaction();
    void ForceAction(DRAGON_ACTION_TYPE action_);
    void SetCanCancel(bool enabled_);
    void SetCanBufferAction(bool enabled_);
    void NotifyActionEnd();
    void NotifySequencerActionEnd();
    void NotifyAIActionEnd();
    bool ConsumeBufferedAction();
    void RegisterActions();
    void ResetBehaviorGraphRuntime();
    bool SaveBehaviorGraphAsset(const std::string& path_) const;
    bool LoadBehaviorGraphAsset(const std::string& path_);
    AIBehaviorGraph& GetBehaviorGraph() { return behaviorGraph; }
    const AIBehaviorGraph& GetBehaviorGraph() const { return behaviorGraph; }
    std::string& GetBehaviorGraphAssetPath() { return behaviorGraphAssetPath; }
    const std::string& GetBehaviorGraphAssetPath() const { return behaviorGraphAssetPath; }
    void SetBehaviorGraphAssetPath(const std::string& path_) { behaviorGraphAssetPath = path_; }
    DRAGON_ACTION_TYPE GetCurrentAction() const { return runtime.currentAction; }

    DRAGON_ACTION_TYPE SelectNextAction(const DragonAIContext& context_);
    void AddActionCandidates(
        const DragonAIContext& context_,
        std::vector<DragonActionCandidate>& candidates_) const;

private:
    // 必要なComponent参照を解決する。
    void EnsureDependencies();
    // DragonActionDefinitionの標準値を構築する。
    void InitializeDefaultDefinitions();
    // Action定義からRuntimeのキャンセルやBuffer設定を反映する。
    void ApplyActionDefinitionRuntimePolicies();
    // Player位置、距離、角度、Action使用可否をContextへ更新する。
    void UpdateContext();
    // 各ActionのCooldownを減算する。
    void UpdateCooldowns(float deltaTime_);
    // BehaviorGraphを使わない場合の重み付き判断を進める。
    void UpdateDecision(float deltaTime_);
    // BehaviorGraphを一定間隔でTickする。
    void UpdateBehaviorGraph(float deltaTime_);
    // 空中待機中に次の空中Actionを選ぶ。
    DRAGON_ACTION_TYPE SelectNextAirAction();
    // Idle Animationへ戻るまで次Actionを待つべきか判定する。
    bool ShouldWaitForIdleAnimation() const;
    // AnimationGraphがIdleへ戻ったタイミングでAI判断へ復帰する。
    bool UpdateIdleAnimationGraphReturn();
    // 現Action中の移動や空中制御を更新する。
    void UpdateActionMotion(float deltaTime_);
    // AI_CONDITIONで終了するActionの終了条件を判定する。
    bool UpdateAIControlledActionEnd();
    // AI_DURATIONで終了するActionの時間終了を判定する。
    bool UpdateDurationControlledActionEnd();
    // Sequencer通知が来ない場合のAction Timeoutを処理する。
    void UpdateActionTimeout();
    // Action終了を確定し、Recoveryや次状態へ遷移する。
    void CompleteActionEnd();
    // Animation EventからTarget方向へ向き直す。
    void FaceTargetFromAnimationEvent();
    void MoveForward(float speed_, float deltaTime_);
    void TurnTowardPlayerLimited(float turnSpeed_, float deltaTime_);
    void MoveGround(float speed_, float turnSpeed_, float deltaTime_);
    void MoveAir(float speed_, float turnSpeed_, float deltaTime_);
    void UpdateAirbornePhysics(float deltaTime_);
    void UpdateLandingDescend(float deltaTime_);
    void SetAirbornePhysicsEnabled(bool enabled_);
    bool IsGrounded();
    bool IsReadyToPlayLandingAnimation();
    void StopMovement();
    void StartAction(DRAGON_ACTION_TYPE action_);
    void EnterRecovery();
    void FinishRecovery();
    void ExecuteActionDefinition(const DragonActionDefinition& definition_);
    std::string ResolveAnimationActionName(const DragonActionDefinition& definition_) const;
    std::string ResolveAnimationStateName(const DragonActionDefinition& definition_) const;
    void ApplyAnimationBlackboard(
        const std::string& animationActionName_,
        const std::string& animationStateName_,
        bool actionTrigger_);
    void RequestIdleGraphTransition();
    void RequestAirIdleGraphTransition();
    void PlayAnimationState(const std::string& stateName_);
    void ResetAnimationActionTriggers(const std::string& keepTriggerName_);
    void ExecuteDirectIK(DRAGON_ACTION_TYPE action_, float duration_);
    void ExecuteOwnerAction(const std::string& actionName_);
    void SendRuntimeToBlackboard();
    void EnsureDefaultBehaviorGraph();
    AIBehaviorNode* AddDragonGraphCondition(
        AIBehaviorNode* parentNode_,
        const std::string& name_,
        const std::string& key_,
        AI_BLACKBOARD_COMPARE_TYPE compareType_,
        AI_BLACKBOARD_VALUE_TYPE valueType_,
        float floatValue_,
        bool boolValue_,
        int priority_,
        const DirectX::XMFLOAT2& position_);
    AIBehaviorNode* AddDragonGraphAction(
        AIBehaviorNode* parentNode_,
        const std::string& name_,
        const std::string& actionName_,
        int priority_,
        const DirectX::XMFLOAT2& position_);
    const char* GetBehaviorStatusName(AI_BEHAVIOR_STATUS status_) const;
    void RegisterRuntimeActions();
    DragonActionDefinition* FindDefinition(DRAGON_ACTION_TYPE action_);
    const DragonActionDefinition* FindDefinition(DRAGON_ACTION_TYPE action_) const;
    bool IsAIControlledAction(DRAGON_ACTION_TYPE action_) const;
    bool IsSequencerControlledAction(DRAGON_ACTION_TYPE action_) const;
    bool IsAirborneAction(DRAGON_ACTION_TYPE action_) const;
    bool IsDeathAction(DRAGON_ACTION_TYPE action_) const;
    bool IsRepeatLimitedAttack(DRAGON_ACTION_TYPE action_) const;
    float GetCooldown(DRAGON_ACTION_TYPE action_) const;
    void SetCooldown(DRAGON_ACTION_TYPE action_, float cooldownTime_);
    bool IsActionAvailable(DRAGON_ACTION_TYPE action_) const;
    float GetSelectionWeight(DRAGON_ACTION_TYPE action_) const;
    bool HasBehaviorGraph() const;

    // Targetとして探すPlayer Object名。
    std::string playerObjectName = "Player";
    // Ownerへ登録するDragonAI Actionのカテゴリ名。
    std::string actionCategory = "AI";
    // 前回登録したActionカテゴリ名。
    std::string registeredActionCategory;
    // DragonAI用Blackboardカテゴリ名。
    std::string blackboardCategory = "DragonAI";

    // Dragonが使用するAction定義一覧。
    std::vector<DragonActionDefinition> actionDefinitions;
    // Action種別ごとの残りCooldown。
    std::vector<float> actionCooldowns;
    // Ownerへ登録したAction名一覧。
    std::vector<std::string> registeredActionNames;

    // 現在の判断材料。
    DragonAIContext context;
    // 現在ActionのRuntime状態。
    DragonActionRuntime runtime;
    // Dragon専用BehaviorGraph。
    AIBehaviorGraph behaviorGraph;
    // BehaviorGraphへ渡す実行Context。
    AIBehaviorContext behaviorContext;
    // 現在のAI大状態。
    DRAGON_AI_STATE aiState = DRAGON_AI_STATE::IDLE;
    // 現在の損傷段階。
    DRAGON_DAMAGE_PHASE damagePhase = DRAGON_DAMAGE_PHASE::NORMAL;

    BlackboardComponent* blackboard = nullptr;
    HealthComponent* health = nullptr;
    std::weak_ptr<GameObject> playerObject;
    std::weak_ptr<AnimationControllerComponent> animationController;
    std::weak_ptr<CharacterMovementComponent> movement;
    std::weak_ptr<DragonNeckCommandIKComponent> neckIK;
    std::weak_ptr<DragonTailCommandIKComponent> tailIK;

    // 重み付きFallback判断を有効にするか。
    bool autoDecisionEnabled = true;
    // BehaviorGraphによる判断を有効にするか。
    bool behaviorGraphEnabled = true;
    // Debug用HP割合を使うか。
    bool useDebugHpRate = false;
    float decisionInterval = 0.25f;
    float decisionTimer = 0.0f;
    float behaviorGraphTickInterval = 0.10f;
    float behaviorGraphTickTimer = 0.0f;
    float debugHpRate = 1.0f;
    float frontAngleDegrees = 115.0f;
    float sideAngleDegrees = 135.0f;
    float underBodyDistance = 4.5f;
    float biteDistance = 6.5f;
    float approachEndDistance = 6.0f;
    float mediumDistance = 10.0f;
    float farDistance = 12.0f;
    float takeOffBackSpeed = 7.0f;
    float groundMoveTurnSpeed = 2.0f;
    float airForwardSpeed = 3.0f;
    float airAttackBackSpeed = 2.0f;
    float airTurnSpeed = 75.0f;
    float airHoverHeight = 10.0f;
    float airHoverAdjustSpeed = 8.0f;
    float airLandingAnimationStartHeight = 0.5f;
    float airLandingDescendSpeed = 12.0f;
    float turnSpeed = 6.0f;
    float walkSpeed = 5.0f;
    float runSpeed = 12.0f;
    float animationBlendDuration = 0.2f;
    bool animationGraphDriven = true;
    // 読み書きするDragon BehaviorGraph Assetパス。
    std::string behaviorGraphAssetPath = "Data/AI/BehaviorGraph/DragonFirstForm.json";
    std::string behaviorGraphStatus;
    std::string statusText;
    AI_BEHAVIOR_STATUS lastBehaviorStatus = AI_BEHAVIOR_STATUS::FAILURE;
    DRAGON_ACTION_TYPE lastStartedAction = DRAGON_ACTION_TYPE::IDLE;
    DRAGON_ACTION_TYPE lastRejectedAction = DRAGON_ACTION_TYPE::IDLE;
    DRAGON_ACTION_TYPE lastCompletedAttackAction = DRAGON_ACTION_TYPE::IDLE;
    int behaviorGraphTickCount = 0;
    int actionStartCount = 0;
    bool hasPlayerTarget = false;
    bool hasAnimationController = false;
    bool hasMovementComponent = false;
    bool hasNeckIK = false;
    bool hasTailIK = false;
    bool pendingSequencerActionEnd = false;
    bool pendingAirAttackDecision = false;
    bool pendingAirLandingDecision = false;
    bool waitingForLandingGround = false;
    bool airbornePhysicsApplied = false;

    DRAGON_ACTION_TYPE debugAction = DRAGON_ACTION_TYPE::BITE;
    std::mt19937 randomEngine;
};

const char* GetDragonActionName(DRAGON_ACTION_TYPE action_);
const char* GetDragonAIStateName(DRAGON_AI_STATE state_);
const char* GetDragonDamagePhaseName(DRAGON_DAMAGE_PHASE phase_);
const char* GetDragonActionFinishModeName(DRAGON_ACTION_FINISH_MODE mode_);
