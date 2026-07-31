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
    AIR_GET_HIT
};

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

enum class DRAGON_DAMAGE_PHASE
{
    NORMAL,
    CRACKED,
    DARK_LEAK,
    COLLAPSE
};

enum class DRAGON_ACTION_FINISH_MODE
{
    SEQUENCER,
    AI_CONDITION,
    AI_DURATION,
    MANUAL
};

struct DragonAIContext
{
    float hpRate = 1.0f;
    DirectX::XMFLOAT3 ownerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 playerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 toPlayer = { 0.0f, 0.0f, 0.0f };
    float playerDistance = 0.0f;
    float playerHorizontalDistance = 0.0f;
    float playerAngleDegrees = 0.0f;

    bool isPlayerInFront = false;
    bool isPlayerInSide = false;
    bool isPlayerBehind = false;
    bool isPlayerUnderBody = false;

    bool canUseBite = true;
    bool canUseFireBall = true;
    bool canUseSpreadBreath = true;
    bool canUseTailSweep = true;
    bool canUseTakeOff = true;
    bool canUseRunCharge = true;
    bool canUseSpinSweep = true;
};

struct DragonActionCandidate
{
    DRAGON_ACTION_TYPE action = DRAGON_ACTION_TYPE::IDLE;
    float weight = 0.0f;
};

struct DragonActionRuntime
{
    DRAGON_ACTION_TYPE currentAction = DRAGON_ACTION_TYPE::IDLE;
    DRAGON_ACTION_TYPE bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    float actionTimer = 0.0f;
    float recoveryTimer = 0.0f;
    bool actionStarted = false;
    bool actionEnded = false;
    bool canCancel = true;
    bool canBufferAction = false;
};

struct DragonActionDefinition
{
    DRAGON_ACTION_TYPE action = DRAGON_ACTION_TYPE::IDLE;
    std::string actionName = "Idle";
    std::string actionSequenceName;
    std::string animationStateName;
    std::string animationActionName;
    std::string ikActionName;
    std::string hitBoxActionName;
    std::string vfxActionName;
    float duration = 1.0f;
    float recoveryTime = 0.35f;
    float cooldownTime = 1.0f;
    float selectionWeight = 1.0f;
    bool enabled = true;
    bool waitForActionEnd = true;
    bool allowBuffer = true;
    bool useActionTimeout = true;
    float actionTimeout = 5.0f;
    DRAGON_ACTION_FINISH_MODE finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
};

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
    void EnsureDependencies();
    void InitializeDefaultDefinitions();
    void ApplyActionDefinitionRuntimePolicies();
    void UpdateContext();
    void UpdateCooldowns(float deltaTime_);
    void UpdateDecision(float deltaTime_);
    void UpdateBehaviorGraph(float deltaTime_);
    DRAGON_ACTION_TYPE SelectNextAirAction() const;
    bool ShouldWaitForIdleAnimation() const;
    bool UpdateIdleAnimationGraphReturn();
    void UpdateActionMotion(float deltaTime_);
    bool UpdateAIControlledActionEnd();
    bool UpdateDurationControlledActionEnd();
    void UpdateActionTimeout();
    void CompleteActionEnd();
    void FaceTargetFromAnimationEvent();
    void MoveForward(float speed_, float deltaTime_);
    void TurnTowardPlayerLimited(float turnSpeed_, float deltaTime_);
    void MoveGround(float speed_, float turnSpeed_, float deltaTime_);
    void MoveAir(float speed_, float turnSpeed_, float deltaTime_);
    void UpdateAirbornePhysics(float deltaTime_);
    void SetAirbornePhysicsEnabled(bool enabled_);
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
    bool IsRepeatLimitedAttack(DRAGON_ACTION_TYPE action_) const;
    float GetCooldown(DRAGON_ACTION_TYPE action_) const;
    void SetCooldown(DRAGON_ACTION_TYPE action_, float cooldownTime_);
    bool IsActionAvailable(DRAGON_ACTION_TYPE action_) const;
    float GetSelectionWeight(DRAGON_ACTION_TYPE action_) const;
    bool HasBehaviorGraph() const;

    std::string playerObjectName = "Player";
    std::string actionCategory = "AI";
    std::string registeredActionCategory;
    std::string blackboardCategory = "DragonAI";

    std::vector<DragonActionDefinition> actionDefinitions;
    std::vector<float> actionCooldowns;
    std::vector<std::string> registeredActionNames;

    DragonAIContext context;
    DragonActionRuntime runtime;
    AIBehaviorGraph behaviorGraph;
    AIBehaviorContext behaviorContext;
    DRAGON_AI_STATE aiState = DRAGON_AI_STATE::IDLE;
    DRAGON_DAMAGE_PHASE damagePhase = DRAGON_DAMAGE_PHASE::NORMAL;

    BlackboardComponent* blackboard = nullptr;
    HealthComponent* health = nullptr;
    std::weak_ptr<GameObject> playerObject;
    std::weak_ptr<AnimationControllerComponent> animationController;
    std::weak_ptr<CharacterMovementComponent> movement;
    std::weak_ptr<DragonNeckCommandIKComponent> neckIK;
    std::weak_ptr<DragonTailCommandIKComponent> tailIK;

    bool autoDecisionEnabled = true;
    bool behaviorGraphEnabled = true;
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
    float turnSpeed = 6.0f;
    float walkSpeed = 5.0f;
    float runSpeed = 12.0f;
    float animationBlendDuration = 0.2f;
    bool animationGraphDriven = true;
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
    bool airbornePhysicsApplied = false;
    bool previousColliderUseGravity = false;

    DRAGON_ACTION_TYPE debugAction = DRAGON_ACTION_TYPE::BITE;
    std::mt19937 randomEngine;
};

const char* GetDragonActionName(DRAGON_ACTION_TYPE action_);
const char* GetDragonAIStateName(DRAGON_AI_STATE state_);
const char* GetDragonDamagePhaseName(DRAGON_DAMAGE_PHASE phase_);
const char* GetDragonActionFinishModeName(DRAGON_ACTION_FINISH_MODE mode_);
