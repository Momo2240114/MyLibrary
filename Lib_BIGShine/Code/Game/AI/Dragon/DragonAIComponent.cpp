#include "PrecompiledHeader.h"
#include "Engine/GamePlay/System/CombatObservationSystem.h"

#include "DragonAIComponent.h"
#include "DragonAttackControllerComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBridgeComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationGraphBlackboardKeys.h"
#include "Engine/Graphics/Resource/Animation/DragonNeckCommandIKComponent.h"
#include "Engine/Graphics/Resource/Animation/DragonTailCommandIKComponent.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
#include "Game/GameMode/GameModeManager.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
    // アクション列挙を配列インデックスとして扱うため、番兵から総数を決める。
    constexpr int DRAGON_ACTION_COUNT =
        static_cast<int>(DRAGON_ACTION_TYPE::ACTION_COUNT);
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr float DEG_TO_RAD = 3.1415926535f / 180.0f;
    constexpr float MIN_MOVE_DISTANCE = 0.001f;

    float NormalizeAngle(float angle_)
    {
        // プレイヤーとの向き差は-180から180度に正規化して、左右判定や旋回量へ使いやすくする。
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
        // 不正な列挙値が来てもクールダウン配列の範囲外へ出ないように丸める。
        return std::clamp(static_cast<int>(action_), 0, DRAGON_ACTION_COUNT - 1);
    }

    DRAGON_ACTION_TYPE ActionFromIndex(int index_)
    {
        return static_cast<DRAGON_ACTION_TYPE>(
            std::clamp(index_, 0, DRAGON_ACTION_COUNT - 1));
    }

    bool UsesTurnRootMotionLimit(DRAGON_ACTION_TYPE action_)
    {
        return action_ == DRAGON_ACTION_TYPE::TURN_TO_PLAYER;
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
    case DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP: return "FireBallSweep";
    case DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD: return "SpreadBreathForward";
    case DRAGON_ACTION_TYPE::AIR_GET_HIT: return "AirGetHit";
    case DRAGON_ACTION_TYPE::AIR_DEATH_FALL_START: return "AirDeathFallStart";
    case DRAGON_ACTION_TYPE::FALLING: return "Falling";
    case DRAGON_ACTION_TYPE::DEATH_HIT_GROUND: return "DeathHitGround";
    case DRAGON_ACTION_TYPE::ULTIMATE_ATTACK: return "UltimateAttack";
    case DRAGON_ACTION_TYPE::FIRE_BALL_FRONT: return "FireBallFront";
    case DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE: return "FireBallTriple";
    case DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP: return "FireBeamSweep";
    case DRAGON_ACTION_TYPE::GROUND_STAMP: return "GroundStamp";
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

const char* GetDragonActionFinishModeName(DRAGON_ACTION_FINISH_MODE mode_)
{
    switch (mode_)
    {
    case DRAGON_ACTION_FINISH_MODE::SEQUENCER: return "Sequencer";
    case DRAGON_ACTION_FINISH_MODE::AI_CONDITION: return "AI Condition";
    case DRAGON_ACTION_FINISH_MODE::AI_DURATION: return "AI Duration";
    case DRAGON_ACTION_FINISH_MODE::MANUAL: return "Manual";
    default: return "Sequencer";
    }
}

const std::string& DragonAIComponent::GetName() const
{
    static std::string name_ = "DragonAIComponent";
    return name_;
}

void DragonAIComponent::Start()
{
    ReloadSkillDefinitions(skillDefinitionsPath);
    // 依存コンポーネント、アクション定義、BehaviorGraph、ActionRegistryを初期化してAIを起動可能にする。
    randomEngine.seed(std::random_device{}());
    EnsureDependencies();
    InitializeDefaultDefinitions();
    actionLastUsedOrders.assign(DRAGON_ACTION_COUNT, -1);
    historySelectionAvailable.assign(DRAGON_ACTION_COUNT, false);
    attackExecutionOrder = 0;
    lastExecutedAttack = DRAGON_ACTION_TYPE::IDLE;
    excludePreviousAttack = false;
    if (behaviorGraphEnabled &&
        behaviorGraph.GetNodes().empty() &&
        !behaviorGraphAssetPath.empty())
    {
        // 指定アセットがある場合は外部JSONを優先し、失敗時は従来の重み付きデバッグAIへ落とす。
        if (behaviorGraph.LoadFromFile(behaviorGraphAssetPath))
        {
            behaviorGraphStatus = "Loaded graph asset: " + behaviorGraphAssetPath;
        }
        else
        {
            behaviorGraphStatus = "Graph asset not found. Weighted Debug/Fallback will be used.";
        }
    }
    if (behaviorGraphEnabled)
    {
        EnsureDefaultBehaviorGraph();
    }
    RegisterActions();
    if (const DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::IDLE))
    {
        ApplyAnimationBlackboard("Idle", definition_->animationStateName, false);
    }
    PlayAnimationState("PlainsDragon_IdleBreathe");
    SendRuntimeToBlackboard();
}

void DragonAIComponent::Update(float deltaTime_)
{
    // 毎フレーム依存と状況を更新し、死亡・アクション中・空中・復帰・意思決定の順で処理を分岐する。
    EnsureDependencies();
    UpdateContext();
    if (IsEnemyAILockedByGameMode())
    {
        // GameMode演出中はAI判断と攻撃遷移を進めず、演出Animationを上書きしない。
        StopMovement();
        SendRuntimeToBlackboard();
        return;
    }
    UpdateUltimateTrigger();
    UpdateCooldowns(deltaTime_);
    if (skillActionRunning)
    {
        skillExecution.Update(deltaTime_);
        if (skillActionRunning && !skillExecution.IsActive())
        {
            skillActionRunning = false;
            pendingSequencerActionEnd = false;
            CompleteActionEnd();
            SendRuntimeToBlackboard();
            return;
        }
    }

    if (pendingSequencerActionEnd)
    {
        // Sequencer通知は次Updateで処理し、アニメーションイベント由来の完了とAI更新タイミングをそろえる。
        pendingSequencerActionEnd = false;
        UpdateContext();
        CompleteActionEnd();
        SendRuntimeToBlackboard();
        return;
    }

    if (aiState == DRAGON_AI_STATE::DEAD)
    {
        return;
    }

    if (health && health->IsDead())
    {
        // HP死亡時は地上/空中に応じて適切な死亡アクションへ強制遷移する。
        if (!IsDeathAction(runtime.currentAction))
        {
            StartAction(IsGrounded()
                ? DRAGON_ACTION_TYPE::DEATH
                : DRAGON_ACTION_TYPE::AIR_DEATH_FALL_START);
        }
    }

    runtime.actionTimer += deltaTime_;

    if (aiState == DRAGON_AI_STATE::ACTION)
    {
        // 地上アクション中は移動/旋回を更新し、AI条件・タイムアウト・時間制御の順に終了判定する。
        UpdateActionMotion(deltaTime_);
        if (UpdateAIControlledActionEnd())
        {
            SendRuntimeToBlackboard();
            return;
        }
        UpdateActionTimeout();
        if (UpdateDurationControlledActionEnd())
        {
            SendRuntimeToBlackboard();
            return;
        }
    }
    else if (aiState == DRAGON_AI_STATE::AIR)
    {
        // 空中状態はAIR_HOLDを待機基準にし、着地可能タイミングや空中攻撃選択へつなげる。
        UpdateActionMotion(deltaTime_);
        if (runtime.currentAction == DRAGON_ACTION_TYPE::AIR_HOLD &&
            runtime.actionEnded)
        {
            if (pendingAirLandingDecision &&
                !IsReadyToPlayLandingAnimation())
            {
                SendRuntimeToBlackboard();
                return;
            }
            UpdateDecision(deltaTime_);
            SendRuntimeToBlackboard();
            return;
        }
        UpdateActionTimeout();
        if (UpdateDurationControlledActionEnd())
        {
            SendRuntimeToBlackboard();
            return;
        }
    }
    else if (aiState == DRAGON_AI_STATE::RECOVERY)
    {
        // 復帰中は着地待ちや硬直時間を処理し、終了後に次の判断へ戻す。
        if (waitingForLandingGround)
        {
            StopMovement();
            if (!IsGrounded())
            {
                SendRuntimeToBlackboard();
                return;
            }
            waitingForLandingGround = false;
        }

        runtime.recoveryTimer -= deltaTime_;
        if (runtime.recoveryTimer <= 0.0f)
        {
            FinishRecovery();
        }
    }

    if (behaviorGraphEnabled && HasBehaviorGraph())
    {
        // BehaviorGraphが有効な時はBlackboardへ最新状況を渡し、グラフ側に行動選択を任せる。
        if (UpdateIdleAnimationGraphReturn())
        {
            SendRuntimeToBlackboard();
            return;
        }
        UpdateBehaviorGraph(deltaTime_);
    }
    else
    {
        // グラフが無い場合は従来の重み付き選択をデバッグ/フォールバックとして使う。
        if (UpdateIdleAnimationGraphReturn())
        {
            SendRuntimeToBlackboard();
            return;
        }
        UpdateDecision(deltaTime_);
    }
    SendRuntimeToBlackboard();
}

void DragonAIComponent::Gui()
{
    BS_PROFILE_SCOPE("DragonAIComponent::Gui");

    // GUIはランタイム状態、調整値、Action定義、BehaviorGraphの診断を1コンポーネント内で確認するためのもの。
    const ImVec4 activeColor_(0.20f, 0.85f, 0.35f, 1.0f);
    const ImVec4 inactiveColor_(0.85f, 0.25f, 0.20f, 1.0f);
    const bool controllerDriven_ = !autoDecisionEnabled && !behaviorGraphEnabled;
    const bool isActive_ =
        (autoDecisionEnabled || controllerDriven_) &&
        aiState != DRAGON_AI_STATE::DEAD;

    {
        BS_PROFILE_SCOPE("DragonAI GUI::Status");

        ImGui::Checkbox("Auto Decision", &autoDecisionEnabled);
        ImGui::SameLine();
        ImGui::Checkbox("Behavior Graph", &behaviorGraphEnabled);
        ImGui::SameLine();
        ImGui::TextDisabled("Weighted is Debug/Fallback");

        ImGui::SeparatorText("Status");
        ImGui::TextColored(
            isActive_ ? activeColor_ : inactiveColor_,
            "%s",
            controllerDriven_ ? "CONTROLLER DRIVEN" : isActive_ ? "AI RUNNING" : "AI STOPPED");
        ImGui::SameLine();
        ImGui::Text(
            "Mode: %s",
            controllerDriven_
                ? "AIController"
                : behaviorGraphEnabled && HasBehaviorGraph() ? "BehaviorGraph" : "Weighted Debug/Fallback");
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
        ImGui::Text("Aggression: %.2f", CalculateAggressionMultiplier(context));
        ImGui::Text("Current: %s", GetDragonActionName(runtime.currentAction));
        ImGui::Text("Wait Idle Animation: %s", ShouldWaitForIdleAnimation() ? "true" : "false");
        ImGui::Text("Last Started: %s  Count: %d", GetDragonActionName(lastStartedAction), actionStartCount);
        ImGui::Text("Last Rejected: %s", GetDragonActionName(lastRejectedAction));
        const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ = animationGraph.lock();
        const AnimState* currentAnimationState_ = animationGraph_ ? animationGraph_->GetActiveState() : nullptr;
        const char* currentAnimationStateName_ = currentAnimationState_ ? currentAnimationState_->name.c_str() : "None";
        std::string animCurrentAction_ = GetDragonActionName(runtime.currentAction);
        std::string animActionTrigger_;
        std::string animBlackboardStateName_;
        bool animActionEnded_ = runtime.actionEnded;
        bool animIsActing_ = runtime.actionStarted && !runtime.actionEnded;
        if (blackboard)
        {
            blackboard->GetString("Anim", "CurrentAction", animCurrentAction_);
            blackboard->GetString("Anim", "ActionTrigger", animActionTrigger_);
            blackboard->GetBool("Anim", "ActionEnded", animActionEnded_);
            blackboard->GetBool("Anim", "IsActing", animIsActing_);
            blackboard->GetString("Anim", "CurrentAnimationState", animBlackboardStateName_);
        }
        ImGui::Text("Anim.CurrentAction: %s", animCurrentAction_.c_str());
        ImGui::Text("Anim.ActionTrigger: %s", animActionTrigger_.c_str());
        ImGui::Text("Anim.ActionEnded: %s", animActionEnded_ ? "true" : "false");
        ImGui::Text("Anim.IsActing: %s", animIsActing_ ? "true" : "false");
        if (!animBlackboardStateName_.empty())
        {
            ImGui::Text("Anim.CurrentAnimationState: %s", animBlackboardStateName_.c_str());
        }
        ImGui::Text("Animation State: %s", currentAnimationStateName_);
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

        ImGui::SeparatorText("Action Test");
        int debugActionIndex_ = static_cast<int>(debugAction);
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::BeginCombo("Selected Action", GetDragonActionName(debugAction)))
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
                if (selected_) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        debugAction = ActionFromIndex(debugActionIndex_);
        if (ImGui::Button("Request Selected Action"))
        {
            RequestAction(debugAction);
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Selected Action"))
        {
            ForceAction(debugAction);
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Idle"))
        {
            ForceAction(DRAGON_ACTION_TYPE::IDLE);
        }

        if (ImGui::Button("Test Bite")) RequestAction(DRAGON_ACTION_TYPE::BITE);
        ImGui::SameLine();
        if (ImGui::Button("Test FireBall")) RequestAction(DRAGON_ACTION_TYPE::FIRE_BALL);
        ImGui::SameLine();
        if (ImGui::Button("AI Action End")) NotifyAIActionEnd();
        ImGui::SameLine();
        if (ImGui::Button("Sequencer Action End")) NotifySequencerActionEnd();
        if (ImGui::Button("Test TakeOff")) RequestAction(DRAGON_ACTION_TYPE::TAKE_OFF);
        ImGui::SameLine();
        if (ImGui::Button("Test Hit")) RequestDamageReaction();
        ImGui::SameLine();
        if (ImGui::Button("Test AirHit")) RequestAction(DRAGON_ACTION_TYPE::AIR_GET_HIT);
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
        ImGui::DragFloat("Approach End Distance", &approachEndDistance, 0.1f, 0.0f, 50.0f);
        ImGui::DragFloat("Medium Distance", &mediumDistance, 0.1f, 0.0f, 80.0f);
        ImGui::DragFloat("Far Distance", &farDistance, 0.1f, 0.0f, 120.0f);
        ImGui::DragFloat("TakeOff Back Speed", &takeOffBackSpeed, 0.1f, 0.0f, 40.0f);
        ImGui::DragFloat("Ground Move Turn Speed", &groundMoveTurnSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Air Forward Speed", &airForwardSpeed, 0.1f, 0.0f, 40.0f);
        ImGui::DragFloat("Air Attack Back Speed", &airAttackBackSpeed, 0.1f, 0.0f, 40.0f);
        ImGui::DragFloat("Air Turn Speed", &airTurnSpeed, 1.0f, 0.0f, 360.0f);
        ImGui::DragFloat("Air Hover Height", &airHoverHeight, 0.1f, 0.0f, 80.0f);
        ImGui::DragFloat("Air Hover Adjust Speed", &airHoverAdjustSpeed, 0.1f, 0.0f, 80.0f);
        ImGui::DragFloat("Air Landing Start Height", &airLandingAnimationStartHeight, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Air Landing Descend Speed", &airLandingDescendSpeed, 0.1f, 0.0f, 80.0f);
        ImGui::DragFloat("Air Hover Height Tolerance", &airHoverHeightTolerance, 0.01f, 0.0f, 5.0f);
        ImGui::DragInt("Max Air Attack Count", &maxAirAttackCount, 1.0f, 0, 5);
        ImGui::DragFloat("First Ultimate HP Rate", &firstUltimateHpRate, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Second Ultimate HP Rate", &secondUltimateHpRate, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Ultimate Damage Rate", &ultimateDamageRate, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Turn Speed", &turnSpeed, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Walk Speed", &walkSpeed, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Run Speed", &runSpeed, 0.1f, 0.0f, 60.0f);
        ImGui::DragFloat("Animation Blend", &animationBlendDuration, 0.01f, 0.0f, 2.0f);
        ImGui::Checkbox("Limit Turn RootMotion", &limitTurnRootMotion);
        ImGui::DragFloat("Turn RootMotion Padding", &turnRootMotionLimitPaddingDegrees, 0.1f, 0.0f, 30.0f);
        ImGui::DragFloat("Spin Tail Rotation Degrees", &spinTailRotationDegrees, 1.0f, 0.0f, 360.0f);
        ImGui::SeparatorText("Monster Tactics");
        ImGui::DragFloat("Cracked Aggression", &crackedAggressionMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Dark Leak Aggression", &darkLeakAggressionMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Collapse Aggression", &collapseAggressionMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Repeat Action Penalty", &repeatActionPenaltyRate, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Under Body Punish", &underBodyPunishMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Side Punish", &sidePunishMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Behind Punish", &behindPunishMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Long Range Pressure", &longRangePressureMultiplier, 0.01f, 0.1f, 5.0f);
        ImGui::Checkbox("Animation Graph Driven", &animationGraphDriven);
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

        if (ImGui::Button("Force Idle"))
        {
            ForceAction(DRAGON_ACTION_TYPE::IDLE);
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Turn"))
        {
            ForceAction(DRAGON_ACTION_TYPE::TURN_TO_PLAYER);
        }
        ImGui::SameLine();
        if (ImGui::Button("Force Walk"))
        {
            ForceAction(DRAGON_ACTION_TYPE::WALK_APPROACH);
        }
        if (ImGui::Button("Test SpreadBreath")) RequestAction(DRAGON_ACTION_TYPE::SPREAD_BREATH);
        ImGui::SameLine();
        if (ImGui::Button("Test TailSweep")) RequestAction(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP);
    }

    DrawHistoryDebug();
    if (ImGui::CollapsingHeader("Action Toggles", ImGuiTreeNodeFlags_None))
    {
        BS_PROFILE_SCOPE("DragonAI GUI::Action Toggles");

        for (DragonActionDefinition& definition_ : actionDefinitions)
        {
            ImGui::PushID(static_cast<int>(definition_.action));
            ImGui::Checkbox("##Enabled", &definition_.enabled);
            ImGui::SameLine();
            ImGui::Text("%s  %s  CD %.2f",
                GetDragonActionName(definition_.action),
                GetDragonActionFinishModeName(definition_.finishMode),
                GetCooldown(definition_.action));
            ImGui::SameLine();
            if (ImGui::SmallButton("Run"))
            {
                RequestAction(definition_.action);
            }
            ImGui::SameLine();
            ImGui::InputText("Skill Name (optional)", &definition_.skillName);
            ImGui::Checkbox("Timeout", &definition_.useActionTimeout);
            ImGui::Checkbox("History", &definition_.useHistory);
            ImGui::DragFloat("Base Weight", &definition_.selectionWeight, 0.05f, 0.0f, 100.0f);
            ImGui::DragFloat("Cooldown", &definition_.cooldownTime, 0.25f, 0.0f, 300.0f);
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
    BS_PROFILE_SCOPE("DragonAIComponent::Serialize");

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
    json_["ApproachEndDistance"] = approachEndDistance;
    json_["MediumDistance"] = mediumDistance;
    json_["FarDistance"] = farDistance;
    json_["TakeOffBackSpeed"] = takeOffBackSpeed;
    json_["GroundMoveTurnSpeed"] = groundMoveTurnSpeed;
    json_["AirForwardSpeed"] = airForwardSpeed;
    json_["AirAttackBackSpeed"] = airAttackBackSpeed;
    json_["AirTurnSpeed"] = airTurnSpeed;
    json_["AirHoverHeight"] = airHoverHeight;
    json_["AirHoverAdjustSpeed"] = airHoverAdjustSpeed;
    json_["AirLandingAnimationStartHeight"] = airLandingAnimationStartHeight;
    json_["AirLandingDescendSpeed"] = airLandingDescendSpeed;
    json_["AirHoverHeightTolerance"] = airHoverHeightTolerance;
    json_["MaxAirAttackCount"] = maxAirAttackCount;
    json_["FirstUltimateHpRate"] = firstUltimateHpRate;
    json_["SecondUltimateHpRate"] = secondUltimateHpRate;
    json_["UltimateDamageRate"] = ultimateDamageRate;
    json_["FirstUltimateTriggered"] = firstUltimateTriggered;
    json_["SecondUltimateTriggered"] = secondUltimateTriggered;
    json_["TurnSpeed"] = turnSpeed;
    json_["WalkSpeed"] = walkSpeed;
    json_["RunSpeed"] = runSpeed;
    json_["AnimationBlendDuration"] = animationBlendDuration;
    json_["LimitTurnRootMotion"] = limitTurnRootMotion;
    json_["TurnRootMotionLimitPaddingDegrees"] = turnRootMotionLimitPaddingDegrees;
    json_["SpinTailRotationDegrees"] = spinTailRotationDegrees;
    json_["CrackedAggressionMultiplier"] = crackedAggressionMultiplier;
    json_["DarkLeakAggressionMultiplier"] = darkLeakAggressionMultiplier;
    json_["CollapseAggressionMultiplier"] = collapseAggressionMultiplier;
    json_["RepeatActionPenaltyRate"] = repeatActionPenaltyRate;
    json_["HistoryEnabled"] = historyEnabled;
    json_["HistoryRecoveryCount"] = historyRecoveryCount;
    json_["HistoryStarvationCount"] = historyStarvationCount;
    json_["HistoryMaxBonus"] = historyMaxBonus;
    json_["AttackCooldownMultiplier"] = attackCooldownMultiplier;
    json_["UnderBodyPunishMultiplier"] = underBodyPunishMultiplier;
    json_["SidePunishMultiplier"] = sidePunishMultiplier;
    json_["BehindPunishMultiplier"] = behindPunishMultiplier;
    json_["LongRangePressureMultiplier"] = longRangePressureMultiplier;
    json_["AnimationGraphDriven"] = animationGraphDriven;

    json_["SkillDefinitionsPath"] = skillDefinitionsPath;
    json_["ActionDefinitions"] = nlohmann::json::array();
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        json_["ActionDefinitions"].push_back({
            { "Action", static_cast<int>(definition_.action) },
            { "ActionName", definition_.actionName },
            { "ActionSequenceName", definition_.actionSequenceName },
            { "SkillName", definition_.skillName },
            { "AnimationStateName", definition_.animationStateName },
            { "AnimationActionName", definition_.animationActionName },
            { "IKActionName", definition_.ikActionName },
            { "HitBoxActionName", definition_.hitBoxActionName },
            { "VFXActionName", definition_.vfxActionName },
            { "Duration", definition_.duration },
            { "RecoveryTime", definition_.recoveryTime },
            { "CooldownTime", definition_.cooldownTime },
            { "SelectionWeight", definition_.selectionWeight },
            { "UseHistory", definition_.useHistory },
            { "Enabled", definition_.enabled },
            { "WaitForActionEnd", definition_.waitForActionEnd },
            { "AllowBuffer", definition_.allowBuffer },
            { "UseActionTimeout", definition_.useActionTimeout },
            { "ActionTimeout", definition_.actionTimeout },
            { "FinishMode", static_cast<int>(definition_.finishMode) },
            });
    }
}

void DragonAIComponent::Deserialize(const nlohmann::json& json_)
{
    skillDefinitionsPath = json_.value("SkillDefinitionsPath", std::string("Data/Skills/PlayerSkills.json"));
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
    behaviorGraphTickInterval = json_.value("BehaviorGraphTickInterval", 0.1f);
    frontAngleDegrees = json_.value("FrontAngleDegrees", 115.0f);
    sideAngleDegrees = json_.value("SideAngleDegrees", 135.0f);
    underBodyDistance = json_.value("UnderBodyDistance", 3.2f);
    biteDistance = json_.value("BiteDistance", 4.8f);
    approachEndDistance = json_.value("ApproachEndDistance", 5.2f);
    mediumDistance = json_.value("MediumDistance", 16.0f);
    farDistance = json_.value("FarDistance", 22.0f);
    takeOffBackSpeed = json_.value("TakeOffBackSpeed", 7.0f);
    groundMoveTurnSpeed = json_.value("GroundMoveTurnSpeed", 2.0f);
    airForwardSpeed = json_.value("AirForwardSpeed", 3.0f);
    airAttackBackSpeed = json_.value("AirAttackBackSpeed", 2.0f);
    airTurnSpeed = json_.value("AirTurnSpeed", 75.0f);
    airHoverHeight = json_.value("AirHoverHeight", 10.0f);
    airHoverAdjustSpeed = json_.value("AirHoverAdjustSpeed", 8.0f);
    airLandingAnimationStartHeight = json_.value("AirLandingAnimationStartHeight", 0.5f);
    airLandingDescendSpeed = json_.value("AirLandingDescendSpeed", 12.0f);
    airHoverHeightTolerance = json_.value("AirHoverHeightTolerance", 0.25f);
    maxAirAttackCount = json_.value("MaxAirAttackCount", 1);
    firstUltimateHpRate = json_.value("FirstUltimateHpRate", 0.5f);
    secondUltimateHpRate = json_.value("SecondUltimateHpRate", 0.1f);
    ultimateDamageRate = json_.value("UltimateDamageRate", 0.1f);
    firstUltimateTriggered = json_.value("FirstUltimateTriggered", false);
    secondUltimateTriggered = json_.value("SecondUltimateTriggered", false);
    turnSpeed = json_.value("TurnSpeed", 6.0f);
    walkSpeed = json_.value("WalkSpeed", 5.0f);
    runSpeed = json_.value("RunSpeed", 12.0f);
    animationBlendDuration = json_.value("AnimationBlendDuration", 0.2f);
    limitTurnRootMotion = json_.value("LimitTurnRootMotion", true);
    turnRootMotionLimitPaddingDegrees = json_.value("TurnRootMotionLimitPaddingDegrees", 2.0f);
    spinTailRotationDegrees = json_.value("SpinTailRotationDegrees", 360.0f);
    crackedAggressionMultiplier = json_.value("CrackedAggressionMultiplier", 1.10f);
    darkLeakAggressionMultiplier = json_.value("DarkLeakAggressionMultiplier", 1.25f);
    collapseAggressionMultiplier = json_.value("CollapseAggressionMultiplier", 1.45f);
    repeatActionPenaltyRate = json_.value("RepeatActionPenaltyRate", 0.35f);
    historyEnabled = json_.value("HistoryEnabled", true);
    historyRecoveryCount = (std::max)(1, json_.value("HistoryRecoveryCount", 3));
    historyStarvationCount = (std::max)(1, json_.value("HistoryStarvationCount", 5));
    historyMaxBonus = (std::max)(1.0f, json_.value("HistoryMaxBonus", 4.0f));
    attackCooldownMultiplier = (std::max)(0.0f, json_.value("AttackCooldownMultiplier", 5.0f));
    underBodyPunishMultiplier = json_.value("UnderBodyPunishMultiplier", 1.60f);
    sidePunishMultiplier = json_.value("SidePunishMultiplier", 1.35f);
    behindPunishMultiplier = json_.value("BehindPunishMultiplier", 1.70f);
    longRangePressureMultiplier = json_.value("LongRangePressureMultiplier", 1.45f);
    animationGraphDriven = json_.value("AnimationGraphDriven", true);

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
            definition_->skillName = definitionJson_.value("SkillName", std::string());
            definition_->animationStateName = definitionJson_.value("AnimationStateName", definition_->animationStateName);
            definition_->animationActionName = definitionJson_.value("AnimationActionName", definition_->animationActionName);
            definition_->ikActionName = definitionJson_.value("IKActionName", definition_->ikActionName);
            definition_->hitBoxActionName = definitionJson_.value("HitBoxActionName", definition_->hitBoxActionName);
            definition_->vfxActionName = definitionJson_.value("VFXActionName", definition_->vfxActionName);
            definition_->duration = definitionJson_.value("Duration", definition_->duration);
            definition_->recoveryTime = definitionJson_.value("RecoveryTime", definition_->recoveryTime);
            definition_->cooldownTime = definitionJson_.value("CooldownTime", definition_->cooldownTime);
            definition_->selectionWeight = definitionJson_.value("SelectionWeight", definition_->selectionWeight);
            definition_->useHistory = definitionJson_.value("UseHistory", true);
            definition_->enabled = definitionJson_.value("Enabled", definition_->enabled);
            definition_->waitForActionEnd = definitionJson_.value("WaitForActionEnd", definition_->waitForActionEnd);
            definition_->allowBuffer = definitionJson_.value("AllowBuffer", definition_->allowBuffer);
            definition_->useActionTimeout = definitionJson_.value("UseActionTimeout", definition_->useActionTimeout);
            definition_->actionTimeout = definitionJson_.value("ActionTimeout", definition_->actionTimeout);
            definition_->finishMode = static_cast<DRAGON_ACTION_FINISH_MODE>(
                definitionJson_.value("FinishMode", static_cast<int>(definition_->finishMode)));
        }
    }
    ApplyActionDefinitionRuntimePolicies();

    if (behaviorGraphEnabled && !behaviorGraphAssetPath.empty())
    {
        BS_PROFILE_SCOPE("DragonAI::LoadBehaviorGraphOnDeserialize");
        behaviorGraph.LoadFromFile(behaviorGraphAssetPath);
    }
    if (behaviorGraphEnabled)
    {
        EnsureDefaultBehaviorGraph();
    }
    RegisterActions();
}

bool DragonAIComponent::RequestAction(DRAGON_ACTION_TYPE action_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::RequestAction");

    if (action_ == DRAGON_ACTION_TYPE::TURN_TO_PLAYER)
    {
        const DRAGON_ACTION_TYPE antiFlankAction_ = ResolveAntiFlankAction();
        if (antiFlankAction_ != DRAGON_ACTION_TYPE::IDLE)
        {
            action_ = antiFlankAction_;
        }
    }

    // 通常リクエストは有効/クールダウン/キャンセル可否を見て、必要ならバッファへ積む。
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
    if ((action_ == DRAGON_ACTION_TYPE::WALK_APPROACH ||
        action_ == DRAGON_ACTION_TYPE::RUN_CHARGE) &&
        !CanUseGroundApproach(context))
    {
        statusText = "Rejected ground approach: target is not in approach lane.";
        lastRejectedAction = action_;
        return false;
    }
    if (action_ == DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP &&
        !CanUseTurnTailSweep(context))
    {
        // Graphから直接呼ばれた尻尾攻撃が届かない時は、停止せず向き直りや離脱へ逃がす。
        if (context.playerHorizontalDistance <= biteDistance + 1.5f &&
            IsActionAvailable(DRAGON_ACTION_TYPE::TAKE_OFF))
        {
            action_ = DRAGON_ACTION_TYPE::TAKE_OFF;
        }
        else if (IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TO_PLAYER))
        {
            action_ = DRAGON_ACTION_TYPE::TURN_TO_PLAYER;
        }
        else
        {
            statusText = "Rejected tail sweep: target is outside tail hit range.";
            lastRejectedAction = DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP;
            return false;
        }
    }
    if (action_ == DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP &&
        !CanUseSpinTailSweep(context))
    {
        // 回転尻尾も射程外では固まらないよう、状況に合う基本行動へ戻す。
        if (context.playerHorizontalDistance <= biteDistance + 1.5f &&
            IsActionAvailable(DRAGON_ACTION_TYPE::TAKE_OFF))
        {
            action_ = DRAGON_ACTION_TYPE::TAKE_OFF;
        }
        else if (IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TO_PLAYER))
        {
            action_ = DRAGON_ACTION_TYPE::TURN_TO_PLAYER;
        }
        else
        {
            statusText = "Rejected spin tail sweep: target is outside spin hit range.";
            lastRejectedAction = DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP;
            return false;
        }
    }
    // 正面足元では、Graphの離陸要求も使用可能な地上近接攻撃へ振り分ける。
    if (action_ == DRAGON_ACTION_TYPE::TAKE_OFF &&
        hasPlayerTarget && context.isPlayerInFront &&
        context.playerHorizontalDistance <= underBodyDistance + 1.0f)
    {
        if (IsActionAvailable(DRAGON_ACTION_TYPE::GROUND_STAMP) &&
            !(historyEnabled && lastExecutedAttack == DRAGON_ACTION_TYPE::GROUND_STAMP))
        {
            action_ = DRAGON_ACTION_TYPE::GROUND_STAMP;
        }
        else if (IsActionAvailable(DRAGON_ACTION_TYPE::BITE) &&
            !(historyEnabled && lastExecutedAttack == DRAGON_ACTION_TYPE::BITE))
        {
            action_ = DRAGON_ACTION_TYPE::BITE;
        }
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

bool DragonAIComponent::RequestWeightedAction()
{
    BS_PROFILE_SCOPE("DragonAIComponent::RequestWeightedAction");

    // フォールバックAIの選択結果を通常リクエストへ流し、候補が無い場合はIdleへ戻す。
    const DRAGON_ACTION_TYPE action_ = SelectNextAction(context);
    if (action_ == DRAGON_ACTION_TYPE::IDLE)
    {
        statusText = "Weighted action found no candidate.";
        FinishRecovery();
        SendRuntimeToBlackboard();
        return false;
    }

    const bool requested_ = RequestAction(action_);
    if (!requested_ && aiState == DRAGON_AI_STATE::IDLE)
    {
        runtime.actionEnded = true;
        SendRuntimeToBlackboard();
    }
    return requested_;
}

void DragonAIComponent::RequestDamageReaction()
{
    BS_PROFILE_SCOPE("DragonAIComponent::RequestDamageReaction");

    // ダメージ反応は現在の空中/地上状態で使うリアクションを切り替える。
    if (runtime.currentAction == DRAGON_ACTION_TYPE::DEATH ||
        aiState == DRAGON_AI_STATE::DEAD)
    {
        return;
    }

    ForceAction(IsAirborneAction(runtime.currentAction) ||
        aiState == DRAGON_AI_STATE::AIR
        ? DRAGON_ACTION_TYPE::AIR_GET_HIT
        : DRAGON_ACTION_TYPE::GET_HIT);
}

void DragonAIComponent::ForceAction(DRAGON_ACTION_TYPE action_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::ForceAction");

    // Forceはデバッグやイベント用の即時遷移で、通常のキャンセル可否やクールダウンを無視する。
    if (action_ == DRAGON_ACTION_TYPE::IDLE)
    {
        FinishRecovery();
        SendRuntimeToBlackboard();
        return;
    }
    if (!FindDefinition(action_))
    {
        lastRejectedAction = action_;
        return;
    }

    StopMovement();
    runtime.bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    behaviorGraph.ResetRuntime();
    StartAction(action_);
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
    BS_PROFILE_SCOPE("DragonAIComponent::NotifyActionEnd");
    NotifySequencerActionEnd();
}

void DragonAIComponent::NotifySequencerActionEnd()
{
    if (skillActionRunning)
    {
        // 既存Animation Eventの通知をPhaseに渡し、旧Action全体を途中で終了させない。
        skillExecution.NotifyPhaseEnd();
        return;
    }
    BS_PROFILE_SCOPE("DragonAIComponent::NotifySequencerActionEnd");

    // Sequencer管理の行動だけ完了通知を受け付け、AI管理行動の誤終了を防ぐ。
    if (!IsSequencerControlledAction(runtime.currentAction))
    {
        statusText = "Ignored SequencerActionEnd: " +
            std::string(GetDragonActionName(runtime.currentAction)) +
            " is AI controlled.";
        SendRuntimeToBlackboard();
        return;
    }

    pendingSequencerActionEnd = true;
    statusText = "Queued SequencerActionEnd: " +
        std::string(GetDragonActionName(runtime.currentAction));
    SendRuntimeToBlackboard();
}

void DragonAIComponent::NotifyAIActionEnd()
{
    if (skillActionRunning)
    {
        skillExecution.NotifyPhaseEnd();
        return;
    }
    BS_PROFILE_SCOPE("DragonAIComponent::NotifyAIActionEnd");
    // AI管理の行動だけ即時完了させ、Sequencer管理行動はアニメーション側の通知を待つ。
    if (IsSequencerControlledAction(runtime.currentAction))
    {
        statusText = "Ignored AIActionEnd: " +
            std::string(GetDragonActionName(runtime.currentAction)) +
            " is Sequencer controlled.";
        return;
    }

    CompleteActionEnd();
}

void DragonAIComponent::CompleteActionEnd()
{
    if (skillActionRunning)
    {
        skillExecution.Stop();
        skillActionRunning = false;
    }
    BS_PROFILE_SCOPE("DragonAIComponent::CompleteActionEnd");

    // Sequencer/AIからの完了通知を受けて、空中・死亡・復帰・バッファ消費の次状態を決める。
    if (aiState != DRAGON_AI_STATE::ACTION &&
        aiState != DRAGON_AI_STATE::AIR)
    {
        return;
    }

    runtime.actionEnded = true;
    runtime.canCancel = true;
    runtime.canBufferAction = false;
    ClearTurnRootMotionLimit();
    DisableSequencerHitBoxes();
    ClearTailIK();
    if (const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction))
    {
        const std::string animationActionName_ = ResolveAnimationActionName(*definition_);
        const std::string animationStateName_ = ResolveAnimationStateName(*definition_);
        ApplyAnimationBlackboard(animationActionName_, animationStateName_, false);
    }
    if (runtime.currentAction == DRAGON_ACTION_TYPE::DEATH ||
        runtime.currentAction == DRAGON_ACTION_TYPE::DEATH_HIT_GROUND)
    {
        aiState = DRAGON_AI_STATE::DEAD;
        SetAirbornePhysicsEnabled(false);
        StopMovement();
        SendRuntimeToBlackboard();
        return;
    }

    if (IsRepeatLimitedAttack(runtime.currentAction))
    {
        lastCompletedAttackAction = runtime.currentAction;
    }

    const DRAGON_ACTION_TYPE completedAction_ = runtime.currentAction;
    if (completedAction_ == DRAGON_ACTION_TYPE::TAKE_OFF)
    {
        pendingAirAttackDecision = true;
        airAttackCount = 0;
        StartAction(DRAGON_ACTION_TYPE::AIR_HOLD);
        return;
    }
    if (completedAction_ == DRAGON_ACTION_TYPE::AIR_FIRE_BALL ||
        completedAction_ == DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH)
    {
        ++airAttackCount;
        pendingAirAttackDecision = airAttackCount < (std::max)(maxAirAttackCount, 0);
        pendingAirLandingDecision = !pendingAirAttackDecision;
        StartAction(DRAGON_ACTION_TYPE::AIR_HOLD);
        return;
    }
    if (completedAction_ == DRAGON_ACTION_TYPE::LANDING_ATTACK)
    {
        SetAirbornePhysicsEnabled(false);
        waitingForLandingGround = true;
        StopMovement();
    }
    if (completedAction_ == DRAGON_ACTION_TYPE::AIR_GET_HIT)
    {
        pendingAirAttackDecision = false;
        pendingAirLandingDecision = true;
        StartAction(DRAGON_ACTION_TYPE::AIR_HOLD);
        return;
    }
    if (completedAction_ == DRAGON_ACTION_TYPE::AIR_DEATH_FALL_START)
    {
        SetAirbornePhysicsEnabled(false);
        StartAction(IsGrounded()
            ? DRAGON_ACTION_TYPE::DEATH_HIT_GROUND
            : DRAGON_ACTION_TYPE::FALLING);
        return;
    }
    if (completedAction_ == DRAGON_ACTION_TYPE::FALLING)
    {
        StartAction(DRAGON_ACTION_TYPE::DEATH_HIT_GROUND);
        return;
    }
    if (completedAction_ == DRAGON_ACTION_TYPE::ULTIMATE_ATTACK)
    {
        pendingUltimateAction = false;
        ultimateDamageReductionActive = false;
    }

    if (ConsumeBufferedAction())
    {
        return;
    }

    RequestIdleGraphTransition();

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
    BS_PROFILE_SCOPE("DragonAIComponent::SelectNextAction");
    if (hasPlayerTarget &&
        !context_.isPlayerInFront &&
        IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TO_PLAYER))
    {
        return DRAGON_ACTION_TYPE::TURN_TO_PLAYER;
    }

    std::vector<DragonActionCandidate> candidates_;
    AddActionCandidates(context_, candidates_);
    return SelectHistoryWeightedAction(candidates_);
}

// 条件を満たした候補へ履歴補正と連続使用除外を適用して抽選する。
DRAGON_ACTION_TYPE DragonAIComponent::SelectHistoryWeightedAction(std::vector<DragonActionCandidate>& candidates_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::SelectHistoryWeightedAction");
    std::vector<DRAGON_ACTION_TYPE> actions_;
    for (const DragonActionCandidate& candidate_ : candidates_) actions_.push_back(candidate_.action);
    PrepareHistorySelection(actions_);
    for (DragonActionSelectionDebug& debug_ : selectionDebugWeights)
    {
        debug_.baseWeight = 0.0f;
        debug_.finalWeight = 0.0f;
    }
    for (DragonActionCandidate& candidate_ : candidates_)
    {
        DragonActionSelectionDebug& debug_ = selectionDebugWeights[ToIndex(candidate_.action)];
        debug_.baseWeight += candidate_.weight;
        debug_.historyModifier = CalculateHistoryModifier(candidate_.action);
        candidate_.weight *= CalculateHistoryModifier(candidate_.action);
        if (excludePreviousAttack && candidate_.action == lastExecutedAttack) candidate_.weight = 0.0f;
        debug_.finalWeight += candidate_.weight;
    }
    candidates_.erase(std::remove_if(candidates_.begin(), candidates_.end(),
        [](const DragonActionCandidate& candidate_) { return candidate_.weight <= 0.0f; }), candidates_.end());
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
    BS_PROFILE_SCOPE("DragonAIComponent::AddActionCandidates");

    if (context_.isPlayerUnderBody && context_.canUseBite)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::BITE, 8.0f);
    }
    // 前足の近くでは地上スタンプを候補に加え、噛みつきと併せて近接攻撃を選びやすくする。
    if (context_.isPlayerInFront &&
        context_.playerHorizontalDistance <= underBodyDistance + 1.0f &&
        context_.canUseGroundStamp)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::GROUND_STAMP, 6.5f);
    }
    if ((context_.isPlayerUnderBody || context_.playerHorizontalDistance <= biteDistance + 1.5f) &&
        context_.canUseTakeOff)
    {
        const float takeOffWeight_ = context_.isPlayerInFront ? 0.7f : 2.0f;
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::TAKE_OFF, takeOffWeight_);
    }
    const bool canUseTurnTailSweep_ =
        context_.canUseTailSweep &&
        CanUseTurnTailSweep(context_);
    const bool canUseSpinTailSweep_ =
        context_.canUseSpinSweep &&
        CanUseSpinTailSweep(context_);
    const float biteCandidateDistance_ = (std::max)(biteDistance, approachEndDistance);

    if (ShouldTurnForCloseFrontTarget(context_) &&
        IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TO_PLAYER))
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::TURN_TO_PLAYER, 3.0f);
    }
    if (context_.isPlayerBehind && canUseTurnTailSweep_)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, 4.0f);
    }
    if (context_.isPlayerBehind && canUseSpinTailSweep_)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, 1.2f);
    }
    if (context_.isPlayerInSide && canUseTurnTailSweep_)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, 1.4f);
    }
    if (context_.isPlayerInSide && canUseSpinTailSweep_)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, 0.6f);
    }
    if (context_.isPlayerInSide)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::TURN_TO_PLAYER, 1.2f);
    }
    if (context_.isPlayerInFront &&
        context_.playerHorizontalDistance <= biteCandidateDistance_ &&
        context_.canUseBite)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::BITE, 8.0f);
    }
    if (context_.isPlayerInFront &&
        context_.playerHorizontalDistance >= mediumDistance &&
        context_.playerHorizontalDistance < farDistance)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::FIRE_BALL, 0.7f);
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::FIRE_BALL_FRONT, 1.1f);
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE, 1.2f);
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP, 1.1f);
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP, 1.15f);
    }
    if (context_.playerHorizontalDistance >= farDistance)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::SPREAD_BREATH, 1.1f);
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD, 1.0f);
    }
    if (CanUseGroundApproach(context_) &&
        context_.playerHorizontalDistance >= mediumDistance &&
        context_.canUseRunCharge)
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::RUN_CHARGE, 1.5f);
    }
    if (CanUseGroundApproach(context_))
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::WALK_APPROACH, 0.25f);
    }
    else if (context_.isPlayerInFront &&
        context_.playerHorizontalDistance > biteDistance &&
        context_.playerHorizontalDistance <= approachEndDistance + 1.0f &&
        IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TO_PLAYER))
    {
        AddWeightedActionCandidate(context_, candidates_, DRAGON_ACTION_TYPE::TURN_TO_PLAYER, 1.0f);
    }
}

float DragonAIComponent::CalculateAggressionMultiplier(const DragonAIContext& context_) const
{
    if (context_.hpRate <= 0.15f)
    {
        return (std::max)(0.1f, collapseAggressionMultiplier);
    }
    if (context_.hpRate <= 0.4f)
    {
        return (std::max)(0.1f, darkLeakAggressionMultiplier);
    }
    if (context_.hpRate <= 0.7f)
    {
        return (std::max)(0.1f, crackedAggressionMultiplier);
    }
    return 1.0f;
}

float DragonAIComponent::CalculateActionSituationWeight(
    DRAGON_ACTION_TYPE action_,
    const DragonAIContext& context_,
    float baseWeight_) const
{
    // 距離、向き、HP段階から候補重みを補正し、怪物が状況に合わせて圧を変える。
    float weight_ = baseWeight_ * GetSelectionWeight(action_);
    if (weight_ <= 0.0f)
    {
        return 0.0f;
    }

    const bool isAttackAction_ =
        action_ != DRAGON_ACTION_TYPE::IDLE &&
        action_ != DRAGON_ACTION_TYPE::WALK_APPROACH &&
        action_ != DRAGON_ACTION_TYPE::TURN_TO_PLAYER &&
        action_ != DRAGON_ACTION_TYPE::AIR_HOLD;
    if (isAttackAction_)
    {
        weight_ *= CalculateAggressionMultiplier(context_);
    }

    if (context_.isPlayerUnderBody &&
        (action_ == DRAGON_ACTION_TYPE::BITE ||
            action_ == DRAGON_ACTION_TYPE::TAKE_OFF ||
            action_ == DRAGON_ACTION_TYPE::GROUND_STAMP ||
            action_ == DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP))
    {
        weight_ *= (std::max)(0.1f, underBodyPunishMultiplier);
    }
    if (context_.isPlayerInSide &&
        (action_ == DRAGON_ACTION_TYPE::TURN_TO_PLAYER ||
            action_ == DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP ||
            action_ == DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP))
    {
        weight_ *= (std::max)(0.1f, sidePunishMultiplier);
    }
    if (context_.isPlayerBehind &&
        (action_ == DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP ||
            action_ == DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP ||
            action_ == DRAGON_ACTION_TYPE::TAKE_OFF))
    {
        weight_ *= (std::max)(0.1f, behindPunishMultiplier);
    }
    if (context_.playerDistance >= farDistance &&
        (action_ == DRAGON_ACTION_TYPE::RUN_CHARGE ||
            action_ == DRAGON_ACTION_TYPE::SPREAD_BREATH ||
            action_ == DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD))
    {
        weight_ *= (std::max)(0.1f, longRangePressureMultiplier);
    }

    return weight_;
}

void DragonAIComponent::AddWeightedActionCandidate(
    const DragonAIContext& context_,
    std::vector<DragonActionCandidate>& candidates_,
    DRAGON_ACTION_TYPE action_,
    float baseWeight_) const
{
    if (baseWeight_ <= 0.0f || !IsActionAvailable(action_))
    {
        return;
    }

    const float weight_ = CalculateActionSituationWeight(action_, context_, baseWeight_);
    if (weight_ <= 0.0f)
    {
        return;
    }

    candidates_.push_back({ action_, weight_ });
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
    animationGraph = AnimationGraphBridgeComponent::Acquire(owner_);
    movement = owner_->GetComponent<CharacterMovementComponent>();
    neckIK = owner_->GetComponent<DragonNeckCommandIKComponent>();
    tailIK = owner_->GetComponent<DragonTailCommandIKComponent>();
    hasAnimationController = !animationGraph.expired();
    hasMovementComponent = !movement.expired();
    hasNeckIK = !neckIK.expired();
    hasTailIK = !tailIK.expired();

    if (playerObjectName.empty())
    {
        playerObject.reset();
        hasPlayerTarget = false;
        return;
    }
    const auto observation_ = owner_->GetManager().GetCombatObservationSystem();
    const auto cachedPlayer_ = playerObject.lock();
    if (!observation_) { playerObject.reset(); hasPlayerTarget = false; return; }
    // Playerが有効な間は保持した参照を使い、破棄・名前変更時だけ登録索引から再取得する。
    if (!cachedPlayer_ || cachedPlayer_->GetName() != playerObjectName || !observation_->IsRegisteredObject(cachedPlayer_))
        playerObject = observation_->FindObject(playerObjectName);
    hasPlayerTarget = !playerObject.expired();
}

void DragonAIComponent::InitializeDefaultDefinitions()
{
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
            definition_.finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
            actionDefinitions.push_back(definition_);
        };

    auto addMissingDefinition_ = [this, &addDefinition_](
        DRAGON_ACTION_TYPE action_,
        const std::string& sequenceActionName_,
        const std::string& animationStateName_,
        const std::string& ikActionName_,
        float duration_,
        float recoveryTime_,
        float cooldownTime_,
        float actionTimeout_)
        {
            if (FindDefinition(action_)) return;
            addDefinition_(
                action_,
                sequenceActionName_,
                animationStateName_,
                ikActionName_,
                duration_,
                recoveryTime_,
                cooldownTime_,
                actionTimeout_);
        };

    addMissingDefinition_(DRAGON_ACTION_TYPE::IDLE, "Idle", "PlainsDragon_IdleBreathe", "", 0.1f, 0.0f, 0.0f, 1.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::WALK_APPROACH, "WalkApproach", "PlainsDragon_Walk", "", 0.9f, 0.1f, 0.2f, 1.2f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::RUN_CHARGE, "RunCharge", "PlainsDragon_Run", "", 1.4f, 0.4f, 3.0f, 2.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::TURN_TO_PLAYER, "TurnToPlayer", "PlainsDragon_Turn90L_RM", "", 0.6f, 0.1f, 0.4f, 1.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::BITE, "Bite", "PlainsDragon_BiteAttack", "", 1.0f, 0.35f, 1.2f, 2.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::FIRE_BALL, "FireBall", "PlainsDragon_SpitFireBall", "", 1.4f, 0.5f, 2.0f, 2.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::SPREAD_BREATH, "SpreadBreath", "PlainsDragon_SpreadFire", "", 2.0f, 0.6f, 3.0f, 3.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, "TurnTailSweep", "PlainsDragon_Turn90L_RM", "", 1.2f, 0.45f, 2.0f, 2.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, "SpinTailSweep", "PlainsDragon_Turn90L_RM", "", 2.0f, 0.65f, 5.0f, 3.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::TAKE_OFF, "TakeOff", "PlainsDragon_TakeOff", "", 1.4f, 0.0f, 5.0f, 2.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::AIR_HOLD, "AirHold", "PlainsDragon_Fly", "", 1.0f, 0.0f, 0.2f, 2.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::AIR_FIRE_BALL, "AirFireBall", "PlainsDragon_FlyStationarySpitFireBall", "", 1.3f, 0.0f, 2.0f, 2.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH, "AirSpreadBreath", "PlainsDragon_FlyStationarySpreadFire", "", 2.0f, 0.0f, 3.0f, 3.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::LANDING_ATTACK, "LandingAttack", "PlainsDragon_FlyStationaryToLanding", "", 1.5f, 0.6f, 4.0f, 2.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::GET_HIT, "GetHit", "PlainsDragon_GetHit1", "", 0.5f, 0.2f, 0.5f, 1.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::DEATH, "Death", "PlainsDragon_Death", "", 3.0f, 0.0f, 0.0f, 5.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP, "FireBallSweep", "PlainsDragon_FireBeam_Sweep45", "", 1.6f, 0.5f, 2.5f, 2.8f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD, "SpreadBreathForward", "PlainsDragon_SpreadFire_Forward", "", 2.0f, 0.6f, 3.5f, 3.5f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::AIR_GET_HIT, "AirGetHit", "PlainsDragon_FlyStationaryGetHit", "", 0.5f, 0.0f, 0.5f, 1.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::AIR_DEATH_FALL_START, "AirDeathFallStart", "PlainsDragon_FlyStationaryGetHitToFalling", "", 1.0f, 0.0f, 0.0f, 2.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::FALLING, "Falling", "PlainsDragon_Falling", "", 1.0f, 0.0f, 0.0f, 0.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::DEATH_HIT_GROUND, "DeathHitGround", "PlainsDragon_DeathHitTheGround", "", 1.5f, 0.0f, 0.0f, 3.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::ULTIMATE_ATTACK, "DragonUltimate_Start", "PlainsDragon_SpreadFire", "", 5.0f, 0.5f, 0.0f, 6.0f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::FIRE_BALL_FRONT, "FireBallFront", "PlainsDragon_SpitFireBall_Front", "", 2.2f, 0.55f, 4.0f, 3.2f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE, "FireBallTriple", "PlainsDragon_SpitFireBall_Triple", "", 3.4f, 0.70f, 6.0f, 4.4f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP, "FireBeamSweep", "PlainsDragon_FireBeam_Sweep45", "", 3.2f, 0.70f, 6.0f, 4.2f);
    addMissingDefinition_(DRAGON_ACTION_TYPE::GROUND_STAMP, "GroundStamp", "DragonGroundStamp", "", 1.8f, 0.45f, 4.0f, 2.8f);

    ApplyActionDefinitionRuntimePolicies();

    actionCooldowns.assign(DRAGON_ACTION_COUNT, 0.0f);
}

void DragonAIComponent::ApplyActionDefinitionRuntimePolicies()
{
    BS_PROFILE_SCOPE("DragonAIComponent::ApplyActionDefinitionRuntimePolicies");

    // 抽選対象の攻撃のWeightとCooldownは定義・JSON・Editorの設定を優先する。

    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::WALK_APPROACH))
    {
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::AI_CONDITION;
        definition_->waitForActionEnd = true;
        definition_->useActionTimeout = false;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::RUN_CHARGE))
    {
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::AI_CONDITION;
        definition_->waitForActionEnd = true;
        definition_->useActionTimeout = false;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::TURN_TO_PLAYER))
    {
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
        definition_->waitForActionEnd = true;
        definition_->useActionTimeout = true;
        definition_->cooldownTime = 0.0f;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::BITE))
    {
        definition_->hitBoxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::FIRE_BALL))
    {
        definition_->recoveryTime = 0.35f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP))
    {
        definition_->recoveryTime = 0.40f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::SPREAD_BREATH))
    {
        definition_->recoveryTime = 0.45f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD))
    {
        definition_->recoveryTime = 0.45f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::FIRE_BALL_FRONT))
    {
        definition_->recoveryTime = 0.35f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE))
    {
        definition_->recoveryTime = 0.45f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP))
    {
        definition_->recoveryTime = 0.45f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP))
    {
        definition_->hitBoxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP))
    {
        // 専用アニメが無い回転尻尾攻撃はTurn90を土台にし、IKと判定を時間制御で重ねる。
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::AI_DURATION;
        definition_->waitForActionEnd = false;
        definition_->useActionTimeout = false;
        definition_->animationStateName = "Turn90L";
        definition_->duration = 1.8f;
        definition_->recoveryTime = 0.45f;
        definition_->hitBoxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::GROUND_STAMP))
    {
        // 回転アニメを別ステートで使い、モデル回転なしの足元スタンプとして扱う。
        definition_->animationStateName = "DragonGroundStamp";
        definition_->duration = 1.8f;
        definition_->recoveryTime = 0.45f;
        definition_->hitBoxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::TAKE_OFF))
    {
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::AIR_FIRE_BALL))
    {
        definition_->recoveryTime = 0.0f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH))
    {
        definition_->recoveryTime = 0.0f;
        definition_->hitBoxActionName.clear();
        definition_->vfxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::LANDING_ATTACK))
    {
        definition_->recoveryTime = 0.35f;
        definition_->hitBoxActionName.clear();
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::AIR_HOLD))
    {
        definition_->animationStateName = "PlainsDragon_Fly";
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::MANUAL;
        definition_->waitForActionEnd = false;
        definition_->useActionTimeout = false;
        definition_->cooldownTime = 0.0f;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::AIR_GET_HIT))
    {
        definition_->cooldownTime = 0.5f;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::AIR_DEATH_FALL_START))
    {
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
        definition_->useActionTimeout = true;
        definition_->cooldownTime = 0.0f;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::FALLING))
    {
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::AI_CONDITION;
        definition_->waitForActionEnd = false;
        definition_->useActionTimeout = false;
        definition_->cooldownTime = 0.0f;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::DEATH_HIT_GROUND))
    {
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
        definition_->useActionTimeout = true;
        definition_->cooldownTime = 0.0f;
        definition_->recoveryTime = 0.0f;
    }
    if (DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::ULTIMATE_ATTACK))
    {
        // HPしきい値で必ず見せたい大技は、演出シーケンサー主導にしつつタイムアウトで停止を防ぐ。
        definition_->finishMode = DRAGON_ACTION_FINISH_MODE::SEQUENCER;
        definition_->waitForActionEnd = true;
        definition_->useActionTimeout = true;
        definition_->actionTimeout = (std::max)(definition_->actionTimeout, definition_->duration + 1.0f);
        definition_->cooldownTime = 0.0f;
        definition_->selectionWeight = 0.0f;
        definition_->hitBoxActionName.clear();
    }
}

void DragonAIComponent::UpdateContext()
{
    // プレイヤー位置、距離、角度、HP段階、使用可能アクションをまとめて現在の判断材料にする。
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
    // 自分の前方ベクトルとプレイヤー方向から、正面/側面/背面の判定に使う角度を求める。
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
    // クールダウンや有効フラグ込みの使用可否をContextへ保存し、Graph/重み選択の共通入力にする。
    context.canUseFireBall = IsActionAvailable(DRAGON_ACTION_TYPE::FIRE_BALL);
    context.canUseSpreadBreath = IsActionAvailable(DRAGON_ACTION_TYPE::SPREAD_BREATH);
    context.canUseTailSweep =
        IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP) &&
        CanUseTurnTailSweep(context);
    context.canUseTakeOff = IsActionAvailable(DRAGON_ACTION_TYPE::TAKE_OFF);
    context.canUseRunCharge = IsActionAvailable(DRAGON_ACTION_TYPE::RUN_CHARGE);
    context.canUseSpinSweep =
        IsActionAvailable(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP) &&
        CanUseSpinTailSweep(context);
    context.canUseGroundStamp = IsActionAvailable(DRAGON_ACTION_TYPE::GROUND_STAMP);
}

void DragonAIComponent::UpdateUltimateTrigger()
{
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateUltimateTrigger");

    if (aiState == DRAGON_AI_STATE::DEAD ||
        !health ||
        health->IsDead())
    {
        ultimateDamageReductionActive = false;
        return;
    }

    const float currentHpRate_ = context.hpRate;
    if (!hasPreviousHpRateForUltimate)
    {
        hasPreviousHpRateForUltimate = true;
        previousHpRateForUltimate = currentHpRate_;
        return;
    }

    const float firstThreshold_ = std::clamp(firstUltimateHpRate, 0.0f, 1.0f);
    const float secondThreshold_ = std::clamp(secondUltimateHpRate, 0.0f, 1.0f);
    const bool crossedFirstThreshold_ =
        !firstUltimateTriggered &&
        previousHpRateForUltimate > firstThreshold_ &&
        currentHpRate_ <= firstThreshold_;
    const bool crossedSecondThreshold_ =
        !secondUltimateTriggered &&
        previousHpRateForUltimate > secondThreshold_ &&
        currentHpRate_ <= secondThreshold_;

    previousHpRateForUltimate = currentHpRate_;

    if (crossedFirstThreshold_)
    {
        firstUltimateTriggered = true;
        pendingUltimateAction = true;
    }
    if (crossedSecondThreshold_)
    {
        secondUltimateTriggered = true;
        pendingUltimateAction = true;
    }

    ultimateDamageReductionActive =
        pendingUltimateAction ||
        runtime.currentAction == DRAGON_ACTION_TYPE::ULTIMATE_ATTACK;

    if (!pendingUltimateAction ||
        runtime.currentAction == DRAGON_ACTION_TYPE::ULTIMATE_ATTACK ||
        IsDeathAction(runtime.currentAction))
    {
        return;
    }

    pendingUltimateAction = false;
    ultimateDamageReductionActive = true;
    ForceAction(DRAGON_ACTION_TYPE::ULTIMATE_ATTACK);
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
    // 重み付きフォールバックAIは一定間隔ごとに候補を作り、地上/空中の状態に応じて行動を選ぶ。
    const bool canDecideGroundAction_ =
        autoDecisionEnabled &&
        aiState == DRAGON_AI_STATE::IDLE;
    const bool canDecideAirAction_ =
        autoDecisionEnabled &&
        aiState == DRAGON_AI_STATE::AIR &&
        runtime.currentAction == DRAGON_ACTION_TYPE::AIR_HOLD &&
        runtime.actionEnded &&
        (pendingAirAttackDecision || pendingAirLandingDecision);

    if ((!canDecideGroundAction_ && !canDecideAirAction_) ||
        ShouldWaitForIdleAnimation())
    {
        return;
    }

    decisionTimer -= deltaTime_;
    if (decisionTimer > 0.0f)
    {
        return;
    }

    decisionTimer = decisionInterval;
    RequestAction(canDecideAirAction_
        ? SelectNextAirAction()
        : SelectNextAction(context));
}

void DragonAIComponent::UpdateBehaviorGraph(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateBehaviorGraph");
    // BehaviorGraphは一定間隔でTickし、Blackboard条件からPLAY_ACTIONノードを通じて行動を要求する。
    if (!autoDecisionEnabled ||
        aiState != DRAGON_AI_STATE::IDLE ||
        ShouldWaitForIdleAnimation())
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
    // Tickへ渡すContextはGameObject参照とBlackboard、距離情報をまとめたグラフ実行用の入力。
    behaviorContext.target = playerObject.lock().get();
    behaviorContext.blackboard = blackboard;
    behaviorContext.globalBlackboard = nullptr;
    behaviorContext.ownerPosition = context.ownerPosition;
    behaviorContext.targetPosition = context.playerPosition;
    behaviorContext.toTargetDirection = context.toPlayer;
    behaviorContext.targetDistance = context.playerDistance;
    behaviorContext.deltaTime = deltaTime_;
    behaviorContext.hasTarget = behaviorContext.target != nullptr;

    // 実行中のGraph継続には補正を掛けず、新しい地上行動の選択時だけ候補を調べる。
    behaviorContext.actionWeightModifier = {};
    behaviorContext.actionWeightObserver = {};
    if (aiState == DRAGON_AI_STATE::IDLE)
    {
        std::vector<DRAGON_ACTION_TYPE> actions_;
        for (const std::string& actionName_ : behaviorGraph.GetSelectableActions(behaviorContext))
        {
            for (const DragonActionDefinition& definition_ : actionDefinitions)
                if (actionName_ == "DragonAI_" + definition_.actionName && IsActionAvailable(definition_.action) && definition_.selectionWeight > 0.0f)
                    actions_.push_back(definition_.action);
        }
        PrepareHistorySelection(actions_);
        behaviorContext.actionWeightModifier = [this](const std::string& actionName_)
        {
            return ResolveGraphActionModifier(actionName_);
        };
        behaviorContext.actionWeightObserver = [this](const std::string& actionName_, float branchWeight_, float modifier_)
        {
            for (const DragonActionDefinition& definition_ : actionDefinitions)
            {
                if (actionName_ != "DragonAI_" + definition_.actionName) continue;
                DragonActionSelectionDebug& debug_ = selectionDebugWeights[ToIndex(definition_.action)];
                debug_.baseWeight = (std::max)(0.0f, branchWeight_) * GetSelectionWeight(definition_.action);
                debug_.historyModifier = CalculateHistoryModifier(definition_.action);
                debug_.finalWeight = (std::max)(0.0f, branchWeight_) * modifier_;
            }
        };
    }

    lastBehaviorStatus = behaviorGraph.Tick(behaviorContext);
    ++behaviorGraphTickCount;
}

DRAGON_ACTION_TYPE DragonAIComponent::SelectNextAirAction()
{
    BS_PROFILE_SCOPE("DragonAIComponent::SelectNextAirAction");

    if (pendingAirAttackDecision && !IsAirHoverHeightReached())
    {
        return DRAGON_ACTION_TYPE::AIR_HOLD;
    }

    if (pendingAirLandingDecision)
    {
        if (!IsReadyToPlayLandingAnimation())
        {
            return DRAGON_ACTION_TYPE::AIR_HOLD;
        }
        return DRAGON_ACTION_TYPE::LANDING_ATTACK;
    }

    if (pendingAirAttackDecision)
    {
        if (airAttackCount >= (std::max)(maxAirAttackCount, 0))
        {
            pendingAirAttackDecision = false;
            pendingAirLandingDecision = true;
            return DRAGON_ACTION_TYPE::AIR_HOLD;
        }
        // 高度・攻撃回数・距離条件を通過した空中攻撃にも履歴補正を適用する。
        std::vector<DragonActionCandidate> candidates_;
        if (context.playerDistance >= farDistance)
            AddWeightedActionCandidate(context, candidates_, DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH, 1.0f);
        AddWeightedActionCandidate(context, candidates_, DRAGON_ACTION_TYPE::AIR_FIRE_BALL, 1.0f);
        const DRAGON_ACTION_TYPE action_ = SelectHistoryWeightedAction(candidates_);
        if (action_ != DRAGON_ACTION_TYPE::IDLE) return action_;
    }

    return DRAGON_ACTION_TYPE::LANDING_ATTACK;
}

bool DragonAIComponent::IsAirHoverHeightReached()
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsAirHoverHeightReached");

    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return true;

    const float targetHeight_ = (std::max)(airHoverHeight, 0.0f);
    const float tolerance_ = (std::max)(airHoverHeightTolerance, 0.0f);
    return owner_->transform->localPosition.y >= targetHeight_ - tolerance_;
}

bool DragonAIComponent::ShouldWaitForIdleAnimation() const
{
    BS_PROFILE_SCOPE("DragonAIComponent::ShouldWaitForIdleAnimation");

    if (!animationGraphDriven ||
        runtime.currentAction != DRAGON_ACTION_TYPE::IDLE ||
        !runtime.actionEnded)
    {
        return false;
    }

    const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock();
    if (!animationGraph_) return false;

    const AnimState* currentState_ = animationGraph_->GetActiveState();
    if (!currentState_) return false;

    return currentState_->name != "PlainsDragon_IdleBreathe";
}

bool DragonAIComponent::UpdateIdleAnimationGraphReturn()
{
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateIdleAnimationGraphReturn");

    if (!ShouldWaitForIdleAnimation())
    {
        return false;
    }

    const DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::IDLE);
    if (definition_)
    {
        ApplyAnimationBlackboard("Idle", definition_->animationStateName, false);
    }

    RequestIdleGraphTransition();

    statusText = "AI requested graph return to Idle.";
    return true;
}

void DragonAIComponent::UpdateActionMotion(float deltaTime_)
{
    switch (runtime.currentAction)
    {
    case DRAGON_ACTION_TYPE::WALK_APPROACH:
        if (!CanUseGroundApproach(context))
        {
            StopMovement();
            break;
        }
        MoveGround(walkSpeed, groundMoveTurnSpeed, deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::RUN_CHARGE:
        if (!CanUseGroundApproach(context))
        {
            StopMovement();
            break;
        }
        MoveGround(runSpeed, groundMoveTurnSpeed * 0.6f, deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::TURN_TO_PLAYER:
        StopMovement();
        break;
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        StopMovement();
        UpdateSpinTailSweepRotation(deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::TAKE_OFF:
        MoveForward(-takeOffBackSpeed, deltaTime_);
        UpdateAirbornePhysics(deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::AIR_HOLD:
        if (pendingAirLandingDecision)
        {
            UpdateLandingDescend(deltaTime_);
            break;
        }
        UpdateAirbornePhysics(deltaTime_);
        MoveAir(airForwardSpeed, airTurnSpeed, deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
    case DRAGON_ACTION_TYPE::GROUND_STAMP:
    case DRAGON_ACTION_TYPE::BITE:
    case DRAGON_ACTION_TYPE::FIRE_BALL:
    case DRAGON_ACTION_TYPE::FIRE_BALL_FRONT:
    case DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP:
    case DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD:
    case DRAGON_ACTION_TYPE::ULTIMATE_ATTACK:
        StopMovement();
        break;
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
        UpdateAirbornePhysics(deltaTime_);
        MoveAir(-airAttackBackSpeed, airTurnSpeed * 0.5f, deltaTime_);
        break;
    case DRAGON_ACTION_TYPE::AIR_GET_HIT:
        StopMovement();
        break;
    default:
        StopMovement();
        break;
    }
}

bool DragonAIComponent::UpdateAIControlledActionEnd()
{
    if (skillActionRunning) return false;
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateAIControlledActionEnd");

    if (!IsAIControlledAction(runtime.currentAction))
    {
        return false;
    }

    switch (runtime.currentAction)
    {
    case DRAGON_ACTION_TYPE::WALK_APPROACH:
    {
        if (!CanUseGroundApproach(context))
        {
            StopMovement();
            statusText = !context.isPlayerInFront
                ? "AI finished WalkApproach: turn required."
                : "AI finished WalkApproach.";
            NotifyAIActionEnd();
            return true;
        }
        break;
    }
    case DRAGON_ACTION_TYPE::RUN_CHARGE:
    {
        const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
        if (!definition_)
        {
            return false;
        }
        const float moveEndDistance_ = (std::max)(approachEndDistance, biteDistance + 0.75f);
        if (!CanUseGroundApproach(context) ||
            runtime.actionTimer >= definition_->duration)
        {
            StopMovement();
            statusText = !context.isPlayerInFront
                ? "AI finished RunCharge: turn required."
                : context.playerHorizontalDistance <= moveEndDistance_
                ? "AI finished RunCharge: target reached."
                : "AI finished RunCharge: charge slice ended.";
            NotifyAIActionEnd();
            return true;
        }
        break;
    }
    case DRAGON_ACTION_TYPE::FALLING:
        if (IsGrounded())
        {
            StopMovement();
            statusText = "AI finished Falling: ground reached.";
            NotifyAIActionEnd();
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

bool DragonAIComponent::UpdateDurationControlledActionEnd()
{
    if (skillActionRunning) return false;
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateDurationControlledActionEnd");

    const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
    if (!definition_ ||
        definition_->finishMode != DRAGON_ACTION_FINISH_MODE::AI_DURATION ||
        runtime.actionTimer < definition_->duration)
    {
        return false;
    }

    statusText = "AI duration finished: DragonAI_" + definition_->actionName;
    NotifyAIActionEnd();
    return true;
}

void DragonAIComponent::FaceTargetFromAnimationEvent()
{
    BS_PROFILE_SCOPE("DragonAIComponent::FaceTargetFromAnimationEvent");

    statusText = "Ignored FaceTarget: body facing is animation root motion only.";
    SendRuntimeToBlackboard();
}

void DragonAIComponent::BeginTurnRootMotionLimit(DRAGON_ACTION_TYPE action_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::BeginTurnRootMotionLimit");

    ClearTurnRootMotionLimit();
    if (!limitTurnRootMotion ||
        !UsesTurnRootMotionLimit(action_) ||
        !hasPlayerTarget)
    {
        return;
    }

    const float targetAngleDegrees_ = NormalizeAngle(context.playerAngleDegrees);
    if (std::abs(targetAngleDegrees_) <= 0.001f)
    {
        return;
    }

    const float paddingDegrees_ = (std::max)(turnRootMotionLimitPaddingDegrees, 0.0f);
    const float signedPaddingDegrees_ = targetAngleDegrees_ >= 0.0f
        ? paddingDegrees_
        : -paddingDegrees_;
    turnRootMotionLimitRemainingDegrees = targetAngleDegrees_ + signedPaddingDegrees_;
    turnRootMotionLimitActive = true;

    if (const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock())
    {
        // 90度ターンRootMotionが目標角度を超えて回らないよう、消費できるYaw量を渡す。
        animationGraph_->RequestYawLimit(turnRootMotionLimitRemainingDegrees);
    }
}

void DragonAIComponent::ClearTurnRootMotionLimit()
{
    if (const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock())
    {
        animationGraph_->ClearYawLimit();
    }
    turnRootMotionLimitRemainingDegrees = 0.0f;
    turnRootMotionLimitActive = false;
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

void DragonAIComponent::TurnTowardPlayerLimited(float turnSpeed_, float deltaTime_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform || !hasPlayerTarget) return;

    const float maxTurnDegrees_ = (std::max)(turnSpeed_, 0.0f) * deltaTime_;
    const float angleDelta_ = std::clamp(
        context.playerAngleDegrees,
        -maxTurnDegrees_,
        maxTurnDegrees_);
    owner_->transform->localRotation.y =
        NormalizeAngle(owner_->transform->localRotation.y + angleDelta_);
}

void DragonAIComponent::MoveGround(float speed_, float turnSpeed_, float deltaTime_)
{
    TurnTowardPlayerLimited(turnSpeed_, deltaTime_);
    MoveForward(speed_, deltaTime_);
}

void DragonAIComponent::MoveAir(float speed_, float turnSpeed_, float deltaTime_)
{
    TurnTowardPlayerLimited(turnSpeed_, deltaTime_);
    MoveForward(speed_, deltaTime_);
}

void DragonAIComponent::UpdateAirbornePhysics(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateAirbornePhysics");

    SetAirbornePhysicsEnabled(true);

    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const float currentY_ = owner_->transform->localPosition.y;
    const float maxStep_ = (std::max)(airHoverAdjustSpeed, 0.0f) * deltaTime_;
    const float deltaY_ = std::clamp(
        airHoverHeight - currentY_,
        -maxStep_,
        maxStep_);
    owner_->transform->AddMoveOffset({ 0.0f, deltaY_, 0.0f });
}

void DragonAIComponent::UpdateLandingDescend(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateLandingDescend");

    SetAirbornePhysicsEnabled(true);
    StopMovement();

    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const float currentY_ = owner_->transform->localPosition.y;
    const float targetY_ = (std::max)(airLandingAnimationStartHeight, 0.0f);
    const float maxStep_ = (std::max)(airLandingDescendSpeed, 0.0f) * deltaTime_;
    const float deltaY_ = std::clamp(
        targetY_ - currentY_,
        -maxStep_,
        maxStep_);
    owner_->transform->AddMoveOffset({ 0.0f, deltaY_, 0.0f });
}

void DragonAIComponent::SetAirbornePhysicsEnabled(bool enabled_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::SetAirbornePhysicsEnabled");

    const std::shared_ptr<CharacterMovementComponent> movement_ = movement.lock();
    std::shared_ptr<ColliderComponent> collider_;
    if (movement_)
    {
        collider_ = movement_->ResolveCollider();
    }
    if (!collider_)
    {
        const auto owner_ = GetGameObject();
        if (owner_)
        {
            collider_ = owner_->GetComponent<ColliderComponent>();
        }
    }
    if (enabled_)
    {
        airbornePhysicsApplied = true;
        if (collider_)
        {
            collider_->useGravity = false;
            collider_->velocity.y = 0.0f;
        }
        if (movement_)
        {
            movement_->ResetVerticalVelocity();
            movement_->SetVerticalMovementLocked(true);
        }
        return;
    }

    airbornePhysicsApplied = false;
    if (collider_)
    {
        collider_->useGravity = true;
        if (collider_->velocity.y > 0.0f)
        {
            collider_->velocity.y = 0.0f;
        }
    }
    if (movement_)
    {
        movement_->SetVerticalMovementLocked(false);
        movement_->ResetVerticalVelocity();
    }
}

bool DragonAIComponent::IsGrounded()
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsGrounded");

    const std::shared_ptr<CharacterMovementComponent> movement_ = movement.lock();
    std::shared_ptr<ColliderComponent> collider_;
    if (movement_)
    {
        collider_ = movement_->ResolveCollider();
    }
    if (!collider_)
    {
        const auto owner_ = GetGameObject();
        if (owner_)
        {
            collider_ = owner_->GetComponent<ColliderComponent>();
        }
    }

    return !collider_ || collider_->isGrounded;
}

bool DragonAIComponent::IsReadyToPlayLandingAnimation()
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsReadyToPlayLandingAnimation");

    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return true;

    return owner_->transform->localPosition.y <=
        (std::max)(airLandingAnimationStartHeight, 0.0f) + 0.001f;
}

void DragonAIComponent::StopMovement()
{
    if (const std::shared_ptr<CharacterMovementComponent> movement_ = movement.lock())
    {
        movement_->StopHorizontalMovement();
    }
}

void DragonAIComponent::BeginSpinTailSweepRotation()
{
    BS_PROFILE_SCOPE("DragonAIComponent::BeginSpinTailSweepRotation");

    spinTailRotationDirection = context.playerAngleDegrees >= 0.0f ? 1.0f : -1.0f;
    spinTailRotationAppliedYaw = 0.0f;
}

void DragonAIComponent::UpdateSpinTailSweepRotation(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::UpdateSpinTailSweepRotation");

    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        return;
    }

    const DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP);
    const float duration_ = definition_ ? (std::max)(definition_->duration, 0.001f) : 1.0f;
    const float afterRate_ = std::clamp((runtime.actionTimer + deltaTime_) / duration_, 0.0f, 1.0f);
    constexpr float WINDUP_END_RATE = 0.18f;
    constexpr float BURST_END_RATE = 0.72f;
    constexpr float WINDUP_YAW_RATE = 0.04f;
    constexpr float BURST_YAW_RATE = 0.92f;
    float rotationRate_ = 0.0f;
    if (afterRate_ < WINDUP_END_RATE)
    {
        const float windupRate_ = afterRate_ / WINDUP_END_RATE;
        rotationRate_ = WINDUP_YAW_RATE * windupRate_ * windupRate_;
    }
    else if (afterRate_ < BURST_END_RATE)
    {
        const float burstRate_ = (afterRate_ - WINDUP_END_RATE) / (BURST_END_RATE - WINDUP_END_RATE);
        const float burstRemainRate_ = 1.0f - burstRate_;
        rotationRate_ = WINDUP_YAW_RATE +
            (BURST_YAW_RATE - WINDUP_YAW_RATE) *
            (1.0f - burstRemainRate_ * burstRemainRate_ * burstRemainRate_);
    }
    else
    {
        const float settleRate_ = (afterRate_ - BURST_END_RATE) / (1.0f - BURST_END_RATE);
        const float settleSmoothRate_ = settleRate_ * settleRate_ * (3.0f - 2.0f * settleRate_);
        rotationRate_ = BURST_YAW_RATE + (1.0f - BURST_YAW_RATE) * settleSmoothRate_;
    }
    const float targetAppliedYaw_ = spinTailRotationDegrees * spinTailRotationDirection * rotationRate_;
    const float yawDelta_ = targetAppliedYaw_ - spinTailRotationAppliedYaw;

    // 溜めから一気に回るカーブにして、尻尾回転攻撃の打撃感を出す。
    owner_->transform->localRotation.y =
        NormalizeAngle(owner_->transform->localRotation.y + yawDelta_);
    spinTailRotationAppliedYaw = targetAppliedYaw_;
}

void DragonAIComponent::StartAction(DRAGON_ACTION_TYPE action_)
{
    // 被弾・死亡・キャンセルでも古い共通Phaseの予約を残さない。
    skillExecution.Stop();
    skillActionRunning = false;
    // 実際のアクション開始地点。ランタイム状態、クールダウン、アニメーション、IK、外部Actionをまとめて更新する。
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_) return;

    lastStartedAction = action_;
    ++actionStartCount;
    pendingSequencerActionEnd = false;
    DisableSequencerHitBoxes();
    ClearTailIK();
    runtime.currentAction = action_;
    runtime.bufferedAction = DRAGON_ACTION_TYPE::IDLE;
    runtime.actionTimer = 0.0f;
    runtime.recoveryTimer = 0.0f;
    runtime.actionStarted = true;
    runtime.actionEnded = false;
    runtime.canCancel = false;
    runtime.canBufferAction = definition_->allowBuffer;
    waitingForLandingGround = false;
    statusText.clear();

    const bool usesAirbornePhysics_ =
        IsAirborneAction(action_) &&
        action_ != DRAGON_ACTION_TYPE::LANDING_ATTACK;
    // 空中系アクションでは移動コンポーネント側の重力/接地処理を切り替える。
    SetAirbornePhysicsEnabled(usesAirbornePhysics_);

    if (action_ == DRAGON_ACTION_TYPE::AIR_FIRE_BALL ||
        action_ == DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH)
    {
        pendingAirAttackDecision = false;
    }
    if (action_ == DRAGON_ACTION_TYPE::LANDING_ATTACK)
    {
        pendingAirLandingDecision = false;
    }
    if (action_ == DRAGON_ACTION_TYPE::ULTIMATE_ATTACK)
    {
        pendingUltimateAction = false;
        ultimateDamageReductionActive = true;
        pendingAirAttackDecision = false;
        pendingAirLandingDecision = false;
    }
    if (action_ == DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP)
    {
        // RMなしのTurn90を使うため、一周分の回転は毎フレームのAI更新で補間する。
        BeginSpinTailSweepRotation();
    }

    aiState = DRAGON_AI_STATE::ACTION;
    if (action_ == DRAGON_ACTION_TYPE::AIR_HOLD ||
        action_ == DRAGON_ACTION_TYPE::AIR_FIRE_BALL ||
        action_ == DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH ||
        action_ == DRAGON_ACTION_TYPE::AIR_GET_HIT)
    {
        aiState = DRAGON_AI_STATE::AIR;
    }

    BeginTurnRootMotionLimit(action_);
    const int startCount_ = actionStartCount;
    ExecuteActionDefinition(*definition_);
    if (actionStartCount != startCount_) return;
    if (action_ == DRAGON_ACTION_TYPE::AIR_HOLD && !skillActionRunning)
    {
        // AIR_HOLDは空中待機状態なので、開始直後に完了扱いにして次の空中判断を受けられるようにする。
        runtime.actionEnded = true;
        runtime.canCancel = true;
        runtime.canBufferAction = true;
        RequestAirIdleGraphTransition();
    }
    RecordActionHistory(action_);
    SetCooldown(action_, definition_->cooldownTime *
        (UsesActionHistory(action_) ? (std::max)(0.0f, attackCooldownMultiplier) : 1.0f));
    SendRuntimeToBlackboard();
}

void DragonAIComponent::EnterRecovery()
{
    aiState = DRAGON_AI_STATE::RECOVERY;
}

void DragonAIComponent::FinishRecovery()
{
    // 硬直終了後は通常Idleへ戻し、空中フラグや保留中の空中判断をクリアする。
    runtime.currentAction = DRAGON_ACTION_TYPE::IDLE;
    runtime.actionTimer = 0.0f;
    runtime.recoveryTimer = 0.0f;
    runtime.actionStarted = false;
    runtime.actionEnded = true;
    runtime.canCancel = true;
    runtime.canBufferAction = false;
    ClearTurnRootMotionLimit();
    DisableSequencerHitBoxes();
    ClearTailIK();
    aiState = DRAGON_AI_STATE::IDLE;
    pendingAirAttackDecision = false;
    pendingAirLandingDecision = false;
    pendingUltimateAction = false;
    ultimateDamageReductionActive = false;
    airAttackCount = 0;
    waitingForLandingGround = false;
    SetAirbornePhysicsEnabled(false);
    StopMovement();
    if (const DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::IDLE))
    {
        ApplyAnimationBlackboard("Idle", definition_->animationStateName, false);
    }
    RequestIdleGraphTransition();
}

void DragonAIComponent::UpdateActionTimeout()
{
    if (skillActionRunning) return;
    // SequencerやAI条件から完了が返らない時の保険として、定義済みタイムアウトで強制完了する。
    if (runtime.actionEnded)
    {
        return;
    }

    const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
    if (!definition_ ||
        !definition_->useActionTimeout ||
        definition_->actionTimeout <= 0.0f ||
        runtime.actionTimer < definition_->actionTimeout)
    {
        return;
    }

    statusText = "Action timeout fallback: DragonAI_" + definition_->actionName;
    CompleteActionEnd();
}

void DragonAIComponent::ExecuteActionDefinition(const DragonActionDefinition& definition_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::ExecuteActionDefinition");
    // 定義に紐づくアニメーション、Blackboard、オーナーAction、IKを実行開始時にまとめて発火する。
    const std::string animationActionName_ = ResolveAnimationActionName(definition_);
    const std::string animationStateName_ = ResolveAnimationStateName(definition_);
    ApplyAnimationBlackboard(animationActionName_, animationStateName_, true);
    if (!definition_.skillName.empty())
    {
        const auto iterator_ = std::find_if(skillDefinitions.begin(), skillDefinitions.end(),
            [&definition_](const SkillDefinition& skill_) { return skill_.name == definition_.skillName; });
        if (iterator_ == skillDefinitions.end())
        {
            statusText = "Skill not found: " + definition_.skillName;
            pendingSequencerActionEnd = true;
            return;
        }
        // AI判断・Cooldown・RecoveryはDragon側、複数Animationの進行は共通実行側に置く。
        skillActionRunning = true;
        const bool started_ = skillExecution.Start(GetGameObject(), *iterator_, [this](const SkillPhase& phase_)
        {
            runtime.canCancel = phase_.canActionBranch;
            runtime.canBufferAction = phase_.canBufferInput;
            SendRuntimeToBlackboard();
        });
        if (!started_)
        {
            skillActionRunning = false;
            statusText = skillExecution.GetError();
            pendingSequencerActionEnd = true;
        }
        return;
    }
    PlayAnimationState(animationStateName_);
    ExecuteDirectIK(definition_.action, definition_.duration);
    ExecuteOwnerAction(definition_.actionSequenceName);
    ExecuteOwnerAction(definition_.animationActionName);
    ExecuteOwnerAction(definition_.ikActionName);
    ExecuteOwnerAction(definition_.hitBoxActionName);
    ExecuteOwnerAction(definition_.vfxActionName);
}

std::string DragonAIComponent::ResolveAnimationActionName(
    const DragonActionDefinition& definition_) const
{
    // 左右差分のある行動はプレイヤー角度からアニメーション用アクション名を選ぶ。
    switch (definition_.action)
    {
    case DRAGON_ACTION_TYPE::TURN_TO_PLAYER:
        return context.playerAngleDegrees > 0.0f ? "TurnToPlayerR" : "TurnToPlayerL";
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
        return context.playerAngleDegrees > 0.0f ? "TailSweepR" : "TailSweepL";
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        return context.playerAngleDegrees > 0.0f ? "SpinTailSweepR" : "SpinTailSweepL";
    default:
        break;
    }

    if (!definition_.animationActionName.empty())
    {
        return definition_.animationActionName;
    }
    return definition_.actionName;
}

std::string DragonAIComponent::ResolveAnimationStateName(
    const DragonActionDefinition& definition_) const
{
    // 左右差分のある旋回系は、行動定義ではなく現在角度から再生ステートを決める。
    switch (definition_.action)
    {
    case DRAGON_ACTION_TYPE::TURN_TO_PLAYER:
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        return context.playerAngleDegrees > 0.0f
            ? "PlainsDragon_Turn90R_RM"
            : "PlainsDragon_Turn90L_RM";
    default:
        break;
    }

    return definition_.animationStateName;
}

void DragonAIComponent::ApplyAnimationBlackboard(
    const std::string& animationActionName_,
    const std::string& animationStateName_,
    bool actionTrigger_)
{
    // AnimationController側の遷移条件として使う値をBlackboardへ書き込み、AIとアニメーションを同期する。
    const bool isActing_ =
        (aiState == DRAGON_AI_STATE::ACTION ||
            aiState == DRAGON_AI_STATE::AIR) &&
        !runtime.actionEnded;
    const bool returnToIdle_ =
        runtime.actionEnded &&
        runtime.currentAction == DRAGON_ACTION_TYPE::IDLE;
    const bool returnToAirIdle_ =
        runtime.actionEnded &&
        runtime.currentAction == DRAGON_ACTION_TYPE::AIR_HOLD;

    if (blackboard)
    {
        // BehaviorGraphや外部デバッグからも読めるよう、同じ値をBlackboardへミラーする。
        blackboard->SetString(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::CURRENT_ACTION, animationActionName_);
        blackboard->SetString(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::ACTION_TRIGGER, actionTrigger_ ? animationActionName_ : "");
        blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::ACTION_ENDED, runtime.actionEnded);
        blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::RETURN_TO_IDLE, returnToIdle_);
        blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::RETURN_TO_AIR_IDLE, returnToAirIdle_);
        blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::IS_ACTING, isActing_);
        blackboard->SetString(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::DESIRED_STATE, animationStateName_);
    }

    // 従来のAnimカテゴリへの接続を維持し、Controllerへ直接設定を渡さない。
    if (const auto animationGraph_ = animationGraph.lock())
        animationGraph_->RequestInputCategory(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY);

}

void DragonAIComponent::PlayAnimationState(const std::string& stateName_)
{
    if (stateName_.empty()) return;

    // 遷移グラフを使う場合はステート名からTo_トリガーを作り、他トリガーをリセットしてから発火する。
    if (const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock())
    {
        const std::string triggerName_ = "To_" + stateName_;
        ResetAnimationActionTriggers(triggerName_);
        animationGraph_->RequestTrigger(triggerName_);
    }
}

void DragonAIComponent::PlaySequencerLinkedState(const std::string& stateName_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::PlaySequencerLinkedState");

    if (stateName_.empty()) return;

    const DragonActionDefinition* definition_ = FindDefinition(runtime.currentAction);
    const std::string animationActionName_ = definition_
        ? ResolveAnimationActionName(*definition_)
        : GetDragonActionName(runtime.currentAction);

    // 連続攻撃の途中遷移ではAI行動を完了させず、Animation側の現在ステートだけを次段へ同期する。
    ApplyAnimationBlackboard(animationActionName_, stateName_, true);

    if (const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock())
    {
        ResetAnimationActionTriggers("");
        animationGraph_->RequestStateTransition(stateName_, 0.079999998f);
    }
}

void DragonAIComponent::RequestIdleGraphTransition()
{
    BS_PROFILE_SCOPE("DragonAIComponent::RequestIdleGraphTransition");

    // 地上Idleへ戻る時はReturnToIdleと専用トリガーを立て、待機アニメーションへ確実に戻す。
    if (const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock())
    {
        const std::string triggerName_ = "To_PlainsDragon_IdleBreathe";
        ResetAnimationActionTriggers(triggerName_);
        if (blackboard) blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::RETURN_TO_IDLE, true);
        if (blackboard) blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::RETURN_TO_AIR_IDLE, false);
        animationGraph_->RequestTrigger(triggerName_);
        animationGraph_->RequestEvaluation();
    }
}

void DragonAIComponent::RequestAirIdleGraphTransition()
{
    BS_PROFILE_SCOPE("DragonAIComponent::RequestAirIdleGraphTransition");

    // 空中待機へ戻る時はReturnToAirIdleを立て、遷移評価まで行って即座にFly系へ寄せる。
    if (const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock())
    {
        const std::string triggerName_ = "To_PlainsDragon_Fly";
        ResetAnimationActionTriggers(triggerName_);
        if (blackboard) blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::RETURN_TO_IDLE, false);
        if (blackboard) blackboard->SetBool(AnimationGraphBlackboardKeys::ANIMATION_CATEGORY, AnimationGraphBlackboardKeys::RETURN_TO_AIR_IDLE, true);
        animationGraph_->RequestTrigger(triggerName_);
        animationGraph_->RequestEvaluation();
    }
}

void DragonAIComponent::ResetAnimationActionTriggers(const std::string& keepTriggerName_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::ResetAnimationActionTriggers");

    // 同時に複数のTo_トリガーが残ると意図しない遷移になるため、使う1つ以外を落とす。
    const std::shared_ptr<AnimationGraphBridgeComponent> animationGraph_ =
        animationGraph.lock();
    if (!animationGraph_) return;

    const char* graphStateNames_[] =
    {
        "PlainsDragon_IdleBreathe",
        "PlainsDragon_Walk",
        "PlainsDragon_Run",
        "PlainsDragon_Turn90L_RM",
        "PlainsDragon_Turn90R_RM",
        "PlainsDragon_BiteAttack",
        "DragonGroundStamp",
        "PlainsDragon_SpitFireBall",
        "PlainsDragon_SpitFireBall_Sweep",
        "PlainsDragon_SpitFireBall_Front",
        "PlainsDragon_SpitFireBall_Triple",
        "PlainsDragon_SpitFireBall_Triple2",
        "PlainsDragon_SpitFireBall_Triple3",
        "PlainsDragon_FireBeam_Sweep45",
        "PlainsDragon_SpreadFire",
        "PlainsDragon_SpreadFire_Forward",
        "PlainsDragon_TakeOff",
        "PlainsDragon_Fly",
        "PlainsDragon_FlyStationary",
        "PlainsDragon_FlyStationarySpitFireBall",
        "PlainsDragon_FlyStationarySpreadFire",
        "PlainsDragon_FlyStationaryToLanding",
        "PlainsDragon_GetHit1",
        "PlainsDragon_FlyStationaryGetHit",
        "PlainsDragon_FlyStationaryGetHitToFalling",
        "PlainsDragon_Falling",
        "PlainsDragon_DeathHitTheGround",
        "PlainsDragon_Death"
    };

    for (const char* graphStateName_ : graphStateNames_)
    {
        const std::string triggerName_ = std::string("To_") + graphStateName_;
        if (triggerName_ == keepTriggerName_)
        {
            continue;
        }

        animationGraph_->ClearTrigger(triggerName_);
    }
}

void DragonAIComponent::ExecuteDirectIK(DRAGON_ACTION_TYPE action_, float duration_)
{
    // 特定攻撃では首や尻尾IKを直接駆動し、アニメーションだけでは足りない狙い方向を補正する。
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
    case DRAGON_ACTION_TYPE::FIRE_BALL_FRONT:
        if (neckIK_) neckIK_->LookAtYawOffset(0.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE:
        if (neckIK_) neckIK_->SweepYaw(-45.0f, 45.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP:
        if (neckIK_) neckIK_->SweepYaw(-30.0f, 30.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP:
        if (neckIK_) neckIK_->SweepYaw(-45.0f, 45.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
        if (neckIK_) neckIK_->SweepYaw(-30.0f, 30.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD:
        if (neckIK_) neckIK_->LookAtYawOffset(0.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
        if (neckIK_) neckIK_->SweepYaw(-35.0f, 35.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::ULTIMATE_ATTACK:
        if (neckIK_) neckIK_->SweepYaw(-45.0f, 45.0f, duration_);
        break;
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
        if (tailIK_)
        {
            if (context.playerAngleDegrees > 0.0f)
            {
                tailIK_->StartSweepRight(duration_);
            }
            else
            {
                tailIK_->StartSweepLeft(duration_);
            }
        }
        break;
    default:
        break;
    }
}

void DragonAIComponent::ExecuteOwnerAction(const std::string& actionName_)
{
    // ヒットボックスやVFXなど、GameObjectへ登録済みの外部Actionを名前で実行する。
    if (actionName_.empty()) return;
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    owner_->ExecuteAction(actionName_);
}

void DragonAIComponent::EnableBiteHitBox()
{
    BS_PROFILE_SCOPE("DragonAIComponent::EnableBiteHitBox");

    // 頭部HitBoxの攻撃IDを更新してから有効化し、噛みつきごとに新しい攻撃判定として扱う。
    ExecuteOwnerAction("BiteHitBox_NewAttackInstance");
    ExecuteOwnerAction("BiteHitBox_ClearAttackHits");
    ExecuteOwnerAction("BiteHitBox_EnableHitBox");
}

void DragonAIComponent::DisableBiteHitBox()
{
    ExecuteOwnerAction("BiteHitBox_DisableHitBox");
}

void DragonAIComponent::EnableTailHitBoxes()
{
    BS_PROFILE_SCOPE("DragonAIComponent::EnableTailHitBoxes");

    // 尻尾は複数部位に分かれているため、同じ攻撃タイミングでまとめて開く。
    ExecuteOwnerAction("TailAttack01HitBox_NewAttackInstance");
    ExecuteOwnerAction("TailAttack01HitBox_ClearAttackHits");
    ExecuteOwnerAction("TailAttack01HitBox_EnableHitBox");
    ExecuteOwnerAction("TailAttack02HitBox_NewAttackInstance");
    ExecuteOwnerAction("TailAttack02HitBox_ClearAttackHits");
    ExecuteOwnerAction("TailAttack02HitBox_EnableHitBox");
}

void DragonAIComponent::DisableTailHitBoxes()
{
    ExecuteOwnerAction("TailAttack01HitBox_DisableHitBox");
    ExecuteOwnerAction("TailAttack02HitBox_DisableHitBox");
}

void DragonAIComponent::DisableSequencerHitBoxes()
{
    DisableBiteHitBox();
    DisableTailHitBoxes();
}

void DragonAIComponent::StartSequencerSpinTailIK()
{
    BS_PROFILE_SCOPE("DragonAIComponent::StartSequencerSpinTailIK");

    const DragonActionDefinition* definition_ = FindDefinition(DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP);
    const float duration_ = definition_ ? definition_->duration : 1.8f;
    if (const std::shared_ptr<DragonTailCommandIKComponent> tailIK_ = tailIK.lock())
    {
        tailIK_->StartSpinSweep(duration_);
    }
}

void DragonAIComponent::ClearTailIK()
{
    if (const std::shared_ptr<DragonTailCommandIKComponent> tailIK_ = tailIK.lock())
    {
        tailIK_->SetFollowBody();
    }
}

void DragonAIComponent::SendRuntimeToBlackboard()
{
    BS_PROFILE_SCOPE("DragonAIComponent::SendRuntimeToBlackboard");

    // BehaviorGraphやAnimationGraphが参照する現在状況を、毎フレームBlackboardへミラーリングする。
    if (!blackboard) return;

    const bool isActing_ =
        (aiState == DRAGON_AI_STATE::ACTION ||
            aiState == DRAGON_AI_STATE::AIR) &&
        !runtime.actionEnded;
    const bool canUseTurnToPlayer_ = IsActionAvailable(DRAGON_ACTION_TYPE::TURN_TO_PLAYER);
    const bool canUseFireBall_ =
        context.canUseFireBall;
    const bool canUseFireBallSweep_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP);
    const bool canUseFireBallFront_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::FIRE_BALL_FRONT);
    const bool canUseFireBallTriple_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE);
    const bool canUseFireBeamSweep_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP);
    const bool canUseSpreadBreathForward_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD);
    const bool canUseAirFireBall_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::AIR_FIRE_BALL);
    const bool canUseAirSpreadBreath_ =
        IsActionAvailable(DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH);
    const bool canUseLandingAttack_ = IsActionAvailable(DRAGON_ACTION_TYPE::LANDING_ATTACK);
    const bool canUseGroundStamp_ =
        context.canUseGroundStamp;

    // 上段は読みやすいキー名、下段は互換用の別名も含めて同じ状況を広めに公開している。
    blackboard->SetBool(blackboardCategory, "HasTarget", hasPlayerTarget);
    blackboard->SetFloat(blackboardCategory, "TargetDistance", context.playerDistance);
    blackboard->SetFloat(blackboardCategory, "TargetHorizontalDistance", context.playerHorizontalDistance);
    blackboard->SetFloat(blackboardCategory, "TargetAngle", context.playerAngleDegrees);
    blackboard->SetBool(blackboardCategory, "IsTargetInFront", context.isPlayerInFront);
    blackboard->SetBool(blackboardCategory, "IsTargetInSide", context.isPlayerInSide);
    blackboard->SetBool(blackboardCategory, "IsTargetBehind", context.isPlayerBehind);
    blackboard->SetBool(blackboardCategory, "IsTargetUnderBody", context.isPlayerUnderBody);
    blackboard->SetBool(blackboardCategory, "CanUseBite", context.canUseBite);
    blackboard->SetBool(blackboardCategory, "CanUseFireBall", canUseFireBall_);
    blackboard->SetBool(blackboardCategory, "CanUseSpreadBreath", context.canUseSpreadBreath);
    blackboard->SetBool(blackboardCategory, "CanUseTailSweep", context.canUseTailSweep);
    blackboard->SetBool(blackboardCategory, "CanUseTakeOff", context.canUseTakeOff);
    blackboard->SetBool(blackboardCategory, "CanUseRunCharge", context.canUseRunCharge);
    blackboard->SetBool(blackboardCategory, "CanUseSpinSweep", context.canUseSpinSweep);
    blackboard->SetBool(blackboardCategory, "CanUseGroundStamp", canUseGroundStamp_);
    blackboard->SetBool(blackboardCategory, "CanUseTurnToPlayer", canUseTurnToPlayer_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBallSweep", canUseFireBallSweep_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBallFront", canUseFireBallFront_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBallTriple", canUseFireBallTriple_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBeamSweep", canUseFireBeamSweep_);
    blackboard->SetBool(blackboardCategory, "CanUseSpreadBreathForward", canUseSpreadBreathForward_);
    blackboard->SetBool(blackboardCategory, "CanUseAirFireBall", canUseAirFireBall_);
    blackboard->SetBool(blackboardCategory, "CanUseAirSpreadBreath", canUseAirSpreadBreath_);
    blackboard->SetBool(blackboardCategory, "CanUseLandingAttack", canUseLandingAttack_);
    blackboard->SetBool(blackboardCategory, "PendingAirAttackDecision", pendingAirAttackDecision);
    blackboard->SetBool(blackboardCategory, "PendingAirLandingDecision", pendingAirLandingDecision);
    blackboard->SetBool(blackboardCategory, "PendingUltimateAction", pendingUltimateAction);
    blackboard->SetBool(blackboardCategory, "UltimateDamageReductionActive", ultimateDamageReductionActive);
    blackboard->SetFloat(blackboardCategory, "UltimateDamageRate", ultimateDamageRate);
    blackboard->SetInt(blackboardCategory, "AirAttackCount", airAttackCount);
    blackboard->SetInt(blackboardCategory, "MaxAirAttackCount", maxAirAttackCount);
    blackboard->SetBool(blackboardCategory, "IsAirIdle", aiState == DRAGON_AI_STATE::AIR && runtime.currentAction == DRAGON_ACTION_TYPE::AIR_HOLD && runtime.actionEnded);
    blackboard->SetInt(blackboardCategory, "CurrentAction", static_cast<int>(runtime.currentAction));
    blackboard->SetInt(blackboardCategory, "LastCompletedAttackAction", static_cast<int>(lastCompletedAttackAction));
    blackboard->SetBool(blackboardCategory, AnimationGraphBlackboardKeys::ACTION_ENDED, runtime.actionEnded);
    blackboard->SetBool(blackboardCategory, AnimationGraphBlackboardKeys::IS_ACTING, isActing_);
    blackboard->SetFloat(blackboardCategory, "HpRate", context.hpRate);
    blackboard->SetFloat(blackboardCategory, "AggressionMultiplier", CalculateAggressionMultiplier(context));
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
    blackboard->SetBool(blackboardCategory, "CanUseFireBall", canUseFireBall_);
    blackboard->SetBool(blackboardCategory, "CanUseSpreadBreath", context.canUseSpreadBreath);
    blackboard->SetBool(blackboardCategory, "CanUseTailSweep", context.canUseTailSweep);
    blackboard->SetBool(blackboardCategory, "CanUseTakeOff", context.canUseTakeOff);
    blackboard->SetBool(blackboardCategory, "CanUseRunCharge", context.canUseRunCharge);
    blackboard->SetBool(blackboardCategory, "CanUseSpinSweep", context.canUseSpinSweep);
    blackboard->SetBool(blackboardCategory, "CanUseTurnToPlayer", canUseTurnToPlayer_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBallSweep", canUseFireBallSweep_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBallFront", canUseFireBallFront_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBallTriple", canUseFireBallTriple_);
    blackboard->SetBool(blackboardCategory, "CanUseFireBeamSweep", canUseFireBeamSweep_);
    blackboard->SetBool(blackboardCategory, "CanUseSpreadBreathForward", canUseSpreadBreathForward_);
    blackboard->SetBool(blackboardCategory, "CanUseAirFireBall", canUseAirFireBall_);
    blackboard->SetBool(blackboardCategory, "CanUseAirSpreadBreath", canUseAirSpreadBreath_);
    blackboard->SetBool(blackboardCategory, "CanUseLandingAttack", canUseLandingAttack_);
    blackboard->SetBool(blackboardCategory, "PendingAirAttackDecision", pendingAirAttackDecision);
    blackboard->SetBool(blackboardCategory, "PendingAirLandingDecision", pendingAirLandingDecision);
    blackboard->SetBool(blackboardCategory, "PendingUltimateAction", pendingUltimateAction);
    blackboard->SetBool(blackboardCategory, "UltimateDamageReductionActive", ultimateDamageReductionActive);
    blackboard->SetFloat(blackboardCategory, "UltimateDamageRate", ultimateDamageRate);
    blackboard->SetInt(blackboardCategory, "AirAttackCount", airAttackCount);
    blackboard->SetInt(blackboardCategory, "MaxAirAttackCount", maxAirAttackCount);
    blackboard->SetBool(blackboardCategory, "IsAirIdle", aiState == DRAGON_AI_STATE::AIR && runtime.currentAction == DRAGON_ACTION_TYPE::AIR_HOLD && runtime.actionEnded);
}

void DragonAIComponent::EnsureDefaultBehaviorGraph()
{
    // グラフ自動生成は行わず、アセットが無い場合はフォールバックAIを使うことを明示する。
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

    // 条件分岐はSequence配下にBlackboard条件ノードを置き、後続Actionをつなげる形で構築する。
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

    // Action名に対応するCanUseキーを挟み、クールダウンや距離条件をBlackboard経由でGraphへ反映する。
    std::string availabilityKey_;
    if (actionName_ == "DragonAI_Bite") availabilityKey_ = "CanUseBite";
    else if (actionName_ == "DragonAI_FireBall") availabilityKey_ = "CanUseFireBall";
    else if (actionName_ == "DragonAI_SpreadBreath") availabilityKey_ = "CanUseSpreadBreath";
    else if (actionName_ == "DragonAI_TurnTailSweep") availabilityKey_ = "CanUseTailSweep";
    else if (actionName_ == "DragonAI_TakeOff") availabilityKey_ = "CanUseTakeOff";
    else if (actionName_ == "DragonAI_SpinTailSweep") availabilityKey_ = "CanUseSpinSweep";
    else if (actionName_ == "DragonAI_GroundStamp") availabilityKey_ = "CanUseGroundStamp";
    else if (actionName_ == "DragonAI_RunCharge") availabilityKey_ = "CanUseRunCharge";
    else if (actionName_ == "DragonAI_TurnToPlayer") availabilityKey_ = "CanUseTurnToPlayer";
    else if (actionName_ == "DragonAI_FireBallSweep") availabilityKey_ = "CanUseFireBallSweep";
    else if (actionName_ == "DragonAI_FireBallFront") availabilityKey_ = "CanUseFireBallFront";
    else if (actionName_ == "DragonAI_FireBallTriple") availabilityKey_ = "CanUseFireBallTriple";
    else if (actionName_ == "DragonAI_FireBeamSweep") availabilityKey_ = "CanUseFireBeamSweep";
    else if (actionName_ == "DragonAI_SpreadBreathForward") availabilityKey_ = "CanUseSpreadBreathForward";
    else if (actionName_ == "DragonAI_AirFireBall") availabilityKey_ = "CanUseAirFireBall";
    else if (actionName_ == "DragonAI_AirSpreadBreath") availabilityKey_ = "CanUseAirSpreadBreath";
    else if (actionName_ == "DragonAI_LandingAttack") availabilityKey_ = "CanUseLandingAttack";

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

    // 読み込み成功時はパスを保持し、ランタイム状態をリセットして新しいグラフを即使える状態にする。
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
    // SequencerやBehaviorGraphからDragonAIを制御するためのランタイムAction名を登録する。
    const auto owner_ = GetGameObject();
    if (!owner_) return;

    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& function_)
        {
            // 登録名は後で解除/確認できるよう、BuildActionName後の完全名も保持しておく。
            owner_->RegisterAction(actionCategory, actionName_, function_);
            registeredActionNames.push_back(owner_->BuildActionName(actionName_));
        };

    registerAction_("DragonAI_CanCancelOn", [this]() { SetCanCancel(true); });
    registerAction_("DragonAI_CanCancelOff", [this]() { SetCanCancel(false); });
    registerAction_("DragonAI_InputBufferOn", [this]() { SetCanBufferAction(true); });
    registerAction_("DragonAI_InputBufferOff", [this]() { SetCanBufferAction(false); });
    registerAction_("DragonAI_ActionEnd", [this]() { NotifySequencerActionEnd(); });
    registerAction_("DragonAI_SequencerActionEnd", [this]() { NotifySequencerActionEnd(); });
    registerAction_("DragonAI_NotifyActionEnd", [this]() { NotifyAIActionEnd(); });
    registerAction_("DragonAI_AIActionEnd", [this]() { NotifyAIActionEnd(); });
    registerAction_("DragonAI_PlaySequencerAttack", [this]() { PlaySequencerAttackAction(); });
    registerAction_("DragonAI_ShowSequencerDanger", [this]() { ShowSequencerDangerAction(); });
    registerAction_("DragonAI_EnableBiteHitBox", [this]() { EnableBiteHitBox(); });
    registerAction_("DragonAI_DisableBiteHitBox", [this]() { DisableBiteHitBox(); });
    registerAction_("DragonAI_EnableTailHitBox", [this]() { EnableTailHitBoxes(); });
    registerAction_("DragonAI_DisableTailHitBox", [this]() { DisableTailHitBoxes(); });
    registerAction_("DragonAI_StartSpinTailIK", [this]() { StartSequencerSpinTailIK(); });
    registerAction_("DragonAI_ClearTailIK", [this]() { ClearTailIK(); });
    registerAction_("DragonAI_PlayFireBallTripleSecond", [this]()
        {
            PlaySequencerLinkedState("PlainsDragon_SpitFireBall_Triple2");
        });
    registerAction_("DragonAI_PlayFireBallTripleThird", [this]()
        {
            PlaySequencerLinkedState("PlainsDragon_SpitFireBall_Triple3");
        });
    registerAction_("DragonAI_SelectWeightedAction", [this]() { RequestWeightedAction(); });
    registerAction_("DragonAI_DamageReaction", [this]() { RequestDamageReaction(); });
    registerAction_("Health_Death", []()
        {
            ActionRegistry::Instance().Call("GameMode.NotifyPhase1BossDefeated");
        });
    // 移行演出へ渡す前にSkillと攻撃判定を閉じ、旧Actionの予約を残さない。
    const std::weak_ptr<GameObject> presentationOwner_ = owner_;
    owner_->RegisterGlobalAction("ActorPresentation", "DragonPresentation_Stop", [presentationOwner_]()
    {
        BS_PROFILE_SCOPE("DragonAIComponent::PresentationStop");
        const auto owner_ = presentationOwner_.lock();
        const auto dragon_ = owner_ ? owner_->GetComponent<DragonAIComponent>() : nullptr;
        if (!dragon_) return;
        dragon_->StopSkill();
        // Skill終了Actionに依存せず、予告だけの攻撃と生成済みEffectも確実に停止する。
        if (const auto attacks_ = owner_->GetComponent<DragonAttackControllerComponent>()) attacks_->StopAttack();
        dragon_->DisableSequencerHitBoxes();
        dragon_->ClearTailIK();
        dragon_->StopMovement();
    });
    registerAction_("DragonAI_GetHit", [this]() { ForceAction(DRAGON_ACTION_TYPE::GET_HIT); });
    registerAction_("DragonAI_AirGetHit", [this]() { ForceAction(DRAGON_ACTION_TYPE::AIR_GET_HIT); });
    registerAction_("DragonAI_FaceTarget", [this]() { FaceTargetFromAnimationEvent(); });
    // ブレス派生のシーケンサーから、首IKの向きを時間ごとに切り替える。
    registerAction_("DragonAI_NeckLookTarget", [this]()
        {
            if (const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock())
            {
                neckIK_->LookAtTarget(playerObjectName, 0.65f, 0.08f, 0.16f);
            }
        });
    registerAction_("DragonAI_NeckLookFront", [this]()
        {
            if (const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock())
            {
                neckIK_->LookAtYawOffset(0.0f, 0.65f, 0.08f, 0.16f);
            }
        });
    registerAction_("DragonAI_NeckLookLeft45", [this]()
        {
            if (const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock())
            {
                neckIK_->LookAtYawOffset(-45.0f, 0.55f, 0.08f, 0.12f);
            }
        });
    registerAction_("DragonAI_NeckLookRight45", [this]()
        {
            if (const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock())
            {
                neckIK_->LookAtYawOffset(45.0f, 0.55f, 0.08f, 0.12f);
            }
        });
    registerAction_("DragonAI_NeckSweepLeft45ToRight45", [this]()
        {
            if (const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock())
            {
                neckIK_->SweepYaw(-45.0f, 45.0f, 2.0f, 0.12f, 0.18f);
            }
        });
    registerAction_("DragonAI_NeckIKClear", [this]()
        {
            if (const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock())
            {
                neckIK_->ClearCommand();
            }
        });
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
    registerAction_("DragonAI_TestGroundStamp", [this]() { RequestAction(DRAGON_ACTION_TYPE::GROUND_STAMP); });
    registerAction_("DragonAI_TestTakeOff", [this]() { RequestAction(DRAGON_ACTION_TYPE::TAKE_OFF); });
    registerAction_("DragonAI_TestUltimate", [this]() { ForceAction(DRAGON_ACTION_TYPE::ULTIMATE_ATTACK); });
    registerAction_("DragonAI_UltimateEnd", [this]() { NotifySequencerActionEnd(); });
    registerAction_("DragonAI_ForceIdle", [this]() { ForceAction(DRAGON_ACTION_TYPE::IDLE); });
    registerAction_("DragonAI_ForceTurn", [this]() { ForceAction(DRAGON_ACTION_TYPE::TURN_TO_PLAYER); });
    registerAction_("DragonAI_ForceWalk", [this]() { ForceAction(DRAGON_ACTION_TYPE::WALK_APPROACH); });
    registerAction_("DragonAI_ForceDeath", [this]() { ForceAction(DRAGON_ACTION_TYPE::DEATH); });
}

void DragonAIComponent::PlaySequencerAttackAction()
{
    BS_PROFILE_SCOPE("DragonAIComponent::PlaySequencerAttackAction");

    // ステートイベントを同じ名前で置けるよう、現在のAI行動から実際の攻撃Actionへ振り分ける。
    switch (runtime.currentAction)
    {
    case DRAGON_ACTION_TYPE::BITE:
        EnableBiteHitBox();
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
    case DRAGON_ACTION_TYPE::FIRE_BALL_FRONT:
    case DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE:
        ExecuteOwnerAction("DragonAttack_PlayBreathProjectileSingle");
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP:
    case DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP:
        ExecuteOwnerAction("DragonAttack_PlayBreathProjectileBeam");
        break;
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD:
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
        ExecuteOwnerAction("DragonAttack_PlayBreathProjectileFan");
        break;
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
        EnableTailHitBoxes();
        break;
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        StartSequencerSpinTailIK();
        EnableTailHitBoxes();
        break;
    case DRAGON_ACTION_TYPE::GROUND_STAMP:
        ExecuteOwnerAction("DragonAttack_PlayStamp");
        break;
    case DRAGON_ACTION_TYPE::LANDING_ATTACK:
    case DRAGON_ACTION_TYPE::DEATH_HIT_GROUND:
        ExecuteOwnerAction("DragonAttack_PlayStamp");
        break;
    default:
        break;
    }
}

void DragonAIComponent::ShowSequencerDangerAction()
{
    BS_PROFILE_SCOPE("DragonAIComponent::ShowSequencerDangerAction");

    // 予兆表示も現在Actionで分岐し、共有ステートが通常旋回として使われる時は何もしない。
    switch (runtime.currentAction)
    {
    case DRAGON_ACTION_TYPE::BITE:
        ExecuteOwnerAction("DragonAttack_ShowBiteDangerArea");
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
    case DRAGON_ACTION_TYPE::FIRE_BALL_FRONT:
    case DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE:
        ExecuteOwnerAction("DragonAttack_ShowBreathProjectileSingleDangerArea");
        break;
    case DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP:
    case DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP:
        ExecuteOwnerAction("DragonAttack_ShowBreathProjectileBeamDangerArea");
        break;
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD:
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
        ExecuteOwnerAction("DragonAttack_ShowBreathProjectileFanDangerArea");
        break;
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
        ExecuteOwnerAction("DragonAttack_ShowTailSweepDangerArea");
        break;
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
        ExecuteOwnerAction("DragonAttack_ShowSpinSweepDangerArea");
        break;
    case DRAGON_ACTION_TYPE::GROUND_STAMP:
    case DRAGON_ACTION_TYPE::LANDING_ATTACK:
    case DRAGON_ACTION_TYPE::DEATH_HIT_GROUND:
        ExecuteOwnerAction("DragonAttack_ShowStampDangerArea");
        break;
    default:
        break;
    }
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

bool DragonAIComponent::IsAIControlledAction(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsAIControlledAction");

    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_) return false;

    return definition_->finishMode == DRAGON_ACTION_FINISH_MODE::AI_CONDITION ||
        definition_->finishMode == DRAGON_ACTION_FINISH_MODE::AI_DURATION;
}

bool DragonAIComponent::IsSequencerControlledAction(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsSequencerControlledAction");

    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_) return false;

    return definition_->finishMode == DRAGON_ACTION_FINISH_MODE::SEQUENCER;
}

bool DragonAIComponent::IsAirborneAction(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsAirborneAction");

    switch (action_)
    {
    case DRAGON_ACTION_TYPE::TAKE_OFF:
    case DRAGON_ACTION_TYPE::AIR_HOLD:
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::LANDING_ATTACK:
    case DRAGON_ACTION_TYPE::AIR_GET_HIT:
        return true;
    default:
        return false;
    }
}

bool DragonAIComponent::IsDeathAction(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsDeathAction");

    return action_ == DRAGON_ACTION_TYPE::DEATH ||
        action_ == DRAGON_ACTION_TYPE::AIR_DEATH_FALL_START ||
        action_ == DRAGON_ACTION_TYPE::FALLING ||
        action_ == DRAGON_ACTION_TYPE::DEATH_HIT_GROUND;
}

bool DragonAIComponent::IsRepeatLimitedAttack(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::IsRepeatLimitedAttack");

    // 連続使用を避けたい攻撃は、完了後にlastCompletedAttackActionへ記録して次回候補の重みを下げる。
    switch (action_)
    {
    case DRAGON_ACTION_TYPE::BITE:
    case DRAGON_ACTION_TYPE::FIRE_BALL:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::AIR_FIRE_BALL:
    case DRAGON_ACTION_TYPE::AIR_SPREAD_BREATH:
    case DRAGON_ACTION_TYPE::FIRE_BALL_SWEEP:
    case DRAGON_ACTION_TYPE::FIRE_BALL_FRONT:
    case DRAGON_ACTION_TYPE::FIRE_BALL_TRIPLE:
    case DRAGON_ACTION_TYPE::FIRE_BEAM_SWEEP:
    case DRAGON_ACTION_TYPE::SPREAD_BREATH_FORWARD:
    case DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP:
    case DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP:
    case DRAGON_ACTION_TYPE::GROUND_STAMP:
        return true;
    default:
        return false;
    }
}

bool DragonAIComponent::IsEnemyAILockedByGameMode() const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->manager) return false;

    const GameModeManager* gameModeManager_ = owner_->GetManager().GetGameModeManager();
    if (!gameModeManager_) return false;

    bool locked_ = false;
    if (gameModeManager_->GetGlobalBlackboard().GetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::ENEMY_AI_LOCKED,
        locked_))
    {
        return locked_;
    }

    return false;
}

bool DragonAIComponent::CanUseGroundApproach(const DragonAIContext& context_) const
{
    const float moveEndDistance_ = (std::max)(approachEndDistance, biteDistance + 0.75f);
    // 地上接近は正面へ捉えている時だけ許可し、横向きの足踏み状態を防ぐ。
    return hasPlayerTarget &&
        context_.isPlayerInFront &&
        context_.playerHorizontalDistance > moveEndDistance_;
}

bool DragonAIComponent::ShouldTurnForCloseFrontTarget(const DragonAIContext& context_) const
{
    const float closeFrontTurnAngleDegrees_ = 35.0f;
    const float closeFrontTurnDistance_ = (std::max)(approachEndDistance + 1.0f, biteDistance + 1.5f);
    // 広めの正面判定内でも、近距離で斜めに立たれた時は攻撃前に向きを整える。
    return hasPlayerTarget &&
        context_.isPlayerInFront &&
        std::abs(context_.playerAngleDegrees) >= closeFrontTurnAngleDegrees_ &&
        context_.playerHorizontalDistance <= closeFrontTurnDistance_;
}

bool DragonAIComponent::CanUseTurnTailSweep(const DragonAIContext& context_) const
{
    const float minimumDistance_ = (std::max)(underBodyDistance * 0.65f, biteDistance * 0.45f);
    const float maximumDistance_ = (std::max)(biteDistance + 3.5f, approachEndDistance + 2.0f);
    // 尻尾の横薙ぎは背面寄りで、近すぎず遠すぎない時だけ使う。
    return hasPlayerTarget &&
        (context_.isPlayerBehind || context_.isPlayerInSide) &&
        context_.playerHorizontalDistance >= minimumDistance_ &&
        context_.playerHorizontalDistance <= maximumDistance_;
}

bool DragonAIComponent::CanUseSpinTailSweep(const DragonAIContext& context_) const
{
    const float minimumDistance_ = (std::max)(biteDistance * 0.75f, underBodyDistance + 0.75f);
    const float maximumDistance_ = (std::max)(biteDistance + 4.5f, approachEndDistance + 3.0f);
    // 回転尻尾は届く外周にいる相手への反撃に限定し、当たらない遠距離や密着での連打を避ける。
    return hasPlayerTarget &&
        (context_.isPlayerBehind || context_.isPlayerInSide) &&
        context_.playerHorizontalDistance >= minimumDistance_ &&
        context_.playerHorizontalDistance <= maximumDistance_;
}

DRAGON_ACTION_TYPE DragonAIComponent::ResolveAntiFlankAction()
{
    BS_PROFILE_SCOPE("DragonAIComponent::ResolveAntiFlankAction");
    if (!hasPlayerTarget || context.isPlayerInFront || context.playerHorizontalDistance > mediumDistance)
        return DRAGON_ACTION_TYPE::IDLE;

    // 横・背後への反撃も同じ履歴補正を使い、旋回要求からの連続攻撃を防ぐ。
    std::vector<DragonActionCandidate> candidates_;
    if (CanUseTurnTailSweep(context))
        AddWeightedActionCandidate(context, candidates_, DRAGON_ACTION_TYPE::TURN_TAIL_SWEEP, context.isPlayerBehind ? 4.0f : 1.4f);
    if (CanUseSpinTailSweep(context))
        AddWeightedActionCandidate(context, candidates_, DRAGON_ACTION_TYPE::SPIN_TAIL_SWEEP, context.isPlayerBehind ? 1.2f : 0.6f);
    if (context.playerHorizontalDistance <= biteDistance + 1.5f)
        AddWeightedActionCandidate(context, candidates_, DRAGON_ACTION_TYPE::TAKE_OFF, 2.0f);

    return SelectHistoryWeightedAction(candidates_);
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
    // 有効フラグとクールダウンをまとめた、AI/Graph共通の使用可否判定。
    if (action_ == DRAGON_ACTION_TYPE::IDLE)
    {
        return true;
    }
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!definition_ || !definition_->enabled)
    {
        return false;
    }
    if (!definition_->skillName.empty())
    {
        const auto iterator_ = std::find_if(skillDefinitions.begin(), skillDefinitions.end(),
            [definition_](const SkillDefinition& skill_) { return skill_.name == definition_->skillName; });
        std::string error_;
        if (iterator_ == skillDefinitions.end() || !SkillExecution::CanExecute(gameObject.lock(), *iterator_, error_)) return false;
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

// 選択される攻撃と離陸・突進を履歴対象とし、待機や強制遷移は対象外にする。
bool DragonAIComponent::UsesActionHistory(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::UsesActionHistory");
    return IsRepeatLimitedAttack(action_) || action_ == DRAGON_ACTION_TYPE::RUN_CHARGE ||
        action_ == DRAGON_ACTION_TYPE::TAKE_OFF;
}

void DragonAIComponent::RecordActionHistory(DRAGON_ACTION_TYPE action_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::RecordActionHistory");
    if (!UsesActionHistory(action_)) return;
    if (actionLastUsedOrders.size() != DRAGON_ACTION_COUNT) actionLastUsedOrders.assign(DRAGON_ACTION_COUNT, -1);
    actionLastUsedOrders[ToIndex(action_)] = ++attackExecutionOrder;
    lastExecutedAttack = action_;
}

// 回復回数までは抑制を緩め、長期間使っていない攻撃には強い抽選補正を与える。
float DragonAIComponent::CalculateHistoryModifier(DRAGON_ACTION_TYPE action_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::CalculateHistoryModifier");
    const DragonActionDefinition* definition_ = FindDefinition(action_);
    if (!historyEnabled || !UsesActionHistory(action_) || !definition_ || !definition_->useHistory) return 1.0f;
    const int lastOrder_ = actionLastUsedOrders.size() == DRAGON_ACTION_COUNT ? actionLastUsedOrders[ToIndex(action_)] : -1;
    const int otherCount_ = lastOrder_ < 0 ? attackExecutionOrder : attackExecutionOrder - lastOrder_;
    const float recentRate_ = std::clamp(repeatActionPenaltyRate, 0.0f, 1.0f);
    const float recovery_ = std::clamp(static_cast<float>(otherCount_) / (std::max)(1, historyRecoveryCount), 0.0f, 1.0f);
    float modifier_ = lastOrder_ < 0 ? 1.0f : recentRate_ + (1.0f - recentRate_) * recovery_;
    if (otherCount_ >= (std::max)(1, historyStarvationCount)) modifier_ = (std::max)(1.0f, historyMaxBonus);
    // 唯一実行可能な攻撃は、抑制値がゼロでも抽選候補として残す。
    return (std::max)(0.001f, modifier_);
}

void DragonAIComponent::PrepareHistorySelection(const std::vector<DRAGON_ACTION_TYPE>& actions_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::PrepareHistorySelection");
    historySelectionAvailable.assign(DRAGON_ACTION_COUNT, false);
    selectionDebugWeights.assign(DRAGON_ACTION_COUNT, {});
    excludePreviousAttack = false;
    for (const DRAGON_ACTION_TYPE action_ : actions_)
    {
        historySelectionAvailable[ToIndex(action_)] = true;
        if (historyEnabled && UsesActionHistory(action_) && action_ != lastExecutedAttack)
            excludePreviousAttack = true;
    }
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        DragonActionSelectionDebug& debug_ = selectionDebugWeights[ToIndex(definition_.action)];
        debug_.baseWeight = GetSelectionWeight(definition_.action);
        debug_.historyModifier = CalculateHistoryModifier(definition_.action);
        debug_.finalWeight = historySelectionAvailable[ToIndex(definition_.action)] &&
            !(excludePreviousAttack && definition_.action == lastExecutedAttack) ? debug_.baseWeight * debug_.historyModifier : 0.0f;
    }
}

float DragonAIComponent::ResolveGraphActionModifier(const std::string& actionName_) const
{
    BS_PROFILE_SCOPE("DragonAIComponent::ResolveGraphActionModifier");
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        if (actionName_ != "DragonAI_" + definition_.actionName) continue;
        if (!UsesActionHistory(definition_.action)) return 1.0f;
        if (!IsActionAvailable(definition_.action) || (excludePreviousAttack && definition_.action == lastExecutedAttack)) return 0.0f;
        return GetSelectionWeight(definition_.action) * CalculateHistoryModifier(definition_.action);
    }
    return 1.0f;
}

// 直近の選択時に使った候補と、現在のCooldown・使用順を表示する。
void DragonAIComponent::DrawHistoryDebug()
{
    BS_PROFILE_SCOPE("DragonAIComponent::DrawHistoryDebug");
    if (!ImGui::CollapsingHeader("Action Selection History")) return;
    ImGui::Checkbox("Enable History", &historyEnabled);
    ImGui::DragFloat("Recent Use Modifier", &repeatActionPenaltyRate, 0.01f, 0.0f, 1.0f);
    ImGui::DragInt("History Recovery Count", &historyRecoveryCount, 1.0f, 1, 20);
    ImGui::DragInt("History Starvation Count", &historyStarvationCount, 1.0f, 1, 30);
    ImGui::DragFloat("History Max Bonus", &historyMaxBonus, 0.1f, 1.0f, 20.0f);
    ImGui::DragFloat("Attack Cooldown Multiplier", &attackCooldownMultiplier, 0.25f, 0.0f, 10.0f);
    ImGui::Text("Last selection candidates / order %d / previous %s", attackExecutionOrder, GetDragonActionName(lastExecutedAttack));
    if (!ImGui::BeginTable("ActionHistory", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX)) return;
    ImGui::TableSetupColumn("Action Name");
    ImGui::TableSetupColumn("Base Weight");
    ImGui::TableSetupColumn("History Modifier");
    ImGui::TableSetupColumn("Final Weight");
    ImGui::TableSetupColumn("Cooldown Remaining");
    ImGui::TableSetupColumn("Last Used Order");
    ImGui::TableSetupColumn("Available");
    ImGui::TableHeadersRow();
    for (const DragonActionDefinition& definition_ : actionDefinitions)
    {
        const int index_ = ToIndex(definition_.action);
        const bool available_ = historySelectionAvailable.size() == DRAGON_ACTION_COUNT && historySelectionAvailable[index_];
        const DragonActionSelectionDebug debug_ = selectionDebugWeights.size() == DRAGON_ACTION_COUNT ? selectionDebugWeights[index_] : DragonActionSelectionDebug{};
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(definition_.actionName.c_str());
        ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", debug_.baseWeight);
        ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", debug_.historyModifier);
        ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", debug_.finalWeight);
        ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", GetCooldown(definition_.action));
        ImGui::TableSetColumnIndex(5); ImGui::Text("%d", actionLastUsedOrders.size() == DRAGON_ACTION_COUNT ? actionLastUsedOrders[index_] : -1);
        ImGui::TableSetColumnIndex(6); ImGui::TextUnformatted(available_ && debug_.finalWeight > 0.0f ? "Available" : "Unavailable");
    }
    ImGui::EndTable();
}

bool DragonAIComponent::HasBehaviorGraph() const
{
    return !behaviorGraph.GetNodes().empty();
}

REGISTER_COMPONENT(DragonAIComponent, "DragonAIComponent")

// 待機時だけ共通ライブラリを読み込み、実行中の定義参照を維持する。
bool DragonAIComponent::ReloadSkillDefinitions(const std::string& path_)
{
    BS_PROFILE_SCOPE("DragonAIComponent::ReloadSkillDefinitions");
    if (skillActionRunning) return false;
    nlohmann::json document_;
    if (!LoadSkillDefinitions(path_, skillDefinitions, document_, statusText)) return false;
    skillDefinitionsPath = path_;
    return true;
}

// 既存のAction識別子・BehaviorGraph・重み付き判断を維持してSkill名を接続する。
bool DragonAIComponent::SetSkillForAction(DRAGON_ACTION_TYPE action_, const std::string& skillName_, const std::string& path_)
{
    if (!ReloadSkillDefinitions(path_)) return false;
    auto* definition_ = FindDefinition(action_);
    if (!definition_) return false;
    definition_->skillName = skillName_;
    return true;
}

// 明示的な中断でも従来のRecovery・先行入力処理を使用する。
void DragonAIComponent::StopSkill()
{
    if (!skillActionRunning) return;
    skillExecution.Stop();
    skillActionRunning = false;
    pendingSequencerActionEnd = false;
    CompleteActionEnd();
}
