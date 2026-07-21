#include "PrecompiledHeader.h"

#include "NodeGraphMonitorWindow.h"

#include "Engine/GamePlay/AI/AIControllerComponent.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationState.h"
#include "Game/AI/Dragon/DragonAIComponent.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    const ImVec2 NODE_SIZE(150.0f, 58.0f);

    const char* GetGraphTypeName(NODE_GRAPH_MONITOR_TYPE type_)
    {
        switch (type_)
        {
        case NODE_GRAPH_MONITOR_TYPE::AUTO: return "Auto";
        case NODE_GRAPH_MONITOR_TYPE::AI_BEHAVIOR: return "AI Behavior";
        case NODE_GRAPH_MONITOR_TYPE::DRAGON_AI: return "Dragon AI";
        case NODE_GRAPH_MONITOR_TYPE::ANIMATION: return "Animation";
        default: return "Auto";
        }
    }

    const char* GetAIBehaviorStatusName(AI_BEHAVIOR_STATUS status_)
    {
        switch (status_)
        {
        case AI_BEHAVIOR_STATUS::SUCCESS: return "Success";
        case AI_BEHAVIOR_STATUS::FAILURE: return "Failure";
        case AI_BEHAVIOR_STATUS::RUNNING: return "Running";
        default: return "Unknown";
        }
    }

    const char* GetAIBehaviorNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_)
    {
        switch (type_)
        {
        case AI_BEHAVIOR_NODE_TYPE::ROOT: return "Root";
        case AI_BEHAVIOR_NODE_TYPE::SELECTOR: return "Selector";
        case AI_BEHAVIOR_NODE_TYPE::SEQUENCE: return "Sequence";
        case AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR: return "Random Selector";
        case AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION: return "Blackboard Condition";
        case AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD: return "Set Blackboard";
        case AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION: return "Play Action";
        case AI_BEHAVIOR_NODE_TYPE::WAIT: return "Wait";
        case AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL: return "Wait Blackboard Bool";
        case AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH: return "Sub Graph";
        default: return "Unknown";
        }
    }

    const char* GetAnimationNodeTypeName(AnimGraphNodeType type_)
    {
        switch (type_)
        {
        case AnimGraphNodeType::State: return "State";
        case AnimGraphNodeType::Start: return "Start";
        case AnimGraphNodeType::Any: return "Any";
        case AnimGraphNodeType::SubGraph: return "Sub Graph";
        case AnimGraphNodeType::Entry: return "Entry";
        case AnimGraphNodeType::Exit: return "Exit";
        default: return "Unknown";
        }
    }

    bool HasAnyGraph(const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return false;
        return object_->GetComponent<AIControllerComponent>() != nullptr ||
            object_->GetComponent<DragonAIComponent>() != nullptr ||
            object_->GetComponent<AnimationControllerComponent>() != nullptr;
    }

    ImVec2 BuildCanvasPosition(const ImVec2& origin_, const ImVec2& graphPosition_, const ImVec2& offset_, float zoom_)
    {
        return ImVec2(
            origin_.x + (graphPosition_.x - offset_.x) * zoom_,
            origin_.y + (graphPosition_.y - offset_.y) * zoom_);
    }

    void DrawMonitorNode(
        ImDrawList* drawList_,
        const ImVec2& minimum_,
        const char* title_,
        const char* subtitle_,
        bool active_)
    {
        const ImVec2 maximum_(minimum_.x + NODE_SIZE.x, minimum_.y + NODE_SIZE.y);
        const ImU32 fillColor_ = active_
            ? IM_COL32(32, 96, 62, 255)
            : IM_COL32(50, 55, 64, 255);
        const ImU32 borderColor_ = active_
            ? IM_COL32(80, 255, 150, 255)
            : IM_COL32(120, 126, 138, 255);
        drawList_->AddRectFilled(minimum_, maximum_, fillColor_, 7.0f);
        drawList_->AddRect(minimum_, maximum_, borderColor_, 7.0f, 0, active_ ? 3.0f : 1.5f);
        drawList_->AddText(ImVec2(minimum_.x + 8.0f, minimum_.y + 8.0f), IM_COL32(245, 247, 250, 255), title_);
        drawList_->AddText(ImVec2(minimum_.x + 8.0f, minimum_.y + 32.0f), IM_COL32(185, 192, 202, 255), subtitle_);
    }

    void DrawArrowLine(ImDrawList* drawList_, const ImVec2& from_, const ImVec2& to_, ImU32 color_, float thickness_)
    {
        drawList_->AddLine(from_, to_, color_, thickness_);

        ImVec2 direction_(to_.x - from_.x, to_.y - from_.y);
        const float length_ = std::sqrt(direction_.x * direction_.x + direction_.y * direction_.y);
        if (length_ <= 0.001f) return;

        direction_.x /= length_;
        direction_.y /= length_;

        const ImVec2 center_((from_.x + to_.x) * 0.5f, (from_.y + to_.y) * 0.5f);
        const ImVec2 side_(-direction_.y, direction_.x);
        const ImVec2 tip_(center_.x + direction_.x * 8.0f, center_.y + direction_.y * 8.0f);
        const ImVec2 back_(center_.x - direction_.x * 8.0f, center_.y - direction_.y * 8.0f);
        drawList_->AddTriangleFilled(
            tip_,
            ImVec2(back_.x + side_.x * 5.0f, back_.y + side_.y * 5.0f),
            ImVec2(back_.x - side_.x * 5.0f, back_.y - side_.y * 5.0f),
            color_);
    }
}

NodeGraphMonitorWindow::NodeGraphMonitorWindow(GameObjectManager* manager_)
    : EditorWindow("Node Graph Monitor", manager_)
{
    isVisible = true;
    defaultWindowSize = ImVec2(520.0f, 460.0f);
    style.backgroundColor = ImVec4(0.07f, 0.075f, 0.085f, 0.95f);
    style.titleBarColor = ImVec4(0.16f, 0.24f, 0.28f, 1.0f);
}

void NodeGraphMonitorWindow::Draw()
{
    std::shared_ptr<GameObject> targetObject_ = ResolveTargetObject();

    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawTargetControls(targetObject_);
        ImGui::EndMenuBar();
    }

    DrawRuntimeSummary(targetObject_);
    ImGui::Separator();
    DrawGraphView(targetObject_);
    DrawStylePopup();

    EndEditorWindow();
}

std::shared_ptr<GameObject> NodeGraphMonitorWindow::ResolveTargetObject()
{
    if (!gameObjectManager) return nullptr;

    if (followSelectedObject)
    {
        const std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
        if (HasAnyGraph(selectedObject_))
        {
            targetObject = selectedObject_;
            return selectedObject_;
        }
    }

    std::shared_ptr<GameObject> currentTarget_ = targetObject.lock();
    if (HasAnyGraph(currentTarget_)) return currentTarget_;

    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!HasAnyGraph(object_)) continue;
        targetObject = object_;
        return object_;
    }

    return nullptr;
}

void NodeGraphMonitorWindow::DrawTargetControls(std::shared_ptr<GameObject>& targetObject_)
{
    if (!gameObjectManager) return;

    ImGui::Checkbox("Follow Selected", &followSelectedObject);
    ImGui::SameLine();

    int graphTypeIndex_ = static_cast<int>(graphType);
    ImGui::SetNextItemWidth(130.0f);
    if (ImGui::BeginCombo("Graph", GetGraphTypeName(graphType)))
    {
        for (int index_ = 0; index_ <= static_cast<int>(NODE_GRAPH_MONITOR_TYPE::ANIMATION); ++index_)
        {
            const NODE_GRAPH_MONITOR_TYPE type_ = static_cast<NODE_GRAPH_MONITOR_TYPE>(index_);
            const bool selected_ = graphTypeIndex_ == index_;
            if (ImGui::Selectable(GetGraphTypeName(type_), selected_))
            {
                graphType = type_;
                graphTypeIndex_ = index_;
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(followSelectedObject);
    ImGui::SetNextItemWidth(180.0f);
    const char* targetName_ = targetObject_ ? targetObject_->GetName().c_str() : "None";
    if (ImGui::BeginCombo("Target", targetName_))
    {
        for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
        {
            if (!HasAnyGraph(object_)) continue;
            const bool selected_ = object_ == targetObject_;
            if (ImGui::Selectable(object_->GetName().c_str(), selected_))
            {
                targetObject = object_;
                targetObject_ = object_;
            }
            if (selected_) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
}

void NodeGraphMonitorWindow::DrawRuntimeSummary(const std::shared_ptr<GameObject>& targetObject_)
{
    if (!targetObject_)
    {
        ImGui::TextDisabled("No graph target.");
        return;
    }

    ImGui::Text("Target: %s", targetObject_->GetName().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("Graph: %s", GetGraphTypeName(ResolveGraphType(targetObject_)));

    if (const std::shared_ptr<AnimationControllerComponent> animation_ =
        targetObject_->GetComponent<AnimationControllerComponent>())
    {
        const AnimState* state_ = animation_->GetCurrentState();
        ImGui::Text("Animation Runtime: %s", state_ ? state_->name.c_str() : "None");
    }

    if (const std::shared_ptr<DragonAIComponent> dragonAI_ =
        targetObject_->GetComponent<DragonAIComponent>())
    {
        ImGui::Text("Dragon Runtime: %s", GetDragonActionName(dragonAI_->GetCurrentAction()));
    }

    if (const std::shared_ptr<AIControllerComponent> aiController_ =
        targetObject_->GetComponent<AIControllerComponent>())
    {
        ImGui::Text("AI Graph Status: %s", GetAIBehaviorStatusName(aiController_->GetGraph().GetLastTickStatus()));
    }
}

void NodeGraphMonitorWindow::DrawGraphView(const std::shared_ptr<GameObject>& targetObject_)
{
    if (!targetObject_)
    {
        ImGui::TextDisabled("Select a GameObject with AnimationControllerComponent, AIControllerComponent, or DragonAIComponent.");
        return;
    }

    ImGui::SliderFloat("Zoom", &zoom, 0.25f, 1.2f);

    const NODE_GRAPH_MONITOR_TYPE resolvedType_ = ResolveGraphType(targetObject_);
    if (resolvedType_ == NODE_GRAPH_MONITOR_TYPE::AI_BEHAVIOR)
    {
        if (const std::shared_ptr<AIControllerComponent> aiController_ =
            targetObject_->GetComponent<AIControllerComponent>())
        {
            DrawAIBehaviorGraph(aiController_->GetGraph());
            return;
        }
    }
    if (resolvedType_ == NODE_GRAPH_MONITOR_TYPE::DRAGON_AI)
    {
        if (const std::shared_ptr<DragonAIComponent> dragonAI_ =
            targetObject_->GetComponent<DragonAIComponent>())
        {
            DrawAIBehaviorGraph(dragonAI_->GetBehaviorGraph());
            return;
        }
    }
    if (resolvedType_ == NODE_GRAPH_MONITOR_TYPE::ANIMATION)
    {
        DrawAnimationGraph(targetObject_);
        return;
    }

    ImGui::TextDisabled("Selected graph type is not available on this object.");
}

void NodeGraphMonitorWindow::DrawAIBehaviorGraph(AIBehaviorGraph& graph_)
{
    graph_.EnsureDefaultGraph();

    const ImVec2 canvasSize_ = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("AIBehaviorGraphMonitorCanvas", canvasSize_, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasOrigin_ = ImGui::GetCursorScreenPos();
    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    drawList_->AddRectFilled(canvasOrigin_, ImVec2(canvasOrigin_.x + canvasSize_.x, canvasOrigin_.y + canvasSize_.y), IM_COL32(25, 27, 32, 255));

    ImVec2 minimumPosition_(FLT_MAX, FLT_MAX);
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        minimumPosition_.x = (std::min)(minimumPosition_.x, node_.position.x);
        minimumPosition_.y = (std::min)(minimumPosition_.y, node_.position.y);
    }
    if (minimumPosition_.x == FLT_MAX) minimumPosition_ = ImVec2(0.0f, 0.0f);
    const ImVec2 offset_(minimumPosition_.x - 24.0f / zoom, minimumPosition_.y - 24.0f / zoom);

    std::unordered_map<uintptr_t, ImVec2> nodeCenters_;
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(
            canvasOrigin_,
            ImVec2(node_.position.x, node_.position.y),
            offset_,
            zoom);
        nodeCenters_[node_.id] = ImVec2(nodeMinimum_.x + NODE_SIZE.x * 0.5f, nodeMinimum_.y + NODE_SIZE.y * 0.5f);
    }

    for (const AIBehaviorLink& link_ : graph_.GetLinks())
    {
        const auto fromIt_ = nodeCenters_.find(link_.fromNodeId);
        const auto toIt_ = nodeCenters_.find(link_.toNodeId);
        if (fromIt_ == nodeCenters_.end() || toIt_ == nodeCenters_.end()) continue;
        const bool active_ = graph_.IsNodeRuntimeActive(link_.fromNodeId) && graph_.IsNodeRuntimeActive(link_.toNodeId);
        DrawArrowLine(drawList_, fromIt_->second, toIt_->second, active_ ? IM_COL32(80, 255, 150, 255) : IM_COL32(130, 136, 148, 255), active_ ? 3.0f : 1.5f);
    }

    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(
            canvasOrigin_,
            ImVec2(node_.position.x, node_.position.y),
            offset_,
            zoom);
        DrawMonitorNode(drawList_, nodeMinimum_, node_.name.c_str(), GetAIBehaviorNodeTypeName(node_.type), graph_.IsNodeRuntimeActive(node_.id));
    }

    ImGui::Dummy(canvasSize_);
    ImGui::EndChild();
}

void NodeGraphMonitorWindow::DrawAnimationGraph(const std::shared_ptr<GameObject>& targetObject_)
{
    const std::shared_ptr<AnimationControllerComponent> animation_ =
        targetObject_ ? targetObject_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (!animation_)
    {
        ImGui::TextDisabled("AnimationControllerComponent is not available.");
        return;
    }

    std::vector<AnimGraphNode>& nodes_ = animation_->GetGraphNodes();
    std::vector<AnimTransition>& transitions_ = animation_->GetTransitions();
    const AnimState* activeState_ = animation_->GetCurrentState();
    const std::string activeStateName_ = activeState_ ? activeState_->name : std::string{};

    const ImVec2 canvasSize_ = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("AnimationGraphMonitorCanvas", canvasSize_, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvasOrigin_ = ImGui::GetCursorScreenPos();
    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    drawList_->AddRectFilled(canvasOrigin_, ImVec2(canvasOrigin_.x + canvasSize_.x, canvasOrigin_.y + canvasSize_.y), IM_COL32(25, 27, 32, 255));

    ImVec2 minimumPosition_(FLT_MAX, FLT_MAX);
    for (const AnimGraphNode& node_ : nodes_)
    {
        minimumPosition_.x = (std::min)(minimumPosition_.x, node_.pos.x);
        minimumPosition_.y = (std::min)(minimumPosition_.y, node_.pos.y);
    }
    if (minimumPosition_.x == FLT_MAX) minimumPosition_ = ImVec2(0.0f, 0.0f);
    const ImVec2 offset_(minimumPosition_.x - 24.0f / zoom, minimumPosition_.y - 24.0f / zoom);

    std::unordered_map<std::string, ImVec2> nodeCenters_;
    for (const AnimGraphNode& node_ : nodes_)
    {
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(canvasOrigin_, node_.pos, offset_, zoom);
        nodeCenters_[node_.stateName] = ImVec2(nodeMinimum_.x + NODE_SIZE.x * 0.5f, nodeMinimum_.y + NODE_SIZE.y * 0.5f);
    }

    for (const AnimTransition& transition_ : transitions_)
    {
        const auto fromIt_ = nodeCenters_.find(transition_.fromStateName);
        const auto toIt_ = nodeCenters_.find(transition_.toStateName);
        if (fromIt_ == nodeCenters_.end() || toIt_ == nodeCenters_.end()) continue;
        const bool active_ = !activeStateName_.empty() &&
            (transition_.fromStateName == activeStateName_ || transition_.toStateName == activeStateName_);
        DrawArrowLine(drawList_, fromIt_->second, toIt_->second, active_ ? IM_COL32(90, 195, 255, 255) : IM_COL32(130, 136, 148, 255), active_ ? 3.0f : 1.5f);
    }

    for (const AnimGraphNode& node_ : nodes_)
    {
        const bool active_ = !activeStateName_.empty() && node_.stateName == activeStateName_;
        const ImVec2 nodeMinimum_ = BuildCanvasPosition(canvasOrigin_, node_.pos, offset_, zoom);
        DrawMonitorNode(drawList_, nodeMinimum_, node_.stateName.c_str(), GetAnimationNodeTypeName(node_.type), active_);
    }

    ImGui::Dummy(canvasSize_);
    ImGui::EndChild();
}

NODE_GRAPH_MONITOR_TYPE NodeGraphMonitorWindow::ResolveGraphType(const std::shared_ptr<GameObject>& targetObject_) const
{
    if (!targetObject_) return NODE_GRAPH_MONITOR_TYPE::AUTO;
    if (graphType != NODE_GRAPH_MONITOR_TYPE::AUTO) return graphType;

    if (targetObject_->GetComponent<DragonAIComponent>()) return NODE_GRAPH_MONITOR_TYPE::DRAGON_AI;
    if (targetObject_->GetComponent<AIControllerComponent>()) return NODE_GRAPH_MONITOR_TYPE::AI_BEHAVIOR;
    if (targetObject_->GetComponent<AnimationControllerComponent>()) return NODE_GRAPH_MONITOR_TYPE::ANIMATION;
    return NODE_GRAPH_MONITOR_TYPE::AUTO;
}
