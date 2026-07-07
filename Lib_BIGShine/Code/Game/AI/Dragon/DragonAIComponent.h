#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <random>
#include <string>
#include <vector>

class BlackboardComponent;
class GameObject;
class HealthComponent;

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
    DEATH
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

struct DragonAIContext
{
    float hpRate = 1.0f;
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
    std::string animationActionName;
    std::string ikActionName;
    std::string hitBoxActionName;
    std::string vfxActionName;
    float duration = 1.0f;
    float recoveryTime = 0.35f;
    float cooldownTime = 1.0f;
    bool waitForActionEnd = true;
    bool allowBuffer = true;
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
    void SetCanCancel(bool enabled_);
    void SetCanBufferAction(bool enabled_);
    void NotifyActionEnd();
    bool ConsumeBufferedAction();
    void RegisterActions();

    DRAGON_ACTION_TYPE SelectNextAction(const DragonAIContext& context_);
    void AddActionCandidates(
        const DragonAIContext& context_,
        std::vector<DragonActionCandidate>& candidates_) const;

private:
    void EnsureDependencies();
    void InitializeDefaultDefinitions();
    void UpdateContext();
    void UpdateCooldowns(float deltaTime_);
    void UpdateDecision(float deltaTime_);
    void StartAction(DRAGON_ACTION_TYPE action_);
    void EnterRecovery();
    void FinishRecovery();
    void ExecuteActionDefinition(const DragonActionDefinition& definition_);
    void ExecuteOwnerAction(const std::string& actionName_);
    void SendRuntimeToBlackboard();
    void RegisterRuntimeActions();
    DragonActionDefinition* FindDefinition(DRAGON_ACTION_TYPE action_);
    const DragonActionDefinition* FindDefinition(DRAGON_ACTION_TYPE action_) const;
    float GetCooldown(DRAGON_ACTION_TYPE action_) const;
    void SetCooldown(DRAGON_ACTION_TYPE action_, float cooldownTime_);
    bool IsActionAvailable(DRAGON_ACTION_TYPE action_) const;

    std::string playerObjectName = "Player";
    std::string actionCategory = "DragonAI";
    std::string registeredActionCategory;
    std::string blackboardCategory = "DragonAI";

    std::vector<DragonActionDefinition> actionDefinitions;
    std::vector<float> actionCooldowns;
    std::vector<std::string> registeredActionNames;

    DragonAIContext context;
    DragonActionRuntime runtime;
    DRAGON_AI_STATE aiState = DRAGON_AI_STATE::IDLE;
    DRAGON_DAMAGE_PHASE damagePhase = DRAGON_DAMAGE_PHASE::NORMAL;

    BlackboardComponent* blackboard = nullptr;
    HealthComponent* health = nullptr;
    std::weak_ptr<GameObject> playerObject;

    bool autoDecisionEnabled = true;
    float decisionInterval = 0.25f;
    float decisionTimer = 0.0f;
    float frontAngleDegrees = 45.0f;
    float sideAngleDegrees = 135.0f;
    float underBodyDistance = 3.0f;
    float biteDistance = 4.0f;
    float mediumDistance = 10.0f;
    float farDistance = 12.0f;
    float turnSpeed = 6.0f;

    DRAGON_ACTION_TYPE debugAction = DRAGON_ACTION_TYPE::BITE;
    std::mt19937 randomEngine;
};

const char* GetDragonActionName(DRAGON_ACTION_TYPE action_);
const char* GetDragonAIStateName(DRAGON_AI_STATE state_);
const char* GetDragonDamagePhaseName(DRAGON_DAMAGE_PHASE phase_);
