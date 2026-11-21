#include "PrecompiledHeader.h"

#include "HumanBossAITraceComponent.h"
#include "HumanBossAIComponent.h"
#include "HumanBossActionComponent.h"
#include "Game/Charcter/Player/PlayerState.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Game/GameMode/GameModeManager.h"
#include "Game/GameMode/GameModeBlackboardKeys.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Debug/TracyProfiler.h"

#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <exception>
#include <utility>
#include <imgui.h>

namespace
{
    // 条件の成否と継続中の状態を追跡画面の表示名へ変換する。
    const char* GetStatusName(AI_BEHAVIOR_STATUS status_)
    {
        switch (status_)
        {
        case AI_BEHAVIOR_STATUS::SUCCESS: return "PASS / SUCCESS";
        case AI_BEHAVIOR_STATUS::FAILURE: return "FAIL / REJECTED";
        default: return "RUNNING";
        }
    }

    // 固定したスナップショットを再帰表示し、現在のBlackboardを参照し直さない。
    void DrawSnapshotFields(const nlohmann::json& value_)
    {
        for (auto field_ = value_.begin(); field_ != value_.end(); ++field_)
        {
            const std::string label_ = value_.is_object() ? field_.key() : std::to_string(field_ - value_.begin());
            if (field_->is_structured())
            {
                if (ImGui::TreeNode(label_.c_str()))
                {
                    DrawSnapshotFields(*field_);
                    ImGui::TreePop();
                }
            }
            else
            {
                const std::string text_ = field_->is_string() ? field_->get<std::string>() : field_->dump();
                ImGui::TextWrapped("%s: %s", label_.c_str(), text_.c_str());
            }
        }
    }
}

const std::string& HumanBossAITraceComponent::GetName() const
{
    static const std::string NAME = "HumanBossAITraceComponent";
    return NAME;
}

void HumanBossAITraceComponent::Start()
{
    ClearHistory();
}

void HumanBossAITraceComponent::ClearHistory()
{
    entries.clear();
    lastSignature.clear();
    evaluatedWeights = nlohmann::json::array();
    evaluatedNodes = nlohmann::json::array();
    graphAction.clear();
    evaluating = false;
    nextEntryId = 1;
    selectedEntryId = 0;
    startTime = SDL_GetTicks();
    evaluationTime = 0;
}

void HumanBossAITraceComponent::UpdateUnscaled(float deltaTime_)
{
    BS_PROFILE_SCOPE("HumanBossAITraceComponent::UpdateUnscaled");
    if (!IsRecording())
    {
        return;
    }
    const auto owner_ = GetGameObject();
    const auto ai_ = owner_ ? owner_->GetComponent<HumanBossAIComponent>() : nullptr;
    if (ai_)
    {
        Capture(*ai_, "GameMode / observed state");
    }
}

void HumanBossAITraceComponent::BeginEvaluation()
{
    BS_PROFILE_SCOPE("HumanBossAITraceComponent::BeginEvaluation");
    evaluatedWeights = nlohmann::json::array();
    evaluatedNodes = nlohmann::json::array();
    graphAction.clear();
    evaluationTime = SDL_GetTicks();
    evaluating = true;
}

void HumanBossAITraceComponent::FinishEvaluation()
{
    evaluating = false;
}

void HumanBossAITraceComponent::ObserveActionWeight(const std::string& actionName_, float weight_, const std::string& reason_)
{
    if (!IsRecording())
    {
        return;
    }
    for (auto& candidate_ : evaluatedWeights)
    {
        if (candidate_["Action"] != actionName_)
        {
            continue;
        }
        candidate_["RuntimeWeight"] = weight_;
        candidate_["Reason"] = weight_ <= 0.0f && reason_ == "Eligible: weighted candidate"
            ? "Weight is zero / reaction not ready" : reason_;
        return;
    }
    evaluatedWeights.push_back({ { "Action", actionName_ }, { "RuntimeWeight", weight_ },
        { "Reason", weight_ <= 0.0f && reason_ == "Eligible: weighted candidate"
            ? "Weight is zero / reaction not ready" : reason_ } });
}

void HumanBossAITraceComponent::ObserveGraphWeight(const std::string& actionName_, float baseWeight_, float modifier_)
{
    if (!IsRecording())
    {
        return;
    }
    for (auto& candidate_ : evaluatedWeights)
    {
        if (candidate_["Action"] != actionName_)
        {
            continue;
        }
        candidate_["GraphWeight"] = baseWeight_;
        candidate_["EffectiveWeight"] = (std::max)(0.0f, baseWeight_) * modifier_;
        return;
    }
}

void HumanBossAITraceComponent::ObserveNode(const AIBehaviorNode& node_, AI_BEHAVIOR_STATUS status_, const AIBehaviorContext& context_)
{
    if (!IsRecording() || evaluatedNodes.size() >= 512)
    {
        return;
    }
    nlohmann::json evaluation_{ { "NodeId", node_.id }, { "Name", node_.name },
        { "Result", GetStatusName(status_) }, { "Priority", node_.priority } };
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION)
    {
        evaluation_["Action"] = node_.actionName;
        if (status_ != AI_BEHAVIOR_STATUS::FAILURE)
        {
            graphAction = node_.actionName;
        }
    }
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION || node_.type == AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL)
    {
        const auto blackboard_ = node_.blackboardScope == AI_BLACKBOARD_SCOPE::GLOBAL
            ? context_.globalBlackboard : context_.blackboard;
        evaluation_["Source"] = node_.blackboardScope == AI_BLACKBOARD_SCOPE::GLOBAL ? "GameMode Global" : "Owner Local";
        evaluation_["Key"] = node_.blackboardCategory + "/" + node_.blackboardKey;
        constexpr const char* COMPARES[] = { "Exists", "Not exists", "==", "!=", ">", ">=", "<", "<=" };
        const int compare_ = static_cast<int>(node_.compareType);
        evaluation_["Compare"] = compare_ >= 0 && compare_ < 8 ? COMPARES[compare_] : "Unknown";
        bool bool_ = false;
        float float_ = 0.0f;
        int int_ = 0;
        std::string string_;
        evaluation_["Actual"] = "<missing>";
        if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::BOOL || node_.type == AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL)
        {
            evaluation_["Expected"] = node_.boolValue;
            if (blackboard_ && blackboard_->GetBool(node_.blackboardCategory, node_.blackboardKey, bool_))
            {
                evaluation_["Actual"] = bool_;
            }
        }
        else if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::FLOAT)
        {
            evaluation_["Expected"] = node_.floatValue;
            if (blackboard_ && blackboard_->GetFloat(node_.blackboardCategory, node_.blackboardKey, float_))
            {
                evaluation_["Actual"] = float_;
            }
        }
        else if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::INT)
        {
            evaluation_["Expected"] = node_.intValue;
            if (blackboard_ && blackboard_->GetInt(node_.blackboardCategory, node_.blackboardKey, int_))
            {
                evaluation_["Actual"] = int_;
            }
        }
        else if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::STRING)
        {
            evaluation_["Expected"] = node_.stringValue;
            if (blackboard_ && blackboard_->GetString(node_.blackboardCategory, node_.blackboardKey, string_))
            {
                evaluation_["Actual"] = string_;
            }
        }
    }
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION)
    {
        const auto blackboard_ = node_.blackboardScope == AI_BLACKBOARD_SCOPE::GLOBAL ? context_.globalBlackboard : context_.blackboard;
        const auto value_ = blackboard_ ? blackboard_->GetValue(node_.blackboardCategory, node_.blackboardKey) : nullptr;
        evaluation_["KeyPresent"] = value_ != nullptr;
        // 存在確認と数値変換はRuntimeの条件比較に合わせ、型付きGetterの失敗を値の欠損と混同しない。
        if (node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::EXISTS || node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::NOT_EXISTS)
        {
            evaluation_["Compare"] = "Key exists ==";
            evaluation_["Expected"] = node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::EXISTS;
            evaluation_["Actual"] = value_ != nullptr;
        }
        else if (value_ && node_.valueType != AI_BLACKBOARD_VALUE_TYPE::STRING)
        {
            evaluation_["Actual"] = BlackboardComponent::ToFloat(*value_);
            if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::VECTOR2 || node_.valueType == AI_BLACKBOARD_VALUE_TYPE::VECTOR3 ||
                node_.valueType == AI_BLACKBOARD_VALUE_TYPE::TRIGGER)
            {
                evaluation_["Expected"] = node_.floatValue;
            }
        }
    }
    evaluatedNodes.push_back(std::move(evaluation_));
}

void HumanBossAITraceComponent::Capture(const HumanBossAIComponent& ai_, const char* event_, bool force_)
{
    BS_PROFILE_SCOPE("HumanBossAITraceComponent::Capture");
    if (!IsRecording())
    {
        return;
    }
    const auto owner_ = gameObject.lock();
    const auto mode_ = owner_ && owner_->manager ? owner_->GetManager().GetGameModeManager() : nullptr;
    const auto executor_ = ai_.executor.lock();
    const auto& player_ = ai_.playerObservation;
    bool blackboardLocked_ = false;
    const bool hasBlackboardLock_ = mode_ && mode_->GetGlobalBlackboard().GetBool(
        GameModeBlackboardKeys::CATEGORY_GAME, GameModeBlackboardKeys::ENEMY_AI_LOCKED, blackboardLocked_);
    // 時間や距離の微小変化では行を増やさず、判断・入力履歴・進行状態の変化を識別する。
    std::string signature_ = ai_.GetCurrentGraphActionName() + "|" + GetHumanBossIntentName(ai_.intent) + "|" +
        ai_.intentReason + "|" + ai_.reactionReason + "|" + ai_.suspensionReason + "|" + ai_.berserkStartReason + "|" +
        std::to_string(ai_.phase) + "|" + std::to_string(ai_.berserkStage) + "|" + std::to_string(ai_.berserkStartedCount) + "|" +
        std::to_string(ai_.berserkStunned) + "|" + std::to_string(ai_.justGuardStunned) + "|" + std::to_string(ai_.finishRequested) + "|" +
        std::to_string(player_.serial) + "|" + std::to_string(static_cast<int>(player_.attackPhase)) + "|" + std::to_string(blackboardLocked_) + "|" +
        std::to_string(evaluating) + "|" + std::to_string(ai_.IsEnabled()) + "|" + std::to_string(ai_.ShouldUpdate()) + "|" + std::to_string(owner_ && owner_->IsCombatReady());
    if (mode_)
    {
        signature_ += "|" + std::string(mode_->GetCurrentGameStateName()) + "|" + mode_->GetCurrentSequenceName() + "|" +
        std::to_string(mode_->GetCurrentBattlePhase()) + std::to_string(mode_->IsEnemyAILocked()) +
        std::to_string(mode_->IsGameplayLocked()) + std::to_string(mode_->IsPresentationPlaying()) + std::to_string(mode_->IsBattlePaused()) + "|" +
        std::to_string(mode_->GetBossHpRate()) + "|" + std::to_string(mode_->GetPlayerHpRate()) + "|" +
        std::to_string(mode_->GetAIDirectorPreferDelayedAttack()) + std::to_string(mode_->GetAIDirectorPreferFeintAttack()) +
        std::to_string(mode_->GetAIDirectorPreferGuardBreak()) + std::to_string(mode_->GetAIDirectorReduceAggression()) + "|" +
        std::to_string(mode_->GetAIDirectorAttackDelayRate()) + "|" + std::to_string(mode_->GetAIDirectorFeintRate()) + "|" +
        std::to_string(mode_->GetAIDirectorGuardBreakRate()) + "|" + std::to_string(mode_->GetAIDirectorAggressionRate());
    }
    for (const auto id_ : ai_.behaviorGraph.GetRuntimeActiveNodeIds())
    {
        signature_ += ":" + std::to_string(id_);
    }
    if (!force_ && signature_ == lastSignature)
    {
        return;
    }
    lastSignature = std::move(signature_);
    if (startTime == 0)
    {
        startTime = SDL_GetTicks();
    }
    TraceEntry entry_;
    entry_.id = nextEntryId++;
    entry_.time = static_cast<float>(SDL_GetTicks() - startTime) * 0.001f;
    entry_.event = event_;
    auto& snapshot_ = entry_.snapshot;
    snapshot_["Action"] = ai_.GetCurrentGraphActionName().empty() ? graphAction : ai_.GetCurrentGraphActionName();
    snapshot_["Intent"] = GetHumanBossIntentName(ai_.intent);
    snapshot_["Reason"] = ai_.intentReason;
    snapshot_["Suspension"] = ai_.suspensionReason;
    snapshot_["ThreatReason"] = ai_.reactionReason;
    snapshot_["AI"] = { { "Phase", ai_.phase + 1 }, { "Distance", ai_.distance }, { "Enabled", ai_.IsEnabled() },
        { "UpdateEnabled", ai_.ShouldUpdate() }, { "RuntimeStarted", ai_.runtimeStarted }, { "CombatReady", owner_ && owner_->IsCombatReady() },
        { "ReactionBudget", ai_.reactionBudget }, { "ReactionDelay", ai_.reactionDelayRemaining },
        { "ThreatDetected", ai_.playerAttackDetected }, { "IncomingTime", ai_.incomingTime },
        { "WaitRemaining", ai_.waitTimer }, { "AttackCooldown", ai_.attackTimer }, { "DodgeCooldown", ai_.dodgeTimer },
        { "ReactionCooldown", ai_.reactionTimer }, { "CounterPending", ai_.counterPending },
        { "AirHeavyChain", ai_.airHeavyAttackChain }, { "AirHeavyPressure", ai_.airHeavyPressureTimer },
        { "RangedPressure", ai_.rangedPressureTime }, { "ExecutorActive", executor_ && executor_->IsActive() },
        { "ExecutorError", executor_ ? executor_->GetValidationError() : "Executor missing" } };
    snapshot_["Berserk"] = { { "Stage", ai_.berserkStage }, { "AttackCount", ai_.berserkStartedCount },
        { "Stunned", ai_.berserkStunned }, { "Pending", ai_.berserkPending }, { "Used", ai_.berserkUsed },
        { "Reason", ai_.berserkStartReason }, { "FinalPhaseReserved", ai_.finalPhaseReserved }, { "FinishRequested", ai_.finishRequested } };
    snapshot_["Player"] = { { "Valid", player_.valid }, { "Alive", player_.alive }, { "Serial", player_.serial },
        { "Action", player_.actionName }, { "Animation", player_.animationState }, { "AttackPhase", static_cast<int>(player_.attackPhase) },
        { "AttackPhaseName", player_.attackPhase == ObservedAttackPhase::STARTUP ? "Startup" : player_.attackPhase == ObservedAttackPhase::ACTIVE
            ? "Active" : player_.attackPhase == ObservedAttackPhase::RECOVERY ? "Recovery" : "None" },
        { "Heavy", player_.heavyAttack }, { "Guarding", player_.guarding }, { "GuardHoldTime", player_.guardHoldTime },
        { "Dodging", player_.dodging }, { "Airborne", player_.airborne }, { "SameActionCount", player_.sameActionCount },
        { "AttackFrequency", player_.attackFrequency }, { "HeavyRate", player_.heavyAttackRate },
        { "GuardFrequency", player_.guardFrequency }, { "DodgeFrequency", player_.dodgeFrequency } };
    snapshot_["PlayerHistory"] = nlohmann::json::array();
    for (const auto& action_ : player_.history)
    {
        snapshot_["PlayerHistory"].push_back({ { "ObservationTime", action_.time }, { "ActionId", action_.action }, { "Action", GetPlayerActionName(static_cast<PLAYER_ACTION>(action_.action)) },
            { "Attack", action_.attack }, { "Weak", action_.weakAttack }, { "Heavy", action_.heavyAttack },
            { "Guard", action_.guard }, { "Dodge", action_.dodge }, { "DodgeDirection", static_cast<int>(action_.dodgeDirection) } });
    }
    snapshot_["GameMode"] = { { "Available", mode_ != nullptr }, { "BlackboardLockPresent", hasBlackboardLock_ },
        { "BlackboardEnemyAILocked", blackboardLocked_ } };
    if (mode_)
    {
        snapshot_["GameMode"].update({ { "State", mode_->GetCurrentGameStateName() }, { "BattlePhase", mode_->GetCurrentBattlePhase() },
            { "Sequence", mode_->GetCurrentSequenceName() }, { "EnemyAILocked", mode_->IsEnemyAILocked() },
            { "GameplayLocked", mode_->IsGameplayLocked() }, { "PlayerControlLocked", mode_->IsPlayerControlLocked() },
            { "PresentationPlaying", mode_->IsPresentationPlaying() }, { "BattlePaused", mode_->IsBattlePaused() },
            { "PlayerHpRate", mode_->GetPlayerHpRate() }, { "BossHpRate", mode_->GetBossHpRate() },
            { "LockMismatch", hasBlackboardLock_ && blackboardLocked_ != mode_->IsEnemyAILocked() },
            { "PreferDelayed", mode_->GetAIDirectorPreferDelayedAttack() }, { "PreferFeint", mode_->GetAIDirectorPreferFeintAttack() },
            { "PreferGuardBreak", mode_->GetAIDirectorPreferGuardBreak() }, { "ReduceAggression", mode_->GetAIDirectorReduceAggression() },
            { "AttackDelayRate", mode_->GetAIDirectorAttackDelayRate() }, { "FeintRate", mode_->GetAIDirectorFeintRate() },
            { "GuardBreakRate", mode_->GetAIDirectorGuardBreakRate() }, { "AggressionRate", mode_->GetAIDirectorAggressionRate() } });
    }
    snapshot_["ActionSettings"] = nlohmann::json::array();
    for (std::size_t index_ = 0; index_ < ai_.actions.size(); ++index_)
    {
        const auto& action_ = ai_.actions[index_];
        snapshot_["ActionSettings"].push_back({ { "Index", index_ }, { "Skill", action_.skill.name },
            { "Enabled", action_.enabled[ai_.phase] }, { "PhaseWeight", action_.weight[ai_.phase] },
            { "Cooldown", index_ < ai_.cooldowns.size() ? ai_.cooldowns[index_] : 0.0f },
            { "MinDistance", action_.minDistance }, { "MaxDistance", action_.maxDistance } });
    }
    snapshot_["Candidates"] = evaluatedWeights;
    snapshot_["EvaluatedNodes"] = evaluatedNodes;
    snapshot_["GraphEvaluationComplete"] = evaluationTime != 0 && !evaluating;
    snapshot_["GraphEvaluationAgeMs"] = evaluationTime == 0 ? -1 : static_cast<int>(SDL_GetTicks() - evaluationTime);
    entries.push_back(std::move(entry_));
    while (entries.size() > static_cast<std::size_t>(maxEntries))
    {
        entries.pop_front();
    }
    if (followLatest)
    {
        selectedEntryId = entries.back().id;
    }
}

void HumanBossAITraceComponent::DrawSnapshot(const nlohmann::json& snapshot_)
{
    const auto reason_ = snapshot_.value("Reason", std::string{});
    const auto suspension_ = snapshot_.value("Suspension", std::string{});
    ImGui::TextWrapped("Action: %s | Intent: %s", snapshot_.value("Action", std::string{}).c_str(), snapshot_.value("Intent", std::string{}).c_str());
    ImGui::TextWrapped("Decision: %s", reason_.c_str());
    if (!suspension_.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Suspended: %s", suspension_.c_str());
    }
    ImGui::TextWrapped("Threat: %s", snapshot_.value("ThreatReason", std::string{}).c_str());
    const auto& mode_ = snapshot_["GameMode"];
    ImGui::TextWrapped("GameMode: %s | Battle phase: %d | Sequence: %s", mode_.value("State", std::string("Unavailable")).c_str(),
        mode_.value("BattlePhase", 0), mode_.value("Sequence", std::string{}).c_str());
    ImGui::Text("AI lock: %d | GlobalBB lock: %d | Gameplay lock: %d | Presentation: %d | Paused: %d",
        mode_.value("EnemyAILocked", false), mode_.value("BlackboardEnemyAILocked", false), mode_.value("GameplayLocked", false),
        mode_.value("PresentationPlaying", false), mode_.value("BattlePaused", false));
    const auto& ai_ = snapshot_["AI"];
    ImGui::Text("AI phase %d | Distance %.2f | Reaction budget %.2f | Reaction delay %.2f | Wait %.2f",
        ai_.value("Phase", 0), ai_.value("Distance", 0.0f), ai_.value("ReactionBudget", 0.0f), ai_.value("ReactionDelay", 0.0f), ai_.value("WaitRemaining", 0.0f));
    if (mode_.value("Available", false) && !mode_.value("BlackboardLockPresent", false))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "GlobalBlackboard AI lock key is missing; normal AI defaults to unlocked.");
    }
    if (mode_.value("LockMismatch", false))
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "GameMode AI lock differs from the GlobalBlackboard consumed by normal AI.");
    }
    ImGui::TextWrapped("Normal AI reads GlobalBlackboard.EnemyAILocked; Berserk also checks presentation, gameplay and pause locks.");
    ImGui::TextWrapped("Candidate weights describe the actual evaluated branches. Unevaluated branches were not tested. Positive weight does not guarantee selection.");
    ImGui::TextWrapped("Candidate and route rows belong to the latest graph evaluation; GameMode or player state may change before the next graph tick.");
    if (ImGui::CollapsingHeader("Evaluated Action Candidates", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("Candidates", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Action");
            ImGui::TableSetupColumn("Runtime");
            ImGui::TableSetupColumn("Effective draw weight");
            ImGui::TableSetupColumn("Eligibility reason");
            ImGui::TableHeadersRow();
            for (const auto& candidate_ : snapshot_["Candidates"])
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(candidate_["Action"].get_ref<const std::string&>().c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", candidate_.value("RuntimeWeight", 0.0f));
                ImGui::TableSetColumnIndex(2);
                if (candidate_.contains("EffectiveWeight"))
                {
                    ImGui::Text("%.3f", candidate_["EffectiveWeight"].get<float>());
                }
                else
                {
                    ImGui::TextUnformatted("No individual draw weight");
                }
                ImGui::TableSetColumnIndex(3);
                ImGui::TextWrapped("%s", candidate_.value("Reason", std::string{}).c_str());
            }
            ImGui::EndTable();
        }
    }
    if (ImGui::CollapsingHeader("Player History Used by AI", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto& player_ = snapshot_["Player"];
        ImGui::Text("Attack %.2f | Heavy %.2f | Guard %.2f | Dodge %.2f | Same action %d",
            player_.value("AttackFrequency", 0.0f), player_.value("HeavyRate", 0.0f), player_.value("GuardFrequency", 0.0f),
            player_.value("DodgeFrequency", 0.0f), player_.value("SameActionCount", 0));
        ImGui::TextWrapped("Times below use the CombatObservation clock; trace times use time since Clear History.");
        if (ImGui::BeginTable("Player History", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            for (const char* column_ : { "Observation time", "Action", "Attack", "Guard", "Dodge", "Direction" })
            {
                ImGui::TableSetupColumn(column_);
            }
            ImGui::TableHeadersRow();
            constexpr const char* DIRECTIONS[] = { "None", "Left", "Right", "Back", "Forward" };
            for (const auto& action_ : snapshot_["PlayerHistory"])
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%.2f", action_.value("ObservationTime", 0.0f));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(action_.value("Action", std::string{}).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(action_.value("Heavy", false) ? "Heavy" : action_.value("Weak", false) ? "Weak" : action_.value("Attack", false) ? "Attack" : "-");
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(action_.value("Guard", false) ? "Yes" : "-");
                ImGui::TableSetColumnIndex(4);
                ImGui::TextUnformatted(action_.value("Dodge", false) ? "Yes" : "-");
                const int direction_ = action_.value("DodgeDirection", 0);
                ImGui::TableSetColumnIndex(5);
                ImGui::TextUnformatted(direction_ >= 0 && direction_ < 5 ? DIRECTIONS[direction_] : "Unknown");
            }
            ImGui::EndTable();
        }
    }
    if (ImGui::CollapsingHeader("Evaluated Conditions and Route", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Evaluation complete: %s | Snapshot captured %d ms after evaluation started",
            snapshot_.value("GraphEvaluationComplete", false) ? "Yes" : "No / action start in progress", snapshot_.value("GraphEvaluationAgeMs", -1));
        if (ImGui::BeginTable("Evaluation Route", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            for (const char* column_ : { "Node", "Result", "Source / Key", "Actual", "Comparison / Expected" })
            {
                ImGui::TableSetupColumn(column_);
            }
            ImGui::TableHeadersRow();
            for (const auto& node_ : snapshot_["EvaluatedNodes"])
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s", node_.value("Name", std::string{}).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(node_.value("Result", std::string{}).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextWrapped("%s %s", node_.value("Source", std::string{}).c_str(), node_.value("Key", std::string{}).c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(node_.contains("Actual") ? node_["Actual"].dump().c_str() : "-");
                ImGui::TableSetColumnIndex(4);
                ImGui::TextWrapped("%s %s", node_.value("Compare", std::string{}).c_str(), node_.contains("Expected") ? node_["Expected"].dump().c_str() : "");
            }
            ImGui::EndTable();
        }
    }
    if (ImGui::CollapsingHeader("Snapshot Details (GameMode / AI / Berserk / Settings)"))
    {
        DrawSnapshotFields(snapshot_);
    }
}

void HumanBossAITraceComponent::DrawHistory()
{
    BS_PROFILE_SCOPE("HumanBossAITraceComponent::DrawHistory");
    ImGui::Checkbox("Recording", &recordingEnabled);
    ImGui::SameLine();
    ImGui::Checkbox("Follow Latest", &followLatest);
    ImGui::SameLine();
    if (ImGui::Button("Clear History"))
    {
        ClearHistory();
    }
    ImGui::Text("Recorded: %d / %d (oldest entries are discarded)", static_cast<int>(entries.size()), maxEntries);
    historyFilter.Draw("Filter", 280.0f);
    if (followLatest && !entries.empty())
    {
        selectedEntryId = entries.back().id;
    }
    if (ImGui::BeginTable("Decision History", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
        ImVec2(0.0f, 240.0f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const char* column_ : { "Time", "Event", "Action", "Intent", "Player", "GameMode" })
        {
            ImGui::TableSetupColumn(column_);
        }
        ImGui::TableHeadersRow();
        for (const auto& entry_ : entries)
        {
            const auto& data_ = entry_.snapshot;
            const std::string action_ = data_.value("Action", std::string{});
            const std::string intent_ = data_.value("Intent", std::string{});
            const std::string player_ = data_["Player"].value("Action", std::string{});
            const std::string mode_ = data_["GameMode"].value("State", std::string("Unavailable"));
            const std::string search_ = entry_.event + " " + action_ + " " + intent_ + " " + player_ + " " + mode_ + " " + data_.value("Reason", std::string{});
            if (!historyFilter.PassFilter(search_.c_str()))
            {
                continue;
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string label_ = std::to_string(entry_.time) + "##" + std::to_string(entry_.id);
            if (ImGui::Selectable(label_.c_str(), selectedEntryId == entry_.id, ImGuiSelectableFlags_SpanAllColumns))
            {
                selectedEntryId = entry_.id;
                followLatest = false;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(entry_.event.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(action_.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(intent_.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(player_.c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(mode_.c_str());
        }
        if (followLatest)
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndTable();
    }
    for (const auto& entry_ : entries)
    {
        if (entry_.id == selectedEntryId)
        {
            DrawSnapshot(entry_.snapshot);
            return;
        }
    }
    ImGui::TextWrapped(entries.empty() ? "Waiting for AI observations. Attach this component to the HumanBoss object."
        : "Select a retained entry. The previously selected entry may have expired from the history.");
}

void HumanBossAITraceComponent::Gui()
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->GetComponent<HumanBossAIComponent>())
    {
        ImGui::TextWrapped("Attach this component to the same object as HumanBossAIComponent.");
    }
    ImGui::SliderInt("Maximum Entries", &maxEntries, 32, 1024);
    while (entries.size() > static_cast<std::size_t>(maxEntries))
    {
        entries.pop_front();
    }
    if (ImGui::Button("Open Trace Window"))
    {
        showWindow = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Trace JSON..."))
    {
        const std::string path_ = WindowsDialogHelper::SaveJsonDialog(L"Save Human Boss AI Trace", "Data/Debug/HumanBossAITrace.json");
        if (!path_.empty())
        {
            exportStatus = ExportTrace(path_) ? "Trace exported: " + path_ : "Trace export failed: " + path_;
        }
    }
    if (!exportStatus.empty())
    {
        ImGui::TextWrapped("%s", exportStatus.c_str());
    }
    DrawHistory();
}

void HumanBossAITraceComponent::RenderUI()
{
    BS_PROFILE_SCOPE("HumanBossAITraceComponent::RenderUI");
    if (!showWindow)
    {
        return;
    }
    const auto owner_ = GetGameObject();
    const std::string title_ = "Human Boss AI Trace##" + (owner_ ? owner_->GetName() : "Detached");
    ImGui::SetNextWindowSize(ImVec2(1100.0f, 760.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(title_.c_str(), &showWindow))
    {
        DrawHistory();
    }
    ImGui::End();
}

bool HumanBossAITraceComponent::ExportTrace(const std::string& path_) const
{
    BS_PROFILE_SCOPE("HumanBossAITraceComponent::ExportTrace");
    nlohmann::json output_ = { { "Version", 1 }, { "Entries", nlohmann::json::array() } };
    for (const auto& entry_ : entries)
    {
        output_["Entries"].push_back({ { "Id", entry_.id }, { "Time", entry_.time }, { "Event", entry_.event }, { "Snapshot", entry_.snapshot } });
    }
    try
    {
        const auto parent_ = std::filesystem::path(path_).parent_path();
        if (!parent_.empty())
        {
            std::filesystem::create_directories(parent_);
        }
        std::ofstream file_(path_, std::ios::binary);
        if (!file_)
        {
            return false;
        }
        file_ << output_.dump(2);
        return file_.good();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void HumanBossAITraceComponent::Serialize(nlohmann::json& json_) const
{
    json_["RecordingEnabled"] = recordingEnabled;
    json_["MaxEntries"] = maxEntries;
}

void HumanBossAITraceComponent::Deserialize(const nlohmann::json& json_)
{
    recordingEnabled = json_.value("RecordingEnabled", true);
    maxEntries = std::clamp(json_.value("MaxEntries", 256), 32, 1024);
    showWindow = false;
    followLatest = true;
    ClearHistory();
}
