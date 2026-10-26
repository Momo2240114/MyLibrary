#pragma once
#include "Engine/GamePlay/Combat/SkillExecution.h"

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"

#include <DirectXMath.h>
#include <random>
#include <string>
#include <vector>

class BlackboardComponent;
class CharacterMovementComponent;
class AnimationGraphBridgeComponent;
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
    DEATH_HIT_GROUND,
    ULTIMATE_ATTACK,
    FIRE_BALL_FRONT,
    FIRE_BALL_TRIPLE,
    FIRE_BEAM_SWEEP,
    GROUND_STAMP,
    ACTION_COUNT
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
    bool canUseGroundStamp = true;
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

// 直近の選択時に使用した重みを、実行後の履歴更新と分離して保持する。
struct DragonActionSelectionDebug
{
    float baseWeight = 0.0f;
    float historyModifier = 1.0f;
    float finalWeight = 0.0f;
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
    // 空なら従来経路を使い、指定時だけ共通Skill Definitionを実行する。
    std::string skillName;
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
    // この行動へ最近の使用履歴による抽選補正を適用する。
    bool useHistory = true;
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
    // WorkspaceからAI Actionへ共通スキルを割り当てる。
    bool SetSkillForAction(DRAGON_ACTION_TYPE action_, const std::string& skillName_, const std::string& path_);
    bool ReloadSkillDefinitions(const std::string& path_);
    void StopSkill();
    const std::vector<DragonActionDefinition>& GetActionDefinitions() const { return actionDefinitions; }
    const SkillExecution& GetSkillExecution() const { return skillExecution; }
    void RequestDamageReaction();
    bool IsUltimateDamageReductionActive() const { return ultimateDamageReductionActive; }
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
    // 回転攻撃の予兆を、AIが確定した本体の回転角度と方向へ合わせる。
    float GetSpinTailRotationDegrees() const { return spinTailRotationDegrees; }
    float GetSpinTailRotationDirection() const { return spinTailRotationDirection; }

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
    // HPしきい値を跨いだ時に、必殺技へ移行するための予約を行う。
    void UpdateUltimateTrigger();
    // 各ActionのCooldownを減算する。
    void UpdateCooldowns(float deltaTime_);
    // BehaviorGraphを使わない場合の重み付き判断を進める。
    void UpdateDecision(float deltaTime_);
    // BehaviorGraphを一定間隔でTickする。
    void UpdateBehaviorGraph(float deltaTime_);
    // 空中待機中に次の空中Actionを選ぶ。
    DRAGON_ACTION_TYPE SelectNextAirAction();
    // 空中攻撃へ移る前に、飛行用の目標高度へ到達しているか確認する。
    bool IsAirHoverHeightReached();
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
    void BeginTurnRootMotionLimit(DRAGON_ACTION_TYPE action_);
    void ClearTurnRootMotionLimit();
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
    // RootMotionを使わない回転尻尾攻撃の本体Yawをコード側で補間する。
    void BeginSpinTailSweepRotation();
    void UpdateSpinTailSweepRotation(float deltaTime_);
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
    // シーケンサー内イベントから、現在のAI行動を維持したまま次のアニメーションステートへ進める。
    void PlaySequencerLinkedState(const std::string& stateName_);
    void ResetAnimationActionTriggers(const std::string& keepTriggerName_);
    void ExecuteDirectIK(DRAGON_ACTION_TYPE action_, float duration_);
    void ExecuteOwnerAction(const std::string& actionName_);
    // 噛みつき攻撃用の頭HitBoxを、アニメーションイベントから開閉する。
    void EnableBiteHitBox();
    void DisableBiteHitBox();
    // 尻尾攻撃用の複数HitBoxを、アニメーションイベントから同時に開閉する。
    void EnableTailHitBoxes();
    void DisableTailHitBoxes();
    void DisableSequencerHitBoxes();
    void StartSequencerSpinTailIK();
    void ClearTailIK();
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
    // アニメーションステート上の仮イベントから、現在Actionに対応する攻撃Actionを発火する。
    void PlaySequencerAttackAction();
    // アニメーションステート上の仮イベントから、現在Actionに対応する予兆表示を発火する。
    void ShowSequencerDangerAction();
    DragonActionDefinition* FindDefinition(DRAGON_ACTION_TYPE action_);
    const DragonActionDefinition* FindDefinition(DRAGON_ACTION_TYPE action_) const;
    bool IsAIControlledAction(DRAGON_ACTION_TYPE action_) const;
    bool IsSequencerControlledAction(DRAGON_ACTION_TYPE action_) const;
    bool IsAirborneAction(DRAGON_ACTION_TYPE action_) const;
    bool IsDeathAction(DRAGON_ACTION_TYPE action_) const;
    bool IsEnemyAILockedByGameMode() const;
    bool IsRepeatLimitedAttack(DRAGON_ACTION_TYPE action_) const;
    // 地上接近を開始・継続できる状況か判定する。
    bool CanUseGroundApproach(const DragonAIContext& context_) const;
    // 近距離の正面斜めで、攻撃前に向き直るべきか判定する。
    bool ShouldTurnForCloseFrontTarget(const DragonAIContext& context_) const;
    // 尻尾薙ぎ払いが届く距離と向きにPlayerがいるか判定する。
    bool CanUseTurnTailSweep(const DragonAIContext& context_) const;
    // 回転尻尾攻撃が届く距離と向きにPlayerがいるか判定する。
    bool CanUseSpinTailSweep(const DragonAIContext& context_) const;
    // 側面や背面からのターン要求を反撃行動へ置き換えられるか判定する。
    DRAGON_ACTION_TYPE ResolveAntiFlankAction();
    float GetCooldown(DRAGON_ACTION_TYPE action_) const;
    void SetCooldown(DRAGON_ACTION_TYPE action_, float cooldownTime_);
    bool IsActionAvailable(DRAGON_ACTION_TYPE action_) const;
    float GetSelectionWeight(DRAGON_ACTION_TYPE action_) const;
    float CalculateAggressionMultiplier(const DragonAIContext& context_) const;
    float CalculateActionSituationWeight(
        DRAGON_ACTION_TYPE action_,
        const DragonAIContext& context_,
        float baseWeight_) const;
    void AddWeightedActionCandidate(
        const DragonAIContext& context_,
        std::vector<DragonActionCandidate>& candidates_,
        DRAGON_ACTION_TYPE action_,
        float baseWeight_) const;
    bool HasBehaviorGraph() const;
    // 実際に開始した攻撃だけを数え、移動や選択失敗では履歴を進めない。
    void RecordActionHistory(DRAGON_ACTION_TYPE action_);
    bool UsesActionHistory(DRAGON_ACTION_TYPE action_) const;
    float CalculateHistoryModifier(DRAGON_ACTION_TYPE action_) const;
    // 現在の条件を満たす候補全体から連続使用を除外する。
    void PrepareHistorySelection(const std::vector<DRAGON_ACTION_TYPE>& actions_);
    DRAGON_ACTION_TYPE SelectHistoryWeightedAction(std::vector<DragonActionCandidate>& candidates_);
    float ResolveGraphActionModifier(const std::string& actionName_) const;
    void DrawHistoryDebug();

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
    // 共通PhaseはDragon AIのUpdateから一度だけ進める。
    SkillExecution skillExecution;
    std::vector<SkillDefinition> skillDefinitions;
    std::string skillDefinitionsPath = "Data/Skills/PlayerSkills.json";
    bool skillActionRunning = false;
    // Action種別ごとの残りCooldown。
    std::vector<float> actionCooldowns;
    // 履歴は戦闘中だけ保持し、シーン保存時には保存しない。
    std::vector<int> actionLastUsedOrders;
    std::vector<bool> historySelectionAvailable;
    std::vector<DragonActionSelectionDebug> selectionDebugWeights;
    int attackExecutionOrder = 0;
    DRAGON_ACTION_TYPE lastExecutedAttack = DRAGON_ACTION_TYPE::IDLE;
    bool excludePreviousAttack = false;
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
    std::weak_ptr<AnimationGraphBridgeComponent> animationGraph;
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
    float underBodyDistance = 3.2f;
    float biteDistance = 4.8f;
    float approachEndDistance = 5.2f;
    float mediumDistance = 16.0f;
    float farDistance = 22.0f;
    float takeOffBackSpeed = 7.0f;
    float groundMoveTurnSpeed = 2.0f;
    float airForwardSpeed = 3.0f;
    float airAttackBackSpeed = 2.0f;
    float airTurnSpeed = 75.0f;
    float airHoverHeight = 10.0f;
    float airHoverAdjustSpeed = 8.0f;
    float airLandingAnimationStartHeight = 0.5f;
    float airLandingDescendSpeed = 12.0f;
    float airHoverHeightTolerance = 0.25f;
    int maxAirAttackCount = 1;
    float firstUltimateHpRate = 0.5f;
    float secondUltimateHpRate = 0.1f;
    float ultimateDamageRate = 0.1f;
    float turnSpeed = 6.0f;
    float walkSpeed = 5.0f;
    float runSpeed = 12.0f;
    float animationBlendDuration = 0.2f;
    bool limitTurnRootMotion = true;
    float turnRootMotionLimitPaddingDegrees = 2.0f;
    // HP低下やプレイヤー位置に応じて、怪物らしい圧力の強弱を調整する。
    float crackedAggressionMultiplier = 1.10f;
    float darkLeakAggressionMultiplier = 1.25f;
    float collapseAggressionMultiplier = 1.45f;
    float repeatActionPenaltyRate = 0.35f;
    bool historyEnabled = true;
    int historyRecoveryCount = 3;
    int historyStarvationCount = 5;
    float historyMaxBonus = 4.0f;
    // 各定義のCooldownTimeは維持し、攻撃開始時だけ倍率を適用する。
    float attackCooldownMultiplier = 5.0f;
    float underBodyPunishMultiplier = 1.60f;
    float sidePunishMultiplier = 1.35f;
    float behindPunishMultiplier = 1.70f;
    float longRangePressureMultiplier = 1.45f;
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
    bool firstUltimateTriggered = false;
    bool secondUltimateTriggered = false;
    bool pendingUltimateAction = false;
    bool ultimateDamageReductionActive = false;
    bool hasPreviousHpRateForUltimate = false;
    float previousHpRateForUltimate = 1.0f;
    float turnRootMotionLimitRemainingDegrees = 0.0f;
    bool turnRootMotionLimitActive = false;
    float spinTailRotationDegrees = 360.0f;
    float spinTailRotationDirection = 1.0f;
    float spinTailRotationAppliedYaw = 0.0f;
    int airAttackCount = 0;

    DRAGON_ACTION_TYPE debugAction = DRAGON_ACTION_TYPE::BITE;
    std::mt19937 randomEngine;
};

const char* GetDragonActionName(DRAGON_ACTION_TYPE action_);
const char* GetDragonAIStateName(DRAGON_AI_STATE state_);
const char* GetDragonDamagePhaseName(DRAGON_DAMAGE_PHASE phase_);
const char* GetDragonActionFinishModeName(DRAGON_ACTION_FINISH_MODE mode_);
