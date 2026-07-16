#include "PrecompiledHeader.h"

#include "DragonAIComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/DragonNeckCommandIKComponent.h"
#include "Engine/Graphics/Resource/Animation/DragonTailCommandIKComponent.h"

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
    constexpr float MIN_MOVE_DISTANCE = 0.001f;

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
    if (behaviorGraph.GetNodes().empty() && !behaviorGraphAssetPath.empty())
    {
        if (behaviorGraph.LoadFromFile(behaviorGraphAssetPath))
        {
            behaviorGraphStatus = "Loaded graph asset: " + behaviorGraphAssetPath;
        }
        else
        {
            behaviorGraphStatus = "Graph asset not found. Weighted Debug/Fallback will be used.";
        }
    }
    EnsureDefaultBehaviorGraph();
    RegisterActions();
    PlayAnimationState("PlainsDragon_IdleBreathe");
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
        if (runtime.currentAction != DRAGON_ACTION_TYPE::DEATH)
        {
            StartAction(DRAGON_ACTION_TYPE::DEATH);
        }
    }

    runtime.actionTimer += deltaTime_;

    if (aiState == DRAGON_AI_STATE::ACTION)
    {
        UpdateActionMotion(deltaTime_);
        UpdateActionTimeout();
        const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
        if (definition_ && !definition_->waitForActionEnd && runtime.actionTimer >= definition_->duration)
        {
            if (runtime.currentAction == DRAGON_ACTION_TYPE::DEATH)
            {
                aiState = DRAGON_AI_STATE::DEAD;
            }
            else
            {
                NotifyActionEnd();
            }
        }
    }
    else if (aiState == DRAGON_AI_STATE::AIR)
    {
        UpdateActionMotion(deltaTime_);
        UpdateActionTimeout();
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

    if (behaviorGraphEnabled && HasBehaviorGraph())
    {
        UpdateBehaviorGraph(deltaTime_);
    }
    else
    {
        UpdateDecision(deltaTime_);
    }
    SendRuntimeToBlackboard();
}

void DragonAIComponent::Gui()
{
    BS_PROFILE_SCOPE("DragonAIComponent::Gui");

    const ImVec4 activeColor_(0.20f, 0.85f, 0.35f, 1.0f);
    const ImVec4 inactiveColor_(0.85f, 0.25f, 0.20f, 1.0f);
    const bool isActive_ = autoDecisionEnabled && aiState != DRAGON_AI_STATE::DEAD;

    {
        BS_PROFILE_SCOPE("DragonAI GUI::Status");

        ImGui::Checkbox("Auto Decision", &autoDecisionEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Behavior Graph", &behaviorGraphEnabled);
        ImGui::SameLine();
        ImGui::TextDisabled("Weighted is Debug/Fallback");

        ImGui::SeparatorText("Status");
        ImGui::TextColored(isActive_ ? activeColor_ : inactiveColor_, "%s", isActive_ ? "AI RUNNING" : "AI STOPPED");
        ImGui::SameLine();
        ImGui::Text("Mode: %s", behaviorGraphEnabled && HasBehaviorGraph() ? "BehaviorGraph" : "Weighted Debug/Fallback");
        if (!statusText.empty())
        {
            ImGui::TextWrapped("Status: %s", statusText.c_str());
        }
        if (!behaviorGraphStatus.empty())
        {
            ImGui::TextWrapped("Graph: %s", behaviorGraphStatus.c_str());
        }
        ImGui::Text("State: %s", GetDragonAIStateName(aiState));
        ImGui::Text("Phase: %s", GetDragonDamagePhaseName(damagePhase));
        ImGui::Text("Current: %s", GetDragonActionName(runtime.currentAction));
        ImGui::Text("Last Started: %s  Count: %d", GetDragonActionName(lastStartedAction), actionStartCount);
        ImGui::Text("Last Rejected: %s", GetDragonActionName(lastRejectedAction));
        ImGui::Text("Graph: %s  Tick: %d", GetBehaviorStatusName(lastBehaviorStatus), behaviorGraphTickCount);
        ImGui::Text("Target: %s  Distance: %.2f  Angle: %.1f",
            hasPlayerTarget ? "Found" : "Missing",
            context.playerDistance,
            context.playerAngleDegrees);
        ImGui::Text("Deps Anim:%s Move:%s NeckIK:%s TailIK:%s",
            hasAnimationController ? "OK" : "--",
            hasMovementComponent ? "OK" : "--",
            hasNeckIK ? "OK" : "--",
            hasTailIK ? "OK" : "--");

        if (ImGui::Button("Test Bite")) RequestAction(DRAGON_ACTION_TYPE::BITE);
        ImGui::SameLine();
        if (ImGui::Button("Test FireBall")) RequestAction(DRAGON_ACTION_TYPE::FIRE_BALL);
        ImGui::SameLine();
        if (ImGui::Button("Action End")) NotifyActionEnd();
        if (ImGui::Button("Test TakeOff")) RequestAction(DRAGON_ACTION_TYPE::TAKE_OFF);
        ImGui::SameLine();
        if (ImGui::Button("Force Death")) StartAction(DRAGON_ACTION_TYPE::DEATH);
    }

    if (ImGui::CollapsingHeader("Tuning", ImGuiTreeNodeFlags_None))
    {
        BS_PROFILE_SCOPE("DragonAI GUI::Tuning");

        ImGui::InputText("Player Object", &playerObjectName);
        ImGui::InputText("Action Category", &actionCategory);
        ImGui::InputText("Blackboard Category", &blackboardCategory);
        ImGui::Checkbox("Use Debug HP Rate", &useDebugHpRate);
        ImGui::DragFloat("Debug HP Rate", &debugHpRate, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Weighted Debug Interval", &decisionInterval, 0.01f, 0.0f, 5.0f);
        ImGui::DragFloat("Graph Tick Interval", &behaviorGraphTickInterval, 0.01f, 0.01f, 2.0f);
        ImGui::DragFloat("Front Angle", &frontAngleDegrees, 0.5f, 1.0f, 180.0f);
        ImGui::DragFloat("Side Angle", &sideAngleDegrees, 0.5f, 1.0f, 180.0f);
        ImGui::DragFloat("Under Body Distance", &underBodyDistance, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Bite Distance", &biteDistance, 0.1f, 0.0f, 50.0f);
        ImGui::DragFloat("Medium Distance", &mediumDistance, 0.1f, 0.0f, 80.0f);
        ImGui::DragFloat("Far Distance", &farDistance, 0.1f, 0.0f, 120.0f);
        ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Walk Speed", &walkSpeed, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Run Speed", &runSpeed, 0.1f, 0.0f, 60.0f);
        ImGui::DragFloat("Animation Blend", &animationBlendDuration, 0.01f, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Debug Action", ImGuiTreeNodeFlags_None))
    {
        BS_PROFILE_SCOPE("DragonAI GUI::Debug Action");

        ImGui::Text("Buffered: %s", GetDragonActionName(runtime.bufferedAction));
        ImGui::Text("CanCancel: %s", runtime.canCancel ? "true" : "false");
        ImGui::Text("CanBuffer: %s", runtime.canBufferAction ? "true" : "false");
        ImGui::Text("ActionEnded: %s", runtime.actionEnded ? "true" : "false");
        if (ImGui::Button("CanCancel On")) SetCanCancel(true);
        ImGui::SameLine();
        if (ImGui::Button("CanCancel Off")) SetCanCancel(false);
        if (ImGui::Button("Buffer On")) SetCanBufferAction(true);
        ImGui::SameLine();
        if (ImGui::Button("Buffer Off")) SetCanBufferAction(false);

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
        if (ImGui::Button("Test SpreadBreath")) RequestAction(DRAGON_ACTION_TYPE::SPREAD_BREATH);
        ImGui::SameLine();
        if (ImGui::Button("Test TailSweep")) RequestAction(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP);
    }

    if (ImGui::CollapsingHeader("Action Toggles", ImGuiTreeNodeFlags_None))
    {
        BS_PROFILE_SCOPE("DragonAI GUI::Action Toggles");

        for (DragonActionDefinition& definition_ : actionDefinitions)
        {
            ImGui::PushID(static_cast<int>(definition_.action));
            ImGui::Checkbox("##Enabled", &definition_.enabled);
            ImGui::SameLine();
            ImGui::Text("%s  CD %.2f", GetDragonActionName(definition_.action), GetCooldown(definition_.action));
            ImGui::SameLine();
            if (ImGui::SmallButton("Run"))
            {
                RequestAction(definition_.action);
            }
            ImGui::SameLine();
            ImGui::Checkbox("Timeout", &definition_.useActionTimeout);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::DragFloat("Sec", &definition_.actionTimeout, 0.05f, 0.0f, 30.0f);
            ImGui::PopID();
        }
    }

    if (ImGui::SmallButton("Register Actions"))
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
    json_["BehaviorGraphEnabled"] = behaviorGraphEnabled;
    json_["BehaviorGraphAssetPath"] = behaviorGraphAssetPath;
    json_["UseDebugHpRate"] = useDebugHpRate;
    json_["DebugHpRate"] = debugHpRate;
    json_["DecisionInterval"] = decisionInterval;
    json_["BehaviorGraphTickInterval"] = behaviorGraphTickInterval;
    json_["FrontAngleDegrees"] = frontAngleDegrees;
    json_["SideAngleDegrees"] = sideAngleDegrees;
    json_["UnderBodyDistance"] = underBodyDistance;
    json_["BiteDistance"] = biteDistance;
    json_["MediumDistance"] = mediumDistance;
    json_["FarDistance"] = farDistance;
    json_["TurnSpeed"] = turnSpeed;
    json_["WalkSpeed"] = walkSpeed;
    json_["RunSpeed"] = runSpeed;
    json_["AnimationBlendDuration"] = animationBlendDuration;

    json_["ActionDefinitions"] = nlohmann::json::array();
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        json_["ActionDefinitions"].push_back({
            { "Action", static_cast<int>(definition_.action) },
            { "ActionName", definition_.actionName },
            { "ActionSequenceName", definition_.actionSequenceName },
            { "AnimationStateName", definition_.animationStateName },
            { "AnimationActionName", definition_.animationActionName },
            { "IKActionName", definition_.ikActionName },
            { "HitBoxActionName", definition_.hitBoxActionName },
            { "VFXActionName", definition_.vfxActionName },
            { "Duration", definition_.duration },
            { "RecoveryTime", definition_.recoveryTime },
            { "CooldownTime", definition_.cooldownTime },
            { "SelectionWeight", definition_.selectionWeight },
            { "Enabled", definition_.enabled },
            { "WaitForActionEnd", definition_.waitForActionEnd },
            { "AllowBuffer", definition_.allowBuffer },
            { "UseActionTimeout", definition_.useActionTimeout },
            { "ActionTimeout", definition_.actionTimeout },
            });
    }
    behaviorGraph.Serialize(json_["BehaviorGraph"]);
}

void DragonAIComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::Deserialize");

    playerObjectName = json_.value("PlayerObjectName", std::string("Player"));
    actionCategory = json_.value("ActionCategory", std::string("AI"));
    blackboardCategory = json_.value("BlackboardCategory", std::string("DragonAI"));
    autoDecisionEnabled = json_.value("AutoDecisionEnabled", true);
    behaviorGraphEnabled = json_.value("BehaviorGraphEnabled", true);
    behaviorGraphAssetPath = json_.value("BehaviorGraphAssetPath", std::string("Data/AI/BehaviorGraph/DragonFirstForm.json"));
    useDebugHpRate = json_.value("UseDebugHpRate", false);
    debugHpRate = json_.value("DebugHpRate", 1.0f);
    decisionInterval = json_.value("DecisionInterval", 0.25f);
    behaviorGraphTickInterval = json_.value("BehaviorGraphTickInterval", 0.2f);
    frontAngleDegrees = json_.value("FrontAngleDegrees", 45.0f);
    sideAngleDegrees = json_.value("SideAngleDegrees", 135.0f);
    underBodyDistance = json_.value("UnderBodyDistance", 3.0f);
    biteDistance = json_.value("BiteDistance", 4.0f);
    mediumDistance = json_.value("MediumDistance", 10.0f);
    farDistance = json_.value("FarDistance", 12.0f);
    turnSpeed = json_.value("TurnSpeed", 6.0f);
    walkSpeed = json_.value("WalkSpeed", 2.0f);
    runSpeed = json_.value("RunSpeed", 6.0f);
    animationBlendDuration = json_.value("AnimationBlendDuration", 0.2f);

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
            definition_->animationStateName = definitionJson_.value("AnimationStateName", definition_->animationStateName);
            definition_->animationActionName = definitionJson_.value("AnimationActionName", definition_->animationActionName);
            definition_->ikActionName = definitionJson_.value("IKActionName", definition_->ikActionName);
            definition_->hitBoxActionName = definitionJson_.value("HitBoxActionName", definition_->hitBoxActionName);
            definition_->vfxActionName = definitionJson_.value("VFXActionName", definition_->vfxActionName);
            definition_->duration = definitionJson_.value("Duration", definition_->duration);
            definition_->recoveryTime = definitionJson_.value("RecoveryTime", definition_->recoveryTime);
            definition_->cooldownTime = definitionJson_.value("CooldownTime", definition_->cooldownTime);
            definition_->selectionWeight = definitionJson_.value("SelectionWeight", definition_->selectionWeight);
            definition_->enabled = definitionJson_.value("Enabled", definition_->enabled);
            definition_->waitForActionEnd = definitionJson_.value("WaitForActionEnd", definition_->waitForActionEnd);
            definition_->allowBuffer = definitionJson_.value("AllowBuffer", definition_->allowBuffer);
            definition_->useActionTimeout = definitionJson_.value("UseActionTimeout", definition_->useActionTimeout);
            definition_->actionTimeout = definitionJson_.value("ActionTimeout", definition_->actionTimeout);
        }
    }

    if (json_.contains("BehaviorGraph"))
    {
        BS_PROFILE_SCOPE("DragonAI::DeserializeInlineBehaviorGraph");
        behaviorGraph.Deserialize(json_["BehaviorGraph"]);
    }
    else if (!behaviorGraphAssetPath.empty())
    {
        BS_PROFILE_SCOPE("DragonAI::LoadBehaviorGraphOnDeserialize");
        behaviorGraph.LoadFromFile(behaviorGraphAssetPath);
    }
    EnsureDefaultBehaviorGraph();
    RegisterActions();
}

bool DragonAIComponent::RequestAction(DRAGON_ACTION_TYPE action_)
{
    if (action_ == DRAGON_ACTION_TYPE::IDLE)
    {
        FinishRecovery();
        return true;
    }
    if (!IsActionAvailable(action_))
    {
        lastRejectedAction = action_;
        return false;
    }
    if (runtime.currentAction == action_)
    {
        lastRejectedAction = action_;
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
        lastRejectedAction = action_;
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
    runtime.canBufferAction = false;

    if (runtime.currentAction == DRAGON_ACTION_TYPE::DEATH)
    {
        aiState = DRAGON_AI_STATE::DEAD;
        StopMovement();
        SendRuntimeToBlackboard();
        return;
    }

    if (ConsumeBufferedAction())
    {
        return;
    }

    const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
    if (definition_ && definition_->recoveryTime > 0.0f)
    {
        runtime.recoveryTimer = definition_->recoveryTime;
        EnterRecovery();
    }
    else
    {
        FinishRecovery();
    }
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

void DragonAIComponent::ResetBehaviorGraphRuntime()
{
    behaviorGraph.ResetRuntime();
    behaviorGraphTickCount = 0;
    lastBehaviorStatus = AI_BEHAVIOR_STATUS::FAILURE;
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
        owner_->RegisterAction(actionCategory, actionName_, [this, action_]()
            {
                RequestAction(action_);
            });
        registeredActionNames.push_back(owner_->BuildActionName(actionName_));
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
            candidates_.push_back({ action_, weight_ * GetSelectionWeight(action_) });
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
    animationController = owner_->GetComponent<AnimationControllerComponent>();
    movement = owner_->GetComponent<CharacterMovementComponent>();
    neckIK = owner_->GetComponent<DragonNeckCommandIKComponent>();
    tailIK = owner_->GetComponent<DragonTailCommandIKComponent>();
    hasAnimationController = !animationController.expired();
    hasMovementComponent = !movement.expired();
    hasNeckIK = !neckIK.expired();
    hasTailIK = !tailIK.expired();

    if (playerObjectName.empty())
    {
        playerObject.reset();
        hasPlayerTarget = false;
        return;
    }
    playerObject = owner_->GetManager().Find(playerObjectName);
    hasPlayerTarget = !playerObject.expired();
}

void DragonAIComponent::InitializeDefaultDefinitions()
{
    if (!actionDefinitions.empty()) return;

    auto addDefinition_ = [this](
        DRAGON_ACTION_TYPE action_,
        const std::string& sequenceActionName_,
        const std::string& animationStateName_,
        const std::string& ikActionName_,
        float duration_,
        float recoveryTime_,
        float cooldownTime_,
        float actionTimeout_)
        {
            DragonActionDefinition definition_;
            definition_.action = action_;
            definition_.actionName = GetDragonActionName(action_);
            definition_.actionSequenceName = sequenceActionName_;
            definition_.animationStateName = animationStateName_;
            definition_.ikActionName = ikActionName_;
            definition_.duration = duration_;
            definition_.recoveryTime = recoveryTime_;
            definition_.cooldownTime = cooldownTime_;
            definition_.waitForActionEnd = true;
            definition_.useActionTimeout = true;
            definition_.actionTimeout = actionTimeout_;
            actionDefinitions.push_back(definition_);
        };

    addDefinition_(DRAGON_ACTION_TYPE::IDLE, "Idle", "PlainsDragon_IdleBreathe", "", 0.1f, 0.0f, 0.0f, 1.0f);
    addDefinition_(DRAGON_ACTION_TYPE::WALK_APPROACH, "WalkApproach", "PlainsDragon_Walk", "", 0.9f, 0.1f, 0.2f, 1.2f);
    addDefinition_(DRAGON_ACTION_TYPE::RUN_CHARGE, "RunCharge", "PlainsDragon_Run", "", 1.4f, 0.4f, 3.0f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::TURN_TO_PLAYER, "TurnToPlayer", "PlainsDragon_Turn90L_RM", "", 0.6f, 0.1f, 0.4f, 1.0f);
    addDefinition_(DRAGON_ACTION_TYPE::BITE, "Bite", "PlainsDragon_BiteAttack", "DragonNeck_LookCenter", 1.0f, 0.35f, 1.2f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::FIRE_BALL, "FireBall", "PlainsDragon_SpitFireBall", "DragonNeck_LookWorldPosition", 1.4f, 0.5f, 2.0f, 2.5f);
    addDefinition_(DRAGON_ACTION_TYPE::SPREAD_BREATH, "SpreadBreath", "PlainsDragon_SpreadFire", "DragonNeck_SweepYaw", 2.0f, 0.6f, 3.0f, 3.5f);
    addDefinition_(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, "TurnTailSweep", "PlainsDragon_Turn90L_RM", "DragonTail_SweepLeft", 1.2f, 0.45f, 2.0f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, "SpinTailSweep", "PlainsDragon_Turn90L_RM", "DragonTail_SpinSweep", 2.0f, 0.65f, 5.0f, 3.0f);
    addDefinition_(DRAGON_ACTION_TYPE::TAKE_OFF, "TakeOff", "PlainsDragon_TakeOff", "", 1.4f, 0.0f, 5.0f, 2.5f);
    addDefinition_(DRAGON_ACTION_TYPE::AIR_HOLD, "AirHold", "PlainsDragon_FlyStationary", "", 1.0f, 0.0f, 0.2f, 2.0f);
    addDefinition_(DRAGON_ACTION_TYPE::AIR_FIRE_BALL, "AirFireBall", "PlainsDragon_FlyStationarySpitFireBall", "DragonNeck_LookWorldPosition", 1.3f, 0.0f, 2.0f, 2.5f);
    addDefinition_(DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH, "AirSpreadBreath", "PlainsDragon_FlyStationarySpreadFire", "DragonNeck_SweepYaw", 2.0f, 0.0f, 3.0f, 3.5f);
    addDefinition_(DRAGON_ACTION_TYPE::LANDING_ATTACK, "LandingAttack", "PlainsDragon_FlyStationaryToLanding", "", 1.5f, 0.6f, 4.0f, 2.5f);
    addDefinition_(DRAGON_ACTION_TYPE::GET_HIT, "GetHit", "PlainsDragon_GetHit1", "", 0.5f, 0.2f, 0.5f, 1.0f);
    addDefinition_(DRAGON_ACTION_TYPE::DEATH, "Death", "PlainsDragon_Death", "", 3.0f, 0.0f, 0.0f, 5.0f);

    actionCooldowns.assign(DRAGON_ACTION_COUNT, 0.0f);
}

void DragonAIComponent::UpdateContext()
{
    context = {};

    if (useDebugHpRate)
    {
        context.hpRate = std::clamp(debugHpRate, 0.0f, 1.0f);
    }
    else if (health && health->GetMaxHealth() > 0.0001f)
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
    context.ownerPosition = ownerPosition_;
    context.playerPosition = playerPosition_;
    context.toPlayer =
    {
        playerPosition_.x - ownerPosition_.x,
        playerPosition_.y - ownerPosition_.y,
        playerPosition_.z - ownerPosition_.z
    };
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
        context.toPlayer.x,
        0.0f,
        context.toPlayer.z
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

void DragonAIComponent::UpdateBehaviorGraph(float deltaTime_)
{
    if (!autoDecisionEnabled || aiState != DRAGON_AI_STATE::IDLE)
    {
        return;
    }

    behaviorGraphTickTimer -= deltaTime_;
    if (behaviorGraphTickTimer > 0.0f)
    {
        return;
    }

    behaviorGraphTickTimer = (std::max)(0.01f, behaviorGraphTickInterval);

    const auto owner_ = GetGameObject();
    if (!owner_ || !blackboard)
    {
        return;
    }

    behaviorContext.owner = owner_.get();
    behaviorContext.target = playerObject.lock().get();
    behaviorContext.blackboard = blackboard;
    behaviorContext.globalBlackboard = nullptr;
    behaviorContext.ownerPosition = context.ownerPosition;
    behaviorContext.targetPosition = context.playerPosition;
    behaviorContext.toTargetDirection = context.toPlayer;
    behaviorContext.targetDistance = context.playerDistance;
    behaviorContext.deltaTime = deltaTime_;
    behaviorContext.hasTarget = behaviorContext.target != nullptr;

    lastBehaviorStatus = behaviorGraph.Tick(behaviorContext);
    ++behaviorGraphTickCount;
}

void DragonAIComponent::UpdateActionMotion(float deltaTime_)
{
    switch (runtime.currentAction)
    {
    case DRAGON_ACTION_TYPE::WALK_APPROACH:
        TurnTowardPlayer(deltaTime_);
        MoveForward(walkSpeed, deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::RUN_CHARGE:
        TurnTowardPlayer(deltaTime_);
        MoveForward(runSpeed, deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::TURN_TO_PLAYER:
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        TurnTowardPlayer(deltaTime_);
        StopMovement();
        break;
    case DRAGON_ACTION_TYPE::BITE:
    case DRAGON_ACTION_TYPE::FIRE_BALL:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
        TurnTowardPlayer(deltaTime_);
        StopMovement();
        break;
    default:
        StopMovement();
        break;
    }
}

void DragonAIComponent::TurnTowardPlayer(float deltaTime_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const DirectX::XMFLOAT3 direction_
    {
        context.toPlayer.x,
        0.0f,
        context.toPlayer.z
    };
    const float lengthSquared_ = direction_.x * direction_.x + direction_.z * direction_.z;
    if (lengthSquared_ <= MIN_MOVE_DISTANCE) return;

    if (const std::shared_ptr<CharacterMovementComponent> movement_ = movement.lock())
    {
        movement_->SetFacingDirection(direction_, false);
        return;
    }

    owner_->transform->SetForwardYawOnly(
        { direction_.x, 0.0f, direction_.z },
        deltaTime_,
        turnSpeed);
}

void DragonAIComponent::MoveForward(float speed_, float deltaTime_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const DirectX::XMFLOAT3 rotation_ = owner_->transform->GetWorldRotation();
    const float yawRadians_ = rotation_.y * DEG_TO_RAD;
    const DirectX::XMFLOAT3 velocity_
    {
        std::sin(yawRadians_) * speed_,
        0.0f,
        std::cos(yawRadians_) * speed_
    };

    if (const std::shared_ptr<CharacterMovementComponent> movement_ = movement.lock())
    {
        movement_->SetHorizontalVelocity(velocity_);
        return;
    }

    owner_->transform->AddMoveOffset(
        {
            velocity_.x * deltaTime_,
            0.0f,
            velocity_.z * deltaTime_
        });
}

void DragonAIComponent::StopMovement()
{
    if (const std::shared_ptr<CharacterMovementComponent> movement_ = movement.lock())
    {
        movement_->StopHorizontalMovement();
    }
}

void DragonAIComponent::StartAction(DRAGON_ACTION_TYPE action_)
{
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_) return;

    lastStartedAction = action_;
    ++actionStartCount;
    runtime.currentAction = action_;
    runtime.bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    runtime.actionTimer = 0.0f;
    runtime.recoveryTimer = 0.0f;
    runtime.actionStarted = true;
    runtime.actionEnded = false;
    runtime.canCancel = false;
    runtime.canBufferAction = definition_->allowBuffer;
    statusText.clear();

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
    runtime.actionEnded = true;
    runtime.canCancel = true;
    runtime.canBufferAction = false;
    aiState = DRAGON_AI_STATE::IDLE;
    StopMovement();
    PlayAnimationState("PlainsDragon_IdleBreathe");
}

void DragonAIComponent::UpdateActionTimeout()
{
    if (runtime.actionEnded)
    {
        return;
    }

    const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
    if (!definition_ ||
        !definition_->waitForActionEnd ||
        !definition_->useActionTimeout ||
        definition_->actionTimeout <= 0.0f ||
        runtime.actionTimer < definition_->actionTimeout)
    {
        return;
    }

    statusText = "Action timeout: DragonAI_" + definition_->actionName;
    NotifyActionEnd();
}

void DragonAIComponent::ExecuteActionDefinition(const DragonActionDefinition& definition_)
{
    PlayAnimationState(definition_.animationStateName);
    ExecuteDirectIK(definition_.action, definition_.duration);
    ExecuteOwnerAction(definition_.actionSequenceName);
    ExecuteOwnerAction(definition_.animationActionName);
    ExecuteOwnerAction(definition_.ikActionName);
    ExecuteOwnerAction(definition_.hitBoxActionName);
    ExecuteOwnerAction(definition_.vfxActionName);
}

void DragonAIComponent::PlayAnimationState(const std::string& stateName_)
{
    if (stateName_.empty()) return;

    if (const std::shared_ptr<AnimationControllerComponent> animationController_ =
        animationController.lock())
    {
        animationController_->ChangeState(stateName_, animationBlendDuration);
    }
}

void DragonAIComponent::ExecuteDirectIK(DRAGON_ACTION_TYPE action_, float duration_)
{
    const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock();
    const std::shared_ptr<DragonTailCommandIKComponent> tailIK_ = tailIK.lock();

    switch (action_)
    {
    case DRAGON_ACTION_TYPE::BITE:
        if (neckIK_) neckIK_->LookAtTarget(playerObjectName, duration_);
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
        if (neckIK_) neckIK_->LookAtTarget(playerObjectName, duration_);
        break;
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
        if (neckIK_) neckIK_->SweepYaw(-30.0f, 30.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
        if (neckIK_) neckIK_->SweepYaw(-35.0f, 35.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
        if (tailIK_)
        {
            if (context.playerAngleDegrees < 0.0f)
            {
                tailIK_->StartSweepRight(duration_);
            }
            else
            {
                tailIK_->StartSweepLeft(duration_);
            }
        }
        break;
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        if (tailIK_) tailIK_->StartSpinSweep(duration_);
        break;
    default:
        break;
    }
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

    const bool isActing_ =
        (aiState == DRAGON_AI_STATE::ACTION ||
            aiState == DRAGON_AI_STATE::AIR) &&
        !runtime.actionEnded;

    blackboard->SetBool(blackboardCategory, "HasTarget", hasPlayerTarget);
    blackboard->SetFloat(blackboardCategory, "TargetDistance", context.playerDistance);
    blackboard->SetFloat(blackboardCategory, "TargetHorizontalDistance", context.playerHorizontalDistance);
    blackboard->SetFloat(blackboardCategory, "TargetAngle", context.playerAngleDegrees);
    blackboard->SetBool(blackboardCategory, "IsTargetInFront", context.isPlayerInFront);
    blackboard->SetBool(blackboardCategory, "IsTargetInSide", context.isPlayerInSide);
    blackboard->SetBool(blackboardCategory, "IsTargetBehind", context.isPlayerBehind);
    blackboard->SetBool(blackboardCategory, "IsTargetUnderBody", context.isPlayerUnderBody);
    blackboard->SetBool(blackboardCategory, "CanUseBite", context.canUseBite);
    blackboard->SetBool(blackboardCategory, "CanUseFireBall", context.canUseFireBall);
    blackboard->SetBool(blackboardCategory, "CanUseSpreadBreath", context.canUseSpreadBreath);
    blackboard->SetBool(blackboardCategory, "CanUseTailSweep", context.canUseTailSweep);
    blackboard->SetBool(blackboardCategory, "CanUseTakeOff", context.canUseTakeOff);
    blackboard->SetBool(blackboardCategory, "CanUseRunCharge", context.canUseRunCharge);
    blackboard->SetBool(blackboardCategory, "CanUseSpinSweep", context.canUseSpinSweep);
    blackboard->SetInt(blackboardCategory, "CurrentAction", static_cast<int>(runtime.currentAction));
    blackboard->SetBool(blackboardCategory, "ActionEnded", runtime.actionEnded);
    blackboard->SetBool(blackboardCategory, "IsActing", isActing_);
    blackboard->SetFloat(blackboardCategory, "HpRate", context.hpRate);
    blackboard->SetInt(blackboardCategory, "DamagePhase", static_cast<int>(damagePhase));

    blackboard->SetInt(blackboardCategory, "State", static_cast<int>(aiState));
    blackboard->SetInt(blackboardCategory, "Action", static_cast<int>(runtime.currentAction));
    blackboard->SetString(blackboardCategory, "ActionName", GetDragonActionName(runtime.currentAction));
    blackboard->SetInt(blackboardCategory, "BufferedAction", static_cast<int>(runtime.bufferedAction));
    blackboard->SetFloat(blackboardCategory, "ActionTimer", runtime.actionTimer);
    blackboard->SetFloat(blackboardCategory, "RecoveryTimer", runtime.recoveryTimer);
    blackboard->SetBool(blackboardCategory, "CanCancel", runtime.canCancel);
    blackboard->SetBool(blackboardCategory, "CanBufferAction", runtime.canBufferAction);
    blackboard->SetFloat(blackboardCategory, "PlayerDistance", context.playerDistance);
    blackboard->SetFloat(blackboardCategory, "PlayerHorizontalDistance", context.playerHorizontalDistance);
    blackboard->SetFloat(blackboardCategory, "PlayerAngleDegrees", context.playerAngleDegrees);
    blackboard->SetBool(blackboardCategory, "PlayerInFront", context.isPlayerInFront);
    blackboard->SetBool(blackboardCategory, "PlayerInSide", context.isPlayerInSide);
    blackboard->SetBool(blackboardCategory, "PlayerBehind", context.isPlayerBehind);
    blackboard->SetBool(blackboardCategory, "PlayerUnderBody", context.isPlayerUnderBody);
    blackboard->SetBool(blackboardCategory, "CanUseBite", context.canUseBite);
    blackboard->SetBool(blackboardCategory, "CanUseFireBall", context.canUseFireBall);
    blackboard->SetBool(blackboardCategory, "CanUseSpreadBreath", context.canUseSpreadBreath);
    blackboard->SetBool(blackboardCategory, "CanUseTailSweep", context.canUseTailSweep);
    blackboard->SetBool(blackboardCategory, "CanUseTakeOff", context.canUseTakeOff);
    blackboard->SetBool(blackboardCategory, "CanUseRunCharge", context.canUseRunCharge);
    blackboard->SetBool(blackboardCategory, "CanUseSpinSweep", context.canUseSpinSweep);
}

void DragonAIComponent::EnsureDefaultBehaviorGraph()
{
    if (HasBehaviorGraph())
    {
        behaviorGraphStatus = "BehaviorGraph asset is active.";
        return;
    }

    behaviorGraphStatus = "No BehaviorGraph asset. Weighted Debug/Fallback will be used.";
}

AIBehaviorNode* DragonAIComponent::AddDragonGraphCondition(
    AIBehaviorNode* parentNode_,
    const std::string& name_,
    const std::string& key_,
    AI_BLACKBOARD_COMPARE_TYPE compareType_,
    AI_BLACKBOARD_VALUE_TYPE valueType_,
    float floatValue_,
    bool boolValue_,
    int priority_,
    const DirectX::XMFLOAT2& position_)
{
    if (!parentNode_) return nullptr;
    const uintptr_t parentNodeId_ = parentNode_->id;

    AIBehaviorNode* conditionNode_ = behaviorGraph.AddNode(
        AI_BEHAVIOR_NODE_TYPE::SEQUENCE,
        name_ + " Sequence",
        position_);
    conditionNode_->priority = priority_;
    const uintptr_t conditionNodeId_ = conditionNode_->id;
    behaviorGraph.AddLink(parentNodeId_, conditionNodeId_);

    AIBehaviorNode* testNode_ = behaviorGraph.AddNode(
        AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION,
        name_,
        { position_.x + 260.0f, position_.y });
    testNode_->blackboardCategory = blackboardCategory;
    testNode_->blackboardKey = key_;
    testNode_->compareType = compareType_;
    testNode_->valueType = valueType_;
    testNode_->floatValue = floatValue_;
    testNode_->boolValue = boolValue_;
    behaviorGraph.AddLink(conditionNodeId_, testNode_->id);

    return behaviorGraph.FindNode(conditionNodeId_);
}

AIBehaviorNode* DragonAIComponent::AddDragonGraphAction(
    AIBehaviorNode* parentNode_,
    const std::string& name_,
    const std::string& actionName_,
    int priority_,
    const DirectX::XMFLOAT2& position_)
{
    if (!parentNode_) return nullptr;
    const uintptr_t parentNodeId_ = parentNode_->id;

    std::string availabilityKey_;
    if (actionName_ == "DragonAI_Bite") availabilityKey_ = "CanUseBite";
    else if (actionName_ == "DragonAI_FireBall") availabilityKey_ = "CanUseFireBall";
    else if (actionName_ == "DragonAI_SpreadBreath") availabilityKey_ = "CanUseSpreadBreath";
    else if (actionName_ == "DragonAI_TurnTailSweep") availabilityKey_ = "CanUseTailSweep";
    else if (actionName_ == "DragonAI_TakeOff") availabilityKey_ = "CanUseTakeOff";
    else if (actionName_ == "DragonAI_SpinTailSweep") availabilityKey_ = "CanUseSpinSweep";
    else if (actionName_ == "DragonAI_RunCharge") availabilityKey_ = "CanUseRunCharge";

    if (parentNode_->type == AI_BEHAVIOR_NODE_TYPE::SEQUENCE && !availabilityKey_.empty())
    {
        AIBehaviorNode* availabilityNode_ = behaviorGraph.AddNode(
            AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION,
            "Can " + name_,
            { position_.x - 180.0f, position_.y + 56.0f });
        availabilityNode_->blackboardCategory = blackboardCategory;
        availabilityNode_->blackboardKey = availabilityKey_;
        availabilityNode_->compareType = AI_BLACKBOARD_COMPARE_TYPE::EQUAL;
        availabilityNode_->valueType = AI_BLACKBOARD_VALUE_TYPE::BOOL;
        availabilityNode_->boolValue = true;
        behaviorGraph.AddLink(parentNodeId_, availabilityNode_->id);
    }

    AIBehaviorNode* actionNode_ = behaviorGraph.AddNode(
        AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION,
        name_,
        position_);
    actionNode_->actionName = actionName_;
    actionNode_->waitForActionEnd = true;
    actionNode_->priority = priority_;
    actionNode_->actionEndCategory = blackboardCategory;
    actionNode_->actionEndKey = "ActionEnded";
    behaviorGraph.AddLink(parentNodeId_, actionNode_->id);
    return actionNode_;
}

bool DragonAIComponent::SaveBehaviorGraphAsset(const std::string& path_) const
{
    return behaviorGraph.SaveToFile(path_);
}

bool DragonAIComponent::LoadBehaviorGraphAsset(const std::string& path_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::LoadBehaviorGraphAsset");

    const bool result_ = behaviorGraph.LoadFromFile(path_);
    if (result_)
    {
        behaviorGraphAssetPath = path_;
        behaviorGraph.ResetRuntime();
        behaviorGraphStatus = "Loaded graph asset: " + behaviorGraphAssetPath;
    }
    else
    {
        behaviorGraphStatus = "Failed to load graph asset: " + path_;
    }
    return result_;
}

const char* DragonAIComponent::GetBehaviorStatusName(AI_BEHAVIOR_STATUS status_) const
{
    switch (status_)
    {
    case AI_BEHAVIOR_STATUS::SUCCESS: return "Success";
    case AI_BEHAVIOR_STATUS::FAILURE: return "Failure";
    case AI_BEHAVIOR_STATUS::RUNNING: return "Running";
    default: return "Unknown";
    }
}

void DragonAIComponent::RegisterRuntimeActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& function_)
        {
            owner_->RegisterAction(actionCategory, actionName_, function_);
            registeredActionNames.push_back(owner_->BuildActionName(actionName_));
        };

    registerAction_("DragonAI_CanCancelOn", [this]() { SetCanCancel(true); });
    registerAction_("DragonAI_CanCancelOff", [this]() { SetCanCancel(false); });
    registerAction_("DragonAI_InputBufferOn", [this]() { SetCanBufferAction(true); });
    registerAction_("DragonAI_InputBufferOff", [this]() { SetCanBufferAction(false); });
    registerAction_("DragonAI_ActionEnd", [this]() { NotifyActionEnd(); });
    registerAction_("DragonAI_NotifyActionEnd", [this]() { NotifyActionEnd(); });
    registerAction_("DragonAI_Enable", [this]() { autoDecisionEnabled = true; });
    registerAction_("DragonAI_Disable", [this]()
        {
            autoDecisionEnabled = false;
            StopMovement();
        });
    registerAction_("DragonAI_TestBite", [this]() { RequestAction(DRAGON_ACTION_TYPE::BITE); });
    registerAction_("DragonAI_TestFireBall", [this]() { RequestAction(DRAGON_ACTION_TYPE::FIRE_BALL); });
    registerAction_("DragonAI_TestSpreadBreath", [this]() { RequestAction(DRAGON_ACTION_TYPE::SPREAD_BREATH); });
    registerAction_("DragonAI_TestTailSweep", [this]() { RequestAction(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP); });
    registerAction_("DragonAI_TestTakeOff", [this]() { RequestAction(DRAGON_ACTION_TYPE::TAKE_OFF); });
    registerAction_("DragonAI_ForceDeath", [this]() { StartAction(DRAGON_ACTION_TYPE::DEATH); });
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
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_ || !definition_->enabled)
    {
        return false;
    }
    return GetCooldown(action_) <= 0.0f;
}

float DragonAIComponent::GetSelectionWeight(DRAGON_ACTION_TYPE action_) const
{
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_ || !definition_->enabled)
    {
        return 0.0f;
    }
    return (std::max)(0.0f, definition_->selectionWeight);
}

bool DragonAIComponent::HasBehaviorGraph() const
{
    return !behaviorGraph.GetNodes().empty();
}

REGISTER_COMPONENT(DragonAIComponent, "DragonAIComponent")
