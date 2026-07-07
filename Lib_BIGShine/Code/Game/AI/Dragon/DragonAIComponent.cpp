#include "DragonAIComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    constexpr int DRAGON_ACTION_COUNT = static_cast<int>(DRAGON_ACTION_TYPE::DEATH) + 1;
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;

    float NormalizeAngle(float angle_)
    {
        while (angle_ > 180.0f) angle_ -= 360.0f;
        while (angle_ < -180.0f) angle_ += 360.0f;
        return angle_;
    }

    float Distance2D(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
    {
        const float x_ = a_.x - b_.x;
        const float z_ = a_.z - b_.z;
        return std::sqrt(x_ * x_ + z_ * z_);
    }

    float Distance3D(const DirectX::XMFLOAT3& a_, const DirectX::XMFLOAT3& b_)
    {
        const float x_ = a_.x - b_.x;
        const float y_ = a_.y - b_.y;
        const float z_ = a_.z - b_.z;
        return std::sqrt(x_ * x_ + y_ * y_ + z_ * z_);
    }

    int ToIndex(DRAGON_ACTION_TYPE action_)
    {
        return std::clamp(static_cast<int>(action_), 0, DRAGON_ACTION_COUNT - 1);
    }

    DRAGON_ACTION_TYPE ActionFromIndex(int index_)
    {
        return static_cast<DRAGON_ACTION_TYPE>(
            std::clamp(index_, 0, DRAGON_ACTION_COUNT - 1));
    }
}

const char* GetDragonActionName(DRAGON_ACTION_TYPE action_)
{
    switch (action_)
    {
    case DRAGON_ACTION_TYPE::IDLE: return "Idle";
    case DRAGON_ACTION_TYPE::WALK_APPROACH: return "WalkApproach";
    case DRAGON_ACTION_TYPE::RUN_CHARGE: return "RunCharge";
    case DRAGON_ACTION_TYPE::TURN_TO_PLAYER: return "TurnToPlayer";
    case DRAGON_ACTION_TYPE::BITE: return "Bite";
    case DRAGON_ACTION_TYPE::FIRE_BALL: return "FireBall";
    case DRAGON_ACTION_TYPE::SPREAD_BREATH: return "SpreadBreath";
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP: return "TurnTailSweep";
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP: return "SpinTailSweep";
    case DRAGON_ACTION_TYPE::TAKE_OFF: return "TakeOff";
    case DRAGON_ACTION_TYPE::AIR_HOLD: return "AirHold";
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL: return "AirFireBall";
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH: return "AirSpreadBreath";
    case DRAGON_ACTION_TYPE::LANDING_ATTACK: return "LandingAttack";
    case DRAGON_ACTION_TYPE::GET_HIT: return "GetHit";
    case DRAGON_ACTION_TYPE::DEATH: return "Death";
    default: return "Idle";
    }
}

const char* GetDragonAIStateName(DRAGON_AI_STATE state_)
{
    switch (state_)
    {
    case DRAGON_AI_STATE::IDLE: return "Idle";
    case DRAGON_AI_STATE::TURN: return "Turn";
    case DRAGON_AI_STATE::APPROACH: return "Approach";
    case DRAGON_AI_STATE::ACTION: return "Action";
    case DRAGON_AI_STATE::RECOVERY: return "Recovery";
    case DRAGON_AI_STATE::AIR: return "Air";
    case DRAGON_AI_STATE::DEAD: return "Dead";
    default: return "Idle";
    }
}

const char* GetDragonDamagePhaseName(DRAGON_DAMAGE_PHASE phase_)
{
    switch (phase_)
    {
    case DRAGON_DAMAGE_PHASE::NORMAL: return "Normal";
    case DRAGON_DAMAGE_PHASE::CRACKED: return "Cracked";
    case DRAGON_DAMAGE_PHASE::DARK_LEAK: return "DarkLeak";
    case DRAGON_DAMAGE_PHASE::COLLAPSE: return "Collapse";
    default: return "Normal";
    }
}

const std::string& DragonAIComponent::GetName() const
{
    static std::string name_ = "DragonAIComponent";
    return name_;
}

void DragonAIComponent::Start()
{
    randomEngine.seed(std::random_device{}());
    EnsureDependencies();
    InitializeDefaultDefinitions();
    RegisterActions();
    SendRuntimeToBlackboard();
}

void DragonAIComponent::Update(float deltaTime_)
{
    EnsureDependencies();
    UpdateContext();
    UpdateCooldowns(deltaTime_);

    if (aiState == DRAGON_AI_STATE::DEAD)
    {
        return;
    }

    if (health && health->IsDead())
    {
        StartAction(DRAGON_ACTION_TYPE::DEATH);
        aiState = DRAGON_AI_STATE::DEAD;
        return;
    }

    runtime.actionTimer += deltaTime_;

    if (aiState == DRAGON_AI_STATE::ACTION)
    {
        const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
        if (definition_ && !definition_->waitForActionEnd && runtime.actionTimer >= definition_->duration)
        {
            NotifyActionEnd();
        }
    }
    else if (aiState == DRAGON_AI_STATE::RECOVERY)
    {
        runtime.recoveryTimer -= deltaTime_;
        if (runtime.recoveryTimer <= 0.0f)
        {
            FinishRecovery();
        }
    }

    UpdateDecision(deltaTime_);
    SendRuntimeToBlackboard();
}

void DragonAIComponent::Gui()
{
    ImGui::Checkbox("Auto Decision", &autoDecisionEnabled);
    ImGui::InputText("Player Object", &playerObjectName);
    ImGui::InputText("Action Category", &actionCategory);
    ImGui::InputText("Blackboard Category", &blackboardCategory);

    ImGui::DragFloat("Decision Interval", &decisionInterval, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Front Angle", &frontAngleDegrees, 0.5f, 1.0f, 180.0f);
    ImGui::DragFloat("Side Angle", &sideAngleDegrees, 0.5f, 1.0f, 180.0f);
    ImGui::DragFloat("Under Body Distance", &underBodyDistance, 0.1f, 0.0f, 30.0f);
    ImGui::DragFloat("Bite Distance", &biteDistance, 0.1f, 0.0f, 50.0f);
    ImGui::DragFloat("Medium Distance", &mediumDistance, 0.1f, 0.0f, 80.0f);
    ImGui::DragFloat("Far Distance", &farDistance, 0.1f, 0.0f, 120.0f);
    ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);

    ImGui::SeparatorText("Runtime");
    ImGui::Text("State: %s", GetDragonAIStateName(aiState));
    ImGui::Text("Phase: %s", GetDragonDamagePhaseName(damagePhase));
    ImGui::Text("Current: %s", GetDragonActionName(runtime.currentAction));
    ImGui::Text("Buffered: %s", GetDragonActionName(runtime.bufferedAction));
    ImGui::Text("CanCancel: %s", runtime.canCancel ? "true" : "false");
    ImGui::Text("CanBuffer: %s", runtime.canBufferAction ? "true" : "false");
    ImGui::Text("ActionEnded: %s", runtime.actionEnded ? "true" : "false");
    ImGui::Text("Distance: %.2f / Angle: %.1f", context.playerDistance, context.playerAngleDegrees);

    if (ImGui::Button("CanCancel On")) SetCanCancel(true);
    ImGui::SameLine();
    if (ImGui::Button("CanCancel Off")) SetCanCancel(false);
    if (ImGui::Button("Buffer On")) SetCanBufferAction(true);
    ImGui::SameLine();
    if (ImGui::Button("Buffer Off")) SetCanBufferAction(false);
    if (ImGui::Button("Action End")) NotifyActionEnd();

    ImGui::SeparatorText("Debug Action");
    int debugActionIndex_ = static_cast<int>(debugAction);
    if (ImGui::BeginCombo("Action", GetDragonActionName(debugAction)))
    {
        for (int actionIndex_ = 0; actionIndex_ < DRAGON_ACTION_COUNT; ++actionIndex_)
        {
            const DRAGON_ACTION_TYPE action_ = ActionFromIndex(actionIndex_);
            const bool selected_ = debugAction == action_;
            if (ImGui::Selectable(GetDragonActionName(action_), selected_))
            {
                debugAction = action_;
                debugActionIndex_ = actionIndex_;
            }
        }
        ImGui::EndCombo();
    }
    debugAction = ActionFromIndex(debugActionIndex_);
    if (ImGui::Button("Request Debug Action"))
    {
        RequestAction(debugAction);
    }

    ImGui::SeparatorText("Definitions");
    for (DragonActionDefinition& definition_ : actionDefinitions)
    {
        ImGui::PushID(static_cast<int>(definition_.action));
        if (ImGui::TreeNode(GetDragonActionName(definition_.action)))
        {
            ImGui::InputText("Action Name", &definition_.actionName);
            ImGui::InputText("Sequence Action", &definition_.actionSequenceName);
            ImGui::InputText("Animation Action", &definition_.animationActionName);
            ImGui::InputText("IK Action", &definition_.ikActionName);
            ImGui::InputText("HitBox Action", &definition_.hitBoxActionName);
            ImGui::InputText("VFX Action", &definition_.vfxActionName);
            ImGui::DragFloat("Duration", &definition_.duration, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Recovery", &definition_.recoveryTime, 0.01f, 0.0f, 60.0f);
            ImGui::DragFloat("Cooldown", &definition_.cooldownTime, 0.01f, 0.0f, 120.0f);
            ImGui::Checkbox("Wait For ActionEnd", &definition_.waitForActionEnd);
            ImGui::Checkbox("Allow Buffer", &definition_.allowBuffer);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (ImGui::Button("Register Actions"))
    {
        RegisterActions();
    }
}

void DragonAIComponent::Serialize(nlohmann::json& json_) const
{
    json_["PlayerObjectName"] = playerObjectName;
    json_["ActionCategory"] = actionCategory;
    json_["BlackboardCategory"] = blackboardCategory;
    json_["AutoDecisionEnabled"] = autoDecisionEnabled;
    json_["DecisionInterval"] = decisionInterval;
    json_["FrontAngleDegrees"] = frontAngleDegrees;
    json_["SideAngleDegrees"] = sideAngleDegrees;
    json_["UnderBodyDistance"] = underBodyDistance;
    json_["BiteDistance"] = biteDistance;
    json_["MediumDistance"] = mediumDistance;
    json_["FarDistance"] = farDistance;
    json_["TurnSpeed"] = turnSpeed;

    json_["ActionDefinitions"] = nlohmann::json::array();
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        json_["ActionDefinitions"].push_back({
            { "Action", static_cast<int>(definition_.action) },
            { "ActionName", definition_.actionName },
            { "ActionSequenceName", definition_.actionSequenceName },
            { "AnimationActionName", definition_.animationActionName },
            { "IKActionName", definition_.ikActionName },
            { "HitBoxActionName", definition_.hitBoxActionName },
            { "VFXActionName", definition_.vfxActionName },
            { "Duration", definition_.duration },
            { "RecoveryTime", definition_.recoveryTime },
            { "CooldownTime", definition_.cooldownTime },
            { "WaitForActionEnd", definition_.waitForActionEnd },
            { "AllowBuffer", definition_.allowBuffer },
            });
    }
}

void DragonAIComponent::Deserialize(const nlohmann::json& json_)
{
    playerObjectName = json_.value("PlayerObjectName", std::string("Player"));
    actionCategory = json_.value("ActionCategory", std::string("DragonAI"));
    blackboardCategory = json_.value("BlackboardCategory", std::string("DragonAI"));
    autoDecisionEnabled = json_.value("AutoDecisionEnabled", true);
    decisionInterval = json_.value("DecisionInterval", 0.25f);
    frontAngleDegrees = json_.value("FrontAngleDegrees", 45.0f);
    sideAngleDegrees = json_.value("SideAngleDegrees", 135.0f);
    underBodyDistance = json_.value("UnderBodyDistance", 3.0f);
    biteDistance = json_.value("BiteDistance", 4.0f);
    mediumDistance = json_.value("MediumDistance", 10.0f);
    farDistance = json_.value("FarDistance", 12.0f);
    turnSpeed = json_.value("TurnSpeed", 6.0f);

    InitializeDefaultDefinitions();
    if (json_.contains("ActionDefinitions") && json_["ActionDefinitions"].is_array())
    {
        for (const nlohmann::json& definitionJson_ : json_["ActionDefinitions"])
        {
            const DRAGON_ACTION_TYPE action_ =
                ActionFromIndex(definitionJson_.value("Action", 0));
            DragonActionDefinition* definition_ = FindDefinition(action_);
            if (!definition_) continue;

            definition_->actionName = definitionJson_.value("ActionName", std::string(GetDragonActionName(action_)));
            definition_->actionSequenceName = definitionJson_.value("ActionSequenceName", definition_->actionSequenceName);
            definition_->animationActionName = definitionJson_.value("AnimationActionName", definition_->animationActionName);
            definition_->ikActionName = definitionJson_.value("IKActionName", definition_->ikActionName);
            definition_->hitBoxActionName = definitionJson_.value("HitBoxActionName", definition_->hitBoxActionName);
            definition_->vfxActionName = definitionJson_.value("VFXActionName", definition_->vfxActionName);
            definition_->duration = definitionJson_.value("Duration", definition_->duration);
            definition_->recoveryTime = definitionJson_.value("RecoveryTime", definition_->recoveryTime);
            definition_->cooldownTime = definitionJson_.value("CooldownTime", definition_->cooldownTime);
            definition_->waitForActionEnd = definitionJson_.value("WaitForActionEnd", definition_->waitForActionEnd);
            definition_->allowBuffer = definitionJson_.value("AllowBuffer", definition_->allowBuffer);
        }
    }

    RegisterActions();
}

bool DragonAIComponent::RequestAction(DRAGON_ACTION_TYPE action_)
{
    if (action_ == DRAGON_ACTION_TYPE::IDLE)
    {
        return false;
    }
    if (!IsActionAvailable(action_))
    {
        return false;
    }
    if (runtime.currentAction == action_)
    {
        return false;
    }

    const bool canChangeImmediately_ =
        aiState == DRAGON_AI_STATE::IDLE ||
        aiState == DRAGON_AI_STATE::APPROACH ||
        aiState == DRAGON_AI_STATE::TURN ||
        runtime.canCancel;

    if (!canChangeImmediately_)
    {
        const DragonActionDefinition* definition_ = FindDefinition(action_);
        if (runtime.canBufferAction && definition_ && definition_->allowBuffer)
        {
            runtime.bufferedAction = action_;
        }
        return false;
    }

    StartAction(action_);
    return true;
}

void DragonAIComponent::SetCanCancel(bool enabled_)
{
    runtime.canCancel = enabled_;
    if (enabled_ && ConsumeBufferedAction())
    {
        return;
    }
    SendRuntimeToBlackboard();
}

void DragonAIComponent::SetCanBufferAction(bool enabled_)
{
    runtime.canBufferAction = enabled_;
    SendRuntimeToBlackboard();
}

void DragonAIComponent::NotifyActionEnd()
{
    if (aiState != DRAGON_AI_STATE::ACTION &&
        aiState != DRAGON_AI_STATE::AIR)
    {
        return;
    }

    runtime.actionEnded = true;
    runtime.canCancel = true;

    if (ConsumeBufferedAction())
    {
        return;
    }

    EnterRecovery();
    SendRuntimeToBlackboard();
}

bool DragonAIComponent::ConsumeBufferedAction()
{
    if (runtime.bufferedAction == DRAGON_ACTION_TYPE::IDLE)
    {
        return false;
    }

    const DRAGON_ACTION_TYPE action_ = runtime.bufferedAction;
    runtime.bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    StartAction(action_);
    return true;
}

void DragonAIComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    const std::string removeCategory_ =
        registeredActionCategory.empty() ? actionCategory : registeredActionCategory;
    for (const std::string& actionName_ : registeredActionNames)
    {
        owner_->GetLocalRegistry().RemoveAction(removeCategory_, actionName_);
    }
    registeredActionNames.clear();
    registeredActionCategory = actionCategory;

    RegisterRuntimeActions();

    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        const std::string actionName_ = "DragonAI_" + definition_.actionName;
        const DRAGON_ACTION_TYPE action_ = definition_.action;
        owner_->GetLocalRegistry().RegisterAction(actionCategory, actionName_, [this, action_]()
            {
                RequestAction(action_);
            });
        registeredActionNames.push_back(actionName_);
    }
}

DRAGON_ACTION_TYPE DragonAIComponent::SelectNextAction(const DragonAIContext& context_)
{
    std::vector<DragonActionCandidate> candidates_;
    AddActionCandidates(context_, candidates_);
    if (candidates_.empty())
    {
        return DRAGON_ACTION_TYPE::IDLE;
    }

    const float totalWeight_ = std::accumulate(
        candidates_.begin(),
        candidates_.end(),
        0.0f,
        [](float total_, const DragonActionCandidate& candidate_)
        {
            return total_ + (std::max)(0.0f, candidate_.weight);
        });
    if (totalWeight_ <= 0.0001f)
    {
        return candidates_.front().action;
    }

    std::uniform_real_distribution<float> distribution_(0.0f, totalWeight_);
    float roll_ = distribution_(randomEngine);
    for (const DragonActionCandidate& candidate_ : candidates_)
    {
        roll_ -= (std::max)(0.0f, candidate_.weight);
        if (roll_ <= 0.0f)
        {
            return candidate_.action;
        }
    }

    return candidates_.back().action;
}

void DragonAIComponent::AddActionCandidates(
    const DragonAIContext& context_,
    std::vector<DragonActionCandidate>& candidates_) const
{
    auto addCandidate_ = [this, &candidates_](DRAGON_ACTION_TYPE action_, float weight_)
        {
            if (weight_ <= 0.0f || !IsActionAvailable(action_)) return;
            candidates_.push_back({ action_, weight_ });
        };

    if (context_.isPlayerUnderBody && context_.canUseTakeOff)
    {
        addCandidate_(DRAGON_ACTION_TYPE::TAKE_OFF, 5.0f);
    }
    if (context_.isPlayerBehind && context_.canUseTailSweep)
    {
        addCandidate_(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, 4.0f);
    }
    if (context_.isPlayerInSide)
    {
        addCandidate_(DRAGON_ACTION_TYPE::TURN_TO_PLAYER, 2.0f);
    }
    if (context_.isPlayerInFront && context_.playerDistance < biteDistance && context_.canUseBite)
    {
        addCandidate_(DRAGON_ACTION_TYPE::BITE, 3.0f);
    }
    if (context_.isPlayerInFront && context_.playerDistance < farDistance && context_.canUseFireBall)
    {
        addCandidate_(DRAGON_ACTION_TYPE::FIRE_BALL, 2.0f);
    }
    if (context_.playerDistance >= mediumDistance && context_.canUseSpreadBreath)
    {
        addCandidate_(DRAGON_ACTION_TYPE::SPREAD_BREATH, 2.0f);
    }
    if (context_.playerDistance >= mediumDistance && context_.canUseRunCharge)
    {
        addCandidate_(DRAGON_ACTION_TYPE::RUN_CHARGE, 1.5f);
    }
    if (static_cast<int>(damagePhase) >= static_cast<int>(DRAGON_DAMAGE_PHASE::DARK_LEAK) &&
        context_.canUseSpinSweep)
    {
        addCandidate_(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, 1.0f);
    }

    addCandidate_(DRAGON_ACTION_TYPE::WALK_APPROACH, 0.5f);
}

void DragonAIComponent::EnsureDependencies()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    auto blackboardComponent_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboardComponent_)
    {
        blackboardComponent_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboard = blackboardComponent_.get();

    health = owner_->GetComponent<HealthComponent>().get();

    if (playerObjectName.empty())
    {
        playerObject.reset();
        return;
    }
    playerObject = owner_->GetManager().Find(playerObjectName);
}

void DragonAIComponent::InitializeDefaultDefinitions()
{
    if (!actionDefinitions.empty()) return;

    auto addDefinition_ = [this](
        DRAGON_ACTION_TYPE action_,
        const std::string& sequenceActionName_,
        const std::string& ikActionName_,
        float duration_,
        float recoveryTime_,
        float cooldownTime_)
        {
            DragonActionDefinition definition_;
            definition_.action = action_;
            definition_.actionName = GetDragonActionName(action_);
            definition_.actionSequenceName = sequenceActionName_;
            definition_.ikActionName = ikActionName_;
            definition_.duration = duration_;
            definition_.recoveryTime = recoveryTime_;
            definition_.cooldownTime = cooldownTime_;
            actionDefinitions.push_back(definition_);
        };

    addDefinition_(DRAGON_ACTION_TYPE::WALK_APPROACH, "WalkApproach", "", 0.5f, 0.1f, 0.2f);
    addDefinition_(DRAGON_ACTION_TYPE::RUN_CHARGE, "RunCharge", "", 1.4f, 0.4f, 3.0f);
    addDefinition_(DRAGON_ACTION_TYPE::TURN_TO_PLAYER, "TurnToPlayer", "", 0.6f, 0.1f, 0.4f);
    addDefinition_(DRAGON_ACTION_TYPE::BITE, "Bite", "DragonNeck_LookCenter", 1.0f, 0.35f, 1.2f);
    addDefinition_(DRAGON_ACTION_TYPE::FIRE_BALL, "FireBall", "DragonNeck_LookWorldPosition", 1.4f, 0.5f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::SPREAD_BREATH, "SpreadBreath", "DragonNeck_SweepYaw", 2.0f, 0.6f, 3.0f);
    addDefinition_(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, "TurnTailSweep", "DragonTail_SweepLeft", 1.2f, 0.45f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, "SpinTailSweep", "DragonTail_SpinSweep", 2.0f, 0.65f, 5.0f);
    addDefinition_(DRAGON_ACTION_TYPE::TAKE_OFF, "TakeOff", "", 1.4f, 0.2f, 5.0f);
    addDefinition_(DRAGON_ACTION_TYPE::AIR_HOLD, "AirHold", "", 1.0f, 0.0f, 0.2f);
    addDefinition_(DRAGON_ACTION_TYPE::AIR_FIRE_BALL, "AirFireBall", "DragonNeck_LookWorldPosition", 1.3f, 0.4f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH, "AirSpreadBreath", "DragonNeck_SweepYaw", 2.0f, 0.5f, 3.0f);
    addDefinition_(DRAGON_ACTION_TYPE::LANDING_ATTACK, "LandingAttack", "", 1.5f, 0.6f, 4.0f);
    addDefinition_(DRAGON_ACTION_TYPE::GET_HIT, "GetHit", "", 0.5f, 0.2f, 0.5f);
    addDefinition_(DRAGON_ACTION_TYPE::DEATH, "Death", "", 3.0f, 0.0f, 0.0f);

    actionCooldowns.assign(DRAGON_ACTION_COUNT, 0.0f);
}

void DragonAIComponent::UpdateContext()
{
    context = {};

    if (health && health->GetMaxHealth() > 0.0001f)
    {
        context.hpRate = std::clamp(health->GetCurrentHealth() / health->GetMaxHealth(), 0.0f, 1.0f);
    }

    damagePhase = DRAGON_DAMAGE_PHASE::NORMAL;
    if (context.hpRate <= 0.15f) damagePhase = DRAGON_DAMAGE_PHASE::COLLAPSE;
    else if (context.hpRate <= 0.4f) damagePhase = DRAGON_DAMAGE_PHASE::DARK_LEAK;
    else if (context.hpRate <= 0.7f) damagePhase = DRAGON_DAMAGE_PHASE::CRACKED;

    context.canUseSpinSweep =
        static_cast<int>(damagePhase) >= static_cast<int>(DRAGON_DAMAGE_PHASE::DARK_LEAK);

    const auto owner_ = GetGameObject();
    const auto player_ = playerObject.lock();
    if (!owner_ || !owner_->transform || !player_ || !player_->transform)
    {
        return;
    }

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const DirectX::XMFLOAT3 playerPosition_ = player_->transform->GetWorldPosition();
    context.playerDistance = Distance3D(ownerPosition_, playerPosition_);
    context.playerHorizontalDistance = Distance2D(ownerPosition_, playerPosition_);

    const DirectX::XMFLOAT3 ownerRotation_ = owner_->transform->GetWorldRotation();
    const float ownerYawRadians_ = ownerRotation_.y * DEG_TO_RAD;
    const DirectX::XMFLOAT3 forward_ = {
        std::sin(ownerYawRadians_),
        0.0f,
        std::cos(ownerYawRadians_)
    };
    const DirectX::XMFLOAT3 toPlayer_ = {
        playerPosition_.x - ownerPosition_.x,
        0.0f,
        playerPosition_.z - ownerPosition_.z
    };
    const float playerYawDegrees_ = std::atan2(toPlayer_.x, toPlayer_.z) * RAD_TO_DEG;
    const float forwardYawDegrees_ = std::atan2(forward_.x, forward_.z) * RAD_TO_DEG;
    context.playerAngleDegrees = NormalizeAngle(playerYawDegrees_ - forwardYawDegrees_);

    const float absoluteAngle_ = std::abs(context.playerAngleDegrees);
    context.isPlayerInFront = absoluteAngle_ <= frontAngleDegrees;
    context.isPlayerInSide = absoluteAngle_ > frontAngleDegrees && absoluteAngle_ <= sideAngleDegrees;
    context.isPlayerBehind = absoluteAngle_ > sideAngleDegrees;
    context.isPlayerUnderBody = context.playerHorizontalDistance <= underBodyDistance;

    context.canUseBite = IsActionAvailable(DRAGON_ACTION_TYPE::BITE);
    context.canUseFireBall = IsActionAvailable(DRAGON_ACTION_TYPE::FIRE_BALL);
    context.canUseSpreadBreath = IsActionAvailable(DRAGON_ACTION_TYPE::SPREAD_BREATH);
    context.canUseTailSweep = IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP);
    context.canUseTakeOff = IsActionAvailable(DRAGON_ACTION_TYPE::TAKE_OFF);
    context.canUseRunCharge = IsActionAvailable(DRAGON_ACTION_TYPE::RUN_CHARGE);
    context.canUseSpinSweep = context.canUseSpinSweep && IsActionAvailable(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP);
}

void DragonAIComponent::UpdateCooldowns(float deltaTime_)
{
    if (actionCooldowns.size() != DRAGON_ACTION_COUNT)
    {
        actionCooldowns.assign(DRAGON_ACTION_COUNT, 0.0f);
    }

    for (float& cooldown_ : actionCooldowns)
    {
        cooldown_ = (std::max)(0.0f, cooldown_ - deltaTime_);
    }
}

void DragonAIComponent::UpdateDecision(float deltaTime_)
{
    if (!autoDecisionEnabled || aiState != DRAGON_AI_STATE::IDLE)
    {
        return;
    }

    decisionTimer -= deltaTime_;
    if (decisionTimer > 0.0f)
    {
        return;
    }

    decisionTimer = decisionInterval;
    RequestAction(SelectNextAction(context));
}

void DragonAIComponent::StartAction(DRAGON_ACTION_TYPE action_)
{
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_) return;

    runtime.currentAction = action_;
    runtime.bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    runtime.actionTimer = 0.0f;
    runtime.recoveryTimer = definition_->recoveryTime;
    runtime.actionStarted = true;
    runtime.actionEnded = false;
    runtime.canCancel = !definition_->waitForActionEnd;
    runtime.canBufferAction = false;

    aiState = DRAGON_AI_STATE::ACTION;
    if (action_ == DRAGON_ACTION_TYPE::AIR_HOLD ||
        action_ == DRAGON_ACTION_TYPE::AIR_FIRE_BALL ||
        action_ == DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH)
    {
        aiState = DRAGON_AI_STATE::AIR;
    }

    ExecuteActionDefinition(*definition_);
    SetCooldown(action_, definition_->cooldownTime);
    SendRuntimeToBlackboard();
}

void DragonAIComponent::EnterRecovery()
{
    aiState = DRAGON_AI_STATE::RECOVERY;
}

void DragonAIComponent::FinishRecovery()
{
    runtime.currentAction = DRAGON_ACTION_TYPE::IDLE;
    runtime.actionTimer = 0.0f;
    runtime.recoveryTimer = 0.0f;
    runtime.actionStarted = false;
    runtime.actionEnded = false;
    runtime.canCancel = true;
    runtime.canBufferAction = false;
    aiState = DRAGON_AI_STATE::IDLE;
}

void DragonAIComponent::ExecuteActionDefinition(const DragonActionDefinition& definition_)
{
    ExecuteOwnerAction(definition_.actionSequenceName);
    ExecuteOwnerAction(definition_.animationActionName);
    ExecuteOwnerAction(definition_.ikActionName);
    ExecuteOwnerAction(definition_.hitBoxActionName);
    ExecuteOwnerAction(definition_.vfxActionName);
}

void DragonAIComponent::ExecuteOwnerAction(const std::string& actionName_)
{
    if (actionName_.empty()) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    owner_->ExecuteAction(actionName_);
}

void DragonAIComponent::SendRuntimeToBlackboard()
{
    if (!blackboard) return;

    blackboard->SetInt(blackboardCategory, "State", static_cast<int>(aiState));
    blackboard->SetInt(blackboardCategory, "Action", static_cast<int>(runtime.currentAction));
    blackboard->SetString(blackboardCategory, "ActionName", GetDragonActionName(runtime.currentAction));
    blackboard->SetInt(blackboardCategory, "BufferedAction", static_cast<int>(runtime.bufferedAction));
    blackboard->SetFloat(blackboardCategory, "ActionTimer", runtime.actionTimer);
    blackboard->SetFloat(blackboardCategory, "RecoveryTimer", runtime.recoveryTimer);
    blackboard->SetBool(blackboardCategory, "CanCancel", runtime.canCancel);
    blackboard->SetBool(blackboardCategory, "CanBufferAction", runtime.canBufferAction);
    blackboard->SetBool(blackboardCategory, "ActionEnded", runtime.actionEnded);
    blackboard->SetFloat(blackboardCategory, "HpRate", context.hpRate);
    blackboard->SetFloat(blackboardCategory, "PlayerDistance", context.playerDistance);
    blackboard->SetFloat(blackboardCategory, "PlayerAngleDegrees", context.playerAngleDegrees);
    blackboard->SetBool(blackboardCategory, "PlayerInFront", context.isPlayerInFront);
    blackboard->SetBool(blackboardCategory, "PlayerInSide", context.isPlayerInSide);
    blackboard->SetBool(blackboardCategory, "PlayerBehind", context.isPlayerBehind);
    blackboard->SetBool(blackboardCategory, "PlayerUnderBody", context.isPlayerUnderBody);
}

void DragonAIComponent::RegisterRuntimeActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& function_)
        {
            owner_->GetLocalRegistry().RegisterAction(actionCategory, actionName_, function_);
            registeredActionNames.push_back(actionName_);
        };

    registerAction_("DragonAI_CanCancelOn", [this]() { SetCanCancel(true); });
    registerAction_("DragonAI_CanCancelOff", [this]() { SetCanCancel(false); });
    registerAction_("DragonAI_InputBufferOn", [this]() { SetCanBufferAction(true); });
    registerAction_("DragonAI_InputBufferOff", [this]() { SetCanBufferAction(false); });
    registerAction_("DragonAI_ActionEnd", [this]() { NotifyActionEnd(); });
    registerAction_("DragonAI_NotifyActionEnd", [this]() { NotifyActionEnd(); });
}

DragonActionDefinition* DragonAIComponent::FindDefinition(DRAGON_ACTION_TYPE action_)
{
    for (DragonActionDefinition& definition_ : actionDefinitions)
    {
        if (definition_.action == action_) return &definition_;
    }
    return nullptr;
}

const DragonActionDefinition* DragonAIComponent::FindDefinition(DRAGON_ACTION_TYPE action_) const
{
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        if (definition_.action == action_) return &definition_;
    }
    return nullptr;
}

float DragonAIComponent::GetCooldown(DRAGON_ACTION_TYPE action_) const
{
    if (actionCooldowns.empty()) return 0.0f;
    return actionCooldowns[ToIndex(action_)];
}

void DragonAIComponent::SetCooldown(DRAGON_ACTION_TYPE action_, float cooldownTime_)
{
    if (actionCooldowns.size() != DRAGON_ACTION_COUNT)
    {
        actionCooldowns.assign(DRAGON_ACTION_COUNT, 0.0f);
    }
    actionCooldowns[ToIndex(action_)] = (std::max)(0.0f, cooldownTime_);
}

bool DragonAIComponent::IsActionAvailable(DRAGON_ACTION_TYPE action_) const
{
    if (action_ == DRAGON_ACTION_TYPE::IDLE)
    {
        return true;
    }
    if (!FindDefinition(action_))
    {
        return false;
    }
    return GetCooldown(action_) <= 0.0f;
}

REGISTER_COMPONENT(DragonAIComponent, "DragonAIComponent")
