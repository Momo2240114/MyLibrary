#include "PrecompiledHeader.h"

#include "AIBehaviorEditorWindow.h"

#include "Editor/AssetBrowserPayload.h"
#include "Engine/GamePlay/AI/AIControllerComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Game/AI/Dragon/DragonAIComponent.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace ed = ax::NodeEditor;

namespace
{
    constexpr float SPLITTER_SIZE = 6.0f;

    ImU32 GetNodeColor(AI_BEHAVIOR_NODE_TYPE type_)
    {
        switch (type_)
        {
        case AI_BEHAVIOR_NODE_TYPE::ROOT: return IM_COL32(90, 150, 235, 255);
        case AI_BEHAVIOR_NODE_TYPE::SELECTOR: return IM_COL32(120, 190, 240, 255);
        case AI_BEHAVIOR_NODE_TYPE::SEQUENCE: return IM_COL32(125, 215, 165, 255);
        case AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR: return IM_COL32(175, 150, 230, 255);
        case AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION: return IM_COL32(245, 205, 120, 255);
        case AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD: return IM_COL32(235, 170, 110, 255);
        case AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION: return IM_COL32(245, 125, 125, 255);
        case AI_BEHAVIOR_NODE_TYPE::WAIT: return IM_COL32(165, 165, 165, 255);
        case AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL: return IM_COL32(205, 180, 120, 255);
        case AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH: return IM_COL32(110, 205, 210, 255);
        default: return IM_COL32(210, 210, 210, 255);
        }
    }

    void DrawVerticalSplitter(const char* id_, float height_, float& width_, float direction_)
    {
        ImGui::InvisibleButton(id_, ImVec2(SPLITTER_SIZE, height_));
        const bool hovered_ = ImGui::IsItemHovered();
        const bool active_ = ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(hovered_ || active_ ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        if (hovered_ || active_) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (active_) width_ += ImGui::GetIO().MouseDelta.x * direction_;
    }

    bool IsCompositeNode(AI_BEHAVIOR_NODE_TYPE type_)
    {
        return type_ == AI_BEHAVIOR_NODE_TYPE::ROOT ||
            type_ == AI_BEHAVIOR_NODE_TYPE::SELECTOR ||
            type_ == AI_BEHAVIOR_NODE_TYPE::SEQUENCE ||
            type_ == AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR ||
            type_ == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH;
    }

    void DrawStraightLink(
        ImDrawList* drawList_,
        const ImVec2& startPosition_,
        const ImVec2& endPosition_,
        ImU32 color_,
        float thickness_ = 3.0f)
    {
        drawList_->AddLine(startPosition_, endPosition_, color_, thickness_);

        ImVec2 direction_(
            endPosition_.x - startPosition_.x,
            endPosition_.y - startPosition_.y);
        const float length_ = std::sqrt(direction_.x * direction_.x + direction_.y * direction_.y);
        if (length_ <= 0.001f) return;

        direction_.x /= length_;
        direction_.y /= length_;

        const ImVec2 center_(
            startPosition_.x + (endPosition_.x - startPosition_.x) * 0.5f,
            startPosition_.y + (endPosition_.y - startPosition_.y) * 0.5f);
        const ImVec2 perpendicular_(-direction_.y, direction_.x);
        constexpr float arrowLength_ = 14.0f;
        constexpr float arrowWidth_ = 8.0f;

        const ImVec2 tip_(
            center_.x + direction_.x * arrowLength_,
            center_.y + direction_.y * arrowLength_);
        const ImVec2 backCenter_(
            center_.x - direction_.x * arrowLength_,
            center_.y - direction_.y * arrowLength_);
        const ImVec2 left_(
            backCenter_.x + perpendicular_.x * arrowWidth_,
            backCenter_.y + perpendicular_.y * arrowWidth_);
        const ImVec2 right_(
            backCenter_.x - perpendicular_.x * arrowWidth_,
            backCenter_.y - perpendicular_.y * arrowWidth_);

        drawList_->AddTriangleFilled(tip_, left_, right_, IM_COL32(255, 210, 80, 255));
        drawList_->AddTriangle(tip_, left_, right_, IM_COL32(70, 45, 0, 255), 2.0f);
    }
}

AIBehaviorEditorWindow::AIBehaviorEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("AI Behavior Graph", manager_)
    , assetBrowser(manager_)
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.065f, 0.07f, 0.08f, 1.0f);
    style.titleBarColor = ImVec4(0.18f, 0.22f, 0.32f, 1.0f);

    ed::Config config_;
    config_.SettingsFile = nullptr;
    config_.DragButtonIndex = 0;
    config_.SelectButtonIndex = 0;
    config_.NavigateButtonIndex = 2;
    config_.ContextMenuButtonIndex = 1;
    nodeEditorContext = ed::CreateEditor(&config_);
}

AIBehaviorEditorWindow::~AIBehaviorEditorWindow()
{
    if (nodeEditorContext)
    {
        ed::DestroyEditor(nodeEditorContext);
        nodeEditorContext = nullptr;
    }
}

void AIBehaviorEditorWindow::Draw()
{
    BehaviorGraphTarget target_ = ResolveTarget();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawToolbar(target_);
        ImGui::EndMenuBar();
    }

    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    if (!layoutSizeInitialized)
    {
        assetBrowserHeight = available_.y * 0.33f;
        layoutSizeInitialized = true;
    }
    assetBrowserHeight = std::clamp(
        assetBrowserHeight,
        140.0f,
        (std::max)(140.0f, available_.y - 220.0f));
    const float topHeight_ = (std::max)(220.0f, available_.y - assetBrowserHeight - SPLITTER_SIZE);
    targetListWidth = std::clamp(targetListWidth, 180.0f, (std::max)(180.0f, available_.x * 0.35f));
    inspectorWidth = std::clamp(inspectorWidth, 260.0f, (std::max)(260.0f, available_.x * 0.40f));
    const float graphWidth_ = (std::max)(260.0f,
        available_.x - targetListWidth - inspectorWidth - SPLITTER_SIZE * 2.0f);

    ImGui::BeginChild("AIBehaviorTopArea", ImVec2(0.0f, topHeight_), false);

    ImGui::BeginChild("AIControllerList", ImVec2(targetListWidth, topHeight_), true);
    DrawTargetList();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##AIListSplitter", topHeight_, targetListWidth, 1.0f);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("AIBehaviorGraphPanel", ImVec2(graphWidth_, topHeight_), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawGraph(target_);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##AIInspectorSplitter", topHeight_, inspectorWidth, -1.0f);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("AIBehaviorInspector", ImVec2(inspectorWidth, topHeight_), true);
    DrawInspector(target_);
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::InvisibleButton("##AIAssetBrowserSplitter", ImVec2(-1.0f, SPLITTER_SIZE));
    const bool hovered_ = ImGui::IsItemHovered();
    const bool active_ = ImGui::IsItemActive();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(),
        ImGui::GetItemRectMax(),
        ImGui::GetColorU32(hovered_ || active_ ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
    if (hovered_ || active_) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (active_) assetBrowserHeight -= ImGui::GetIO().MouseDelta.y;

    ImGui::BeginChild("AIAssetBrowserBottom", ImVec2(0.0f, 0.0f), true);
    DrawAssetBrowserPanel();
    ImGui::EndChild();

    EndEditorWindow();
}

AIBehaviorEditorWindow::BehaviorGraphTarget AIBehaviorEditorWindow::ResolveTarget()
{
    BehaviorGraphTarget target_;
    if (!gameObjectManager) return target_;

    const std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
    if (selectedObject_)
    {
        const std::shared_ptr<AIControllerComponent> selectedAI_ =
            selectedObject_->GetComponent<AIControllerComponent>();
        if (selectedAI_)
        {
            targetObject = selectedObject_;
            target_.aiController = selectedAI_.get();
            target_.owner = selectedObject_.get();
            return target_;
        }

        const std::shared_ptr<DragonAIComponent> selectedDragonAI_ =
            selectedObject_->GetComponent<DragonAIComponent>();
        if (selectedDragonAI_)
        {
            targetObject = selectedObject_;
            target_.dragonAI = selectedDragonAI_.get();
            target_.owner = selectedObject_.get();
            return target_;
        }
    }

    const std::shared_ptr<GameObject> currentTarget_ = targetObject.lock();
    if (currentTarget_)
    {
        const std::shared_ptr<AIControllerComponent> currentAI_ =
            currentTarget_->GetComponent<AIControllerComponent>();
        if (currentAI_)
        {
            target_.aiController = currentAI_.get();
            target_.owner = currentTarget_.get();
            return target_;
        }

        const std::shared_ptr<DragonAIComponent> currentDragonAI_ =
            currentTarget_->GetComponent<DragonAIComponent>();
        if (currentDragonAI_)
        {
            target_.dragonAI = currentDragonAI_.get();
            target_.owner = currentTarget_.get();
            return target_;
        }
    }

    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!object_) continue;
        const std::shared_ptr<AIControllerComponent> aiController_ =
            object_->GetComponent<AIControllerComponent>();
        if (aiController_)
        {
            targetObject = object_;
            target_.aiController = aiController_.get();
            target_.owner = object_.get();
            return target_;
        }

        const std::shared_ptr<DragonAIComponent> dragonAI_ =
            object_->GetComponent<DragonAIComponent>();
        if (dragonAI_)
        {
            targetObject = object_;
            target_.dragonAI = dragonAI_.get();
            target_.owner = object_.get();
            return target_;
        }
    }

    return target_;
}

void AIBehaviorEditorWindow::DrawToolbar(const BehaviorGraphTarget& target_)
{
    const std::shared_ptr<GameObject> targetObject_ = targetObject.lock();
    ImGui::TextUnformatted("Target");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", targetObject_ ? targetObject_->GetName().c_str() : "None");
    ImGui::SameLine();

    if (target_.IsValid())
    {
        if (ImGui::Button("Reset Tree")) ResetGraphRuntime(target_);
        ImGui::SameLine();
        if (ImGui::Button("Focus")) restoreNodePositions = true;
    }
}

void AIBehaviorEditorWindow::DrawTargetList()
{
    ImGui::TextUnformatted("AI Behavior Graph Targets");
    ImGui::Separator();

    if (!gameObjectManager) return;

    const std::shared_ptr<GameObject> currentTarget_ = targetObject.lock();
    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!object_) continue;
        const bool hasAIController_ = object_->GetComponent<AIControllerComponent>() != nullptr;
        const bool hasDragonAI_ = object_->GetComponent<DragonAIComponent>() != nullptr;
        if (!hasAIController_ && !hasDragonAI_) continue;

        const bool selected_ = currentTarget_ == object_;
        const std::string label_ = object_->GetName() + (hasDragonAI_ ? " [Dragon]" : " [AI]");
        if (ImGui::Selectable(label_.c_str(), selected_))
        {
            targetObject = object_;
            selectedNodeId = 0;
            restoreNodePositions = true;
        }
    }

}

void AIBehaviorEditorWindow::DrawAssetBrowserPanel()
{
    ImGui::TextUnformatted("Asset Browser");
    ImGui::SameLine();
    ImGui::TextDisabled("Drop JSON graph assets onto Graph Asset field to load.");
    ImGui::Separator();
    assetBrowser.DrawContent();
}

void AIBehaviorEditorWindow::DrawGraph(const BehaviorGraphTarget& target_)
{
    AIBehaviorGraph* graphPointer_ = GetGraph(target_);
    if (!graphPointer_)
    {
        ImGui::TextDisabled("Select GameObject with AIControllerComponent or DragonAIComponent.");
        return;
    }

    AIBehaviorGraph& graph_ = *graphPointer_;
    graph_.EnsureDefaultGraph();
    activeActionName.clear();
    if (target_.dragonAI)
    {
        activeActionName = "DragonAI_" + std::string(GetDragonActionName(target_.dragonAI->GetCurrentAction()));
    }

    DrawGraphAssetBar(target_);
    ImGui::TextDisabled("Right-click the graph background to create nodes.");

    ed::SetCurrentEditor(nodeEditorContext);
    ed::PushStyleColor(ed::StyleColor_HovLinkBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ed::PushStyleColor(ed::StyleColor_SelLinkBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ed::PushStyleColor(ed::StyleColor_HighlightLinkBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ed::Begin("AI Behavior Workspace Graph", ImVec2(0.0f, 0.0f));

    pinPositions.clear();

    for (AIBehaviorNode& node_ : graph_.GetNodes())
    {
        DrawNode(node_);
    }

    for (const AIBehaviorLink& link_ : graph_.GetLinks())
    {
        const AIBehaviorNode* fromNode_ = graph_.FindNode(link_.fromNodeId);
        const AIBehaviorNode* toNode_ = graph_.FindNode(link_.toNodeId);
        const bool activeLink_ =
            (fromNode_ && IsRuntimeActiveNode(*fromNode_)) ||
            (toNode_ && IsRuntimeActiveNode(*toNode_));
        const ImU32 linkColor_ = activeLink_
            ? IM_COL32(80, 255, 150, 255)
            : IM_COL32(230, 230, 230, 255);
        const float linkThickness_ = activeLink_ ? 5.0f : 3.0f;

        ed::Link(
            ed::LinkId(link_.id),
            ed::PinId(link_.fromPinId),
            ed::PinId(link_.toPinId),
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
            1.0f);
        const auto fromPosition_ = pinPositions.find(link_.fromPinId);
        const auto toPosition_ = pinPositions.find(link_.toPinId);
        if (fromPosition_ != pinPositions.end() && toPosition_ != pinPositions.end())
        {
            DrawStraightLink(
                ImGui::GetWindowDrawList(),
                fromPosition_->second,
                toPosition_->second,
                linkColor_,
                linkThickness_);
        }
    }

    if (restoreNodePositions)
    {
        for (AIBehaviorNode& node_ : graph_.GetNodes())
        {
            ed::SetNodePosition(ed::NodeId(node_.id), ImVec2(node_.position.x, node_.position.y));
        }
        ed::NavigateToContent();
        restoreNodePositions = false;
    }

    if (nodeToPlaceId != 0)
    {
        ed::SetNodePosition(ed::NodeId(nodeToPlaceId), ImVec2(nextNodePosition.x, nextNodePosition.y));
        nodeToPlaceId = 0;
    }

    if (ed::BeginCreate())
    {
        ed::PinId startPinId_;
        ed::PinId endPinId_;
        if (ed::QueryNewLink(&startPinId_, &endPinId_))
        {
            bool startIsInput_ = false;
            bool endIsInput_ = false;
            uintptr_t startNodeId_ = 0;
            uintptr_t endNodeId_ = 0;
            const bool validStart_ = GetPinInfo(graph_, static_cast<uintptr_t>(startPinId_.Get()), startIsInput_, startNodeId_);
            const bool validEnd_ = GetPinInfo(graph_, static_cast<uintptr_t>(endPinId_.Get()), endIsInput_, endNodeId_);

            if (!validStart_ || !validEnd_ || startIsInput_ == endIsInput_)
            {
                ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
            }
            else
            {
                const uintptr_t fromNodeId_ = startIsInput_ ? endNodeId_ : startNodeId_;
                const uintptr_t toNodeId_ = startIsInput_ ? startNodeId_ : endNodeId_;
                AIBehaviorNode* fromNode_ = graph_.FindNode(fromNodeId_);
                AIBehaviorNode* toNode_ = graph_.FindNode(toNodeId_);
                const bool invalid_ = !fromNode_ || !toNode_ ||
                    fromNode_->outputPinId == 0 ||
                    toNode_->inputPinId == 0 ||
                    toNode_->type == AI_BEHAVIOR_NODE_TYPE::ROOT;

                if (invalid_)
                {
                    ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
                }
                else if (ed::AcceptNewItem(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), 2.0f))
                {
                    graph_.AddLink(fromNodeId_, toNodeId_);
                    graph_.ResetRuntime();
                }
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
        ed::LinkId linkId_;
        while (ed::QueryDeletedLink(&linkId_))
        {
            if (ed::AcceptDeletedItem())
            {
                graph_.RemoveLink(static_cast<uintptr_t>(linkId_.Get()));
            }
        }

        ed::NodeId nodeId_;
        while (ed::QueryDeletedNode(&nodeId_))
        {
            if (ed::AcceptDeletedItem())
            {
                graph_.RemoveNode(static_cast<uintptr_t>(nodeId_.Get()));
            }
        }
    }
    ed::EndDelete();

    ed::Suspend();
    if (ed::ShowBackgroundContextMenu())
    {
        ImGui::OpenPopup("Create AI Node");
        ImVec2 mouseCanvasPosition_ = ed::ScreenToCanvas(ImGui::GetMousePos());
        nextNodePosition = DirectX::XMFLOAT2(mouseCanvasPosition_.x, mouseCanvasPosition_.y);
    }
    ed::Resume();

    ed::Suspend();
    DrawCreateNodePopup(target_);
    ed::Resume();

    ed::NodeId selectedNodes_[1];
    if (ed::GetSelectedNodes(selectedNodes_, 1) > 0)
    {
        selectedNodeId = static_cast<uintptr_t>(selectedNodes_[0].Get());
    }

    for (AIBehaviorNode& node_ : graph_.GetNodes())
    {
        const ImVec2 position_ = ed::GetNodePosition(ed::NodeId(node_.id));
        node_.position = { position_.x, position_.y };
    }
    UpdatePriorityFromNodePositions(graph_);

    ed::End();
    ed::PopStyleColor(3);
    ed::SetCurrentEditor(nullptr);
}

void AIBehaviorEditorWindow::DrawGraphAssetBar(const BehaviorGraphTarget& target_)
{
    std::string* graphAssetPathPointer_ = GetGraphAssetPath(target_);
    if (!graphAssetPathPointer_) return;

    std::string& graphAssetPath_ = *graphAssetPathPointer_;
    ImGui::SetNextItemWidth(340.0f);
    ImGui::InputText("Graph Asset", &graphAssetPath_);
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload(AssetBrowserPayload::JSON))
        {
            const char* path_ = static_cast<const char*>(payload_->Data);
            if (path_ && path_[0] != '\0')
            {
                graphAssetStatus = LoadGraphAsset(target_, path_)
                    ? "Loaded Graph: " + std::string(path_)
                    : "Failed to load Graph: " + std::string(path_);
                restoreNodePositions = true;
                selectedNodeId = 0;
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        const std::string selectedPath_ = WindowsDialogHelper::OpenJsonDialog(
            L"Open AI Behavior Graph",
            "Data/AI/BehaviorGraph");
        if (!selectedPath_.empty())
        {
            graphAssetStatus = LoadGraphAsset(target_, selectedPath_)
                ? "Loaded Graph: " + selectedPath_
                : "Failed to load Graph: " + selectedPath_;
            restoreNodePositions = true;
            selectedNodeId = 0;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        graphAssetStatus = LoadGraphAsset(target_, graphAssetPath_)
            ? "Loaded Graph: " + graphAssetPath_
            : "Failed to load Graph: " + graphAssetPath_;
        restoreNodePositions = true;
        selectedNodeId = 0;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        graphAssetStatus = SaveGraphAsset(target_, graphAssetPath_)
            ? "Saved Graph: " + graphAssetPath_
            : "Failed to save Graph: " + graphAssetPath_;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save As..."))
    {
        const std::string selectedPath_ = WindowsDialogHelper::SaveJsonDialog(
            L"Save AI Behavior Graph",
            graphAssetPath_.empty() ? "Data/AI/BehaviorGraph/AIBehaviorGraph.json" : graphAssetPath_);
        if (!selectedPath_.empty())
        {
            graphAssetPath_ = selectedPath_;
            graphAssetStatus = SaveGraphAsset(target_, selectedPath_)
                ? "Saved Graph: " + selectedPath_
                : "Failed to save Graph: " + selectedPath_;
        }
    }

    if (!graphAssetStatus.empty())
    {
        ImGui::TextDisabled("%s", graphAssetStatus.c_str());
    }
}

void AIBehaviorEditorWindow::DrawInspector(const BehaviorGraphTarget& target_)
{
    AIBehaviorGraph* graphPointer_ = GetGraph(target_);
    if (!graphPointer_)
    {
        ImGui::TextDisabled("No AI behavior graph target.");
        return;
    }

    AIBehaviorNode* node_ = graphPointer_->FindNode(selectedNodeId);
    if (!node_)
    {
        ImGui::TextDisabled("Select node.");
        return;
    }

    ImGui::Text("Type: %s", GetNodeTypeName(node_->type));
    ImGui::InputText("Name", &node_->name);
    ImGui::InputTextMultiline("Comment", &node_->comment, ImVec2(0.0f, 70.0f));
    ImGui::InputInt("Priority", &node_->priority);
    ImGui::TextDisabled("Priority is also updated from child node X position. Left node = higher priority.");
    ImGui::DragFloat("Weight", &node_->weight, 0.05f, 0.0f, 100.0f);

    if (node_->type == AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION ||
        node_->type == AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD ||
        node_->type == AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL)
    {
        ImGui::SeparatorText("Blackboard");
        const char* scopeNames_[] = { "Local", "Global" };
        int blackboardScope_ = static_cast<int>(node_->blackboardScope);
        if (ImGui::Combo("Scope", &blackboardScope_, scopeNames_, IM_ARRAYSIZE(scopeNames_)))
        {
            node_->blackboardScope = static_cast<AI_BLACKBOARD_SCOPE>(blackboardScope_);
        }
        ImGui::InputText("Category", &node_->blackboardCategory);
        ImGui::InputText("Key", &node_->blackboardKey);

        const char* valueTypeNames_[] = { "Float", "Int", "Bool", "Vector2", "Vector3", "String", "Trigger" };
        int valueType_ = static_cast<int>(node_->valueType);
        if (ImGui::Combo("Value Type", &valueType_, valueTypeNames_, IM_ARRAYSIZE(valueTypeNames_)))
        {
            node_->valueType = static_cast<AI_BLACKBOARD_VALUE_TYPE>(valueType_);
        }

        if (node_->type == AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION)
        {
            const char* compareNames_[] = { "Exists", "Not Exists", "Equal", "Not Equal", "Greater", "Greater Equal", "Less", "Less Equal" };
            int compareType_ = static_cast<int>(node_->compareType);
            if (ImGui::Combo("Compare", &compareType_, compareNames_, IM_ARRAYSIZE(compareNames_)))
            {
                node_->compareType = static_cast<AI_BLACKBOARD_COMPARE_TYPE>(compareType_);
            }
        }

        if (node_->valueType == AI_BLACKBOARD_VALUE_TYPE::FLOAT) ImGui::DragFloat("Float Value", &node_->floatValue, 0.05f);
        if (node_->valueType == AI_BLACKBOARD_VALUE_TYPE::INT) ImGui::InputInt("Int Value", &node_->intValue);
        if (node_->valueType == AI_BLACKBOARD_VALUE_TYPE::BOOL) ImGui::Checkbox("Bool Value", &node_->boolValue);
        if (node_->valueType == AI_BLACKBOARD_VALUE_TYPE::VECTOR2) ImGui::DragFloat2("Vector2 Value", &node_->vector2Value.x, 0.05f);
        if (node_->valueType == AI_BLACKBOARD_VALUE_TYPE::VECTOR3) ImGui::DragFloat3("Vector3 Value", &node_->vector3Value.x, 0.05f);
        if (node_->valueType == AI_BLACKBOARD_VALUE_TYPE::STRING) ImGui::InputText("String Value", &node_->stringValue);
    }

    if (node_->type == AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION)
    {
        ImGui::SeparatorText("Action");
        ImGui::InputText("Action Name", &node_->actionName);
        if (target_.dragonAI)
        {
            if (ImGui::Button("DragonAI_Bite")) node_->actionName = "DragonAI_Bite";
            ImGui::SameLine();
            if (ImGui::Button("DragonAI_FireBall")) node_->actionName = "DragonAI_FireBall";
            if (ImGui::Button("DragonAI_SpreadBreath")) node_->actionName = "DragonAI_SpreadBreath";
            ImGui::SameLine();
            if (ImGui::Button("DragonAI_TailSweep")) node_->actionName = "DragonAI_TurnTailSweep";
            if (ImGui::Button("DragonAI_TakeOff")) node_->actionName = "DragonAI_TakeOff";
            ImGui::SameLine();
            if (ImGui::Button("DragonAI_Walk")) node_->actionName = "DragonAI_WalkApproach";
        }
        else
        {
            if (ImGui::Button("AI.ChaseTarget")) node_->actionName = "AI.ChaseTarget";
            ImGui::SameLine();
            if (ImGui::Button("AI.ReturnToSpawn")) node_->actionName = "AI.ReturnToSpawn";
            if (ImGui::Button("AI.StopMove")) node_->actionName = "AI.StopMove";
            ImGui::SameLine();
            if (ImGui::Button("AI.Attack")) node_->actionName = "AI.Attack";
            ImGui::SameLine();
            if (ImGui::Button("AI.NotifyActionEnd")) node_->actionName = "AI.NotifyActionEnd";
        }
        ImGui::Checkbox("Wait For Action End", &node_->waitForActionEnd);
        ImGui::InputText("ActionEnd Category", &node_->actionEndCategory);
        ImGui::InputText("ActionEnd Key", &node_->actionEndKey);
    }

    if (node_->type == AI_BEHAVIOR_NODE_TYPE::WAIT)
    {
        ImGui::DragFloat("Wait Seconds", &node_->waitSeconds, 0.05f, 0.0f, 999.0f);
    }

    if (node_->type == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH)
    {
        ImGui::InputText("SubGraph Name", &node_->subGraphName);
    }
}

void AIBehaviorEditorWindow::DrawNode(AIBehaviorNode& node_)
{
    BS_PROFILE_SCOPE("AIBehaviorEditorWindow::DrawNode");

    const bool selected_ = selectedNodeId == node_.id;
    const bool runtimeActive_ = IsRuntimeActiveNode(node_);
    const ImU32 categoryColor_ = runtimeActive_ ? IM_COL32(28, 96, 62, 255) : GetNodeColor(node_.type);
    const ImU32 bodyColor_ = IM_COL32(248, 250, 252, 255);
    const ImU32 borderColor_ = runtimeActive_
        ? IM_COL32(80, 255, 150, 255)
        : (selected_ ? IM_COL32(255, 245, 120, 255) : IM_COL32(35, 35, 35, 255));
    const ImU32 categoryTextColor_ = IM_COL32(20, 24, 28, 255);
    const ImU32 nameTextColor_ = IM_COL32(28, 32, 36, 255);
    constexpr float nodeWidth_ = 190.0f;
    constexpr float nodeHeight_ = 92.0f;
    constexpr float categoryHeight_ = 28.0f;
    constexpr float portWidth_ = 86.0f;
    constexpr float portHeight_ = 24.0f;
    const ImVec2 portSize_(portWidth_, portHeight_);

    ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImGui::ColorConvertU32ToFloat4(borderColor_));

    ed::BeginNode(ed::NodeId(node_.id));
    ImGui::PushID(static_cast<int>(node_.id));

    const ImVec2 nodeMin_ = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(nodeWidth_, nodeHeight_));
    const ImVec2 nodeMax_(nodeMin_.x + nodeWidth_, nodeMin_.y + nodeHeight_);
    ImDrawList* drawList_ = ImGui::GetWindowDrawList();

    drawList_->AddRectFilled(nodeMin_, nodeMax_, bodyColor_, 10.0f);
    drawList_->AddRectFilled(
        nodeMin_,
        ImVec2(nodeMax_.x, nodeMin_.y + categoryHeight_),
        categoryColor_,
        10.0f,
        ImDrawFlags_RoundCornersTop);
    drawList_->AddRect(nodeMin_, nodeMax_, borderColor_, 10.0f, 0, runtimeActive_ ? 4.0f : (selected_ ? 3.0f : 2.0f));
    drawList_->AddLine(
        ImVec2(nodeMin_.x, nodeMin_.y + categoryHeight_),
        ImVec2(nodeMax_.x, nodeMin_.y + categoryHeight_),
        IM_COL32(35, 35, 35, 210),
        1.5f);

    const char* typeName_ = GetNodeTypeName(node_.type);
    const ImVec2 typeSize_ = ImGui::CalcTextSize(typeName_);
    ImGui::SetCursorScreenPos(ImVec2(
        nodeMin_.x + (nodeWidth_ - typeSize_.x) * 0.5f,
        nodeMin_.y + 7.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, categoryTextColor_);
    ImGui::Text("%s", typeName_);
    ImGui::PopStyleColor();

    const ImVec2 titleSize_ = ImGui::CalcTextSize(node_.name.c_str());
    ImGui::SetCursorScreenPos(ImVec2(
        nodeMin_.x + (nodeWidth_ - titleSize_.x) * 0.5f,
        nodeMin_.y + 48.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, nameTextColor_);
    ImGui::Text("%s", node_.name.c_str());
    ImGui::PopStyleColor();

    if (node_.inputPinId != 0)
    {
        const ImVec2 inputMin_(
            nodeMin_.x + (nodeWidth_ - portWidth_) * 0.5f,
            nodeMin_.y - portHeight_ * 0.5f);
        const ImVec2 inputMax_(
            inputMin_.x + portWidth_,
            inputMin_.y + portHeight_);
        const ImVec2 inputCenter_(
            (inputMin_.x + inputMax_.x) * 0.5f,
            (inputMin_.y + inputMax_.y) * 0.5f);
        ImGui::SetCursorScreenPos(inputMin_);
        ed::BeginPin(ed::PinId(node_.inputPinId), ed::PinKind::Input);
        ImGui::Dummy(portSize_);
        ed::EndPin();
        drawList_->AddRectFilled(inputMin_, inputMax_, IM_COL32(235, 220, 0, 255), 5.0f);
        drawList_->AddRect(inputMin_, inputMax_, IM_COL32(45, 45, 20, 255), 5.0f, 0, 2.0f);
        const ImVec2 inTextSize_ = ImGui::CalcTextSize("IN");
        drawList_->AddText(
            ImVec2(inputCenter_.x - inTextSize_.x * 0.5f, inputCenter_.y - inTextSize_.y * 0.5f),
            IM_COL32(20, 20, 20, 255),
            "IN");
        pinPositions[node_.inputPinId] = inputCenter_;
    }

    if (node_.outputPinId != 0)
    {
        const ImVec2 outputMin_(
            nodeMin_.x + (nodeWidth_ - portWidth_) * 0.5f,
            nodeMax_.y - portHeight_ * 0.5f);
        const ImVec2 outputMax_(
            outputMin_.x + portWidth_,
            outputMin_.y + portHeight_);
        const ImVec2 outputCenter_(
            (outputMin_.x + outputMax_.x) * 0.5f,
            (outputMin_.y + outputMax_.y) * 0.5f);
        ImGui::SetCursorScreenPos(outputMin_);
        ed::BeginPin(ed::PinId(node_.outputPinId), ed::PinKind::Output);
        ImGui::Dummy(portSize_);
        ed::EndPin();
        drawList_->AddRectFilled(outputMin_, outputMax_, IM_COL32(230, 45, 55, 255), 5.0f);
        drawList_->AddRect(outputMin_, outputMax_, IM_COL32(60, 10, 10, 255), 5.0f, 0, 2.0f);
        const ImVec2 outTextSize_ = ImGui::CalcTextSize("OUT");
        drawList_->AddText(
            ImVec2(outputCenter_.x - outTextSize_.x * 0.5f, outputCenter_.y - outTextSize_.y * 0.5f),
            IM_COL32(255, 245, 245, 255),
            "OUT");
        pinPositions[node_.outputPinId] = outputCenter_;
    }

    ImGui::SetCursorScreenPos(nodeMax_);
    ImGui::PopID();
    ed::EndNode();
    ed::PopStyleColor(2);
    ed::PopStyleVar(2);
}

bool AIBehaviorEditorWindow::IsRuntimeActiveNode(const AIBehaviorNode& node_) const
{
    const std::shared_ptr<GameObject> targetObject_ = targetObject.lock();
    if (targetObject_)
    {
        if (const std::shared_ptr<AIControllerComponent> aiController_ =
            targetObject_->GetComponent<AIControllerComponent>())
        {
            if (aiController_->GetGraph().IsNodeRuntimeActive(node_.id)) return true;
        }

        if (const std::shared_ptr<DragonAIComponent> dragonAI_ =
            targetObject_->GetComponent<DragonAIComponent>())
        {
            if (dragonAI_->GetBehaviorGraph().IsNodeRuntimeActive(node_.id)) return true;
        }
    }

    return !activeActionName.empty() &&
        node_.type == AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION &&
        node_.actionName == activeActionName;
}

void AIBehaviorEditorWindow::AddNode(
    const BehaviorGraphTarget& target_,
    AI_BEHAVIOR_NODE_TYPE type_,
    const char* name_)
{
    AIBehaviorGraph* graphPointer_ = GetGraph(target_);
    if (!graphPointer_) return;

    AIBehaviorNode* node_ = graphPointer_->AddNode(type_, name_, nextNodePosition);
    if (!node_) return;

    nodeToPlaceId = node_->id;
    selectedNodeId = node_->id;
    nextNodePosition.x += 40.0f;
    nextNodePosition.y += 40.0f;
}

void AIBehaviorEditorWindow::DrawCreateNodePopup(const BehaviorGraphTarget& target_)
{
    if (!ImGui::BeginPopup("Create AI Node")) return;

    ImGui::TextUnformatted("Create AI Node");
    ImGui::Separator();

    if (ImGui::MenuItem("Selector")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::SELECTOR, "Selector");
    if (ImGui::MenuItem("Sequence")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::SEQUENCE, "Sequence");
    if (ImGui::MenuItem("Random Selector")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR, "Random");
    ImGui::Separator();
    if (ImGui::MenuItem("Blackboard Condition")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION, "Condition");
    if (ImGui::MenuItem("Set Blackboard")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD, "Set Blackboard");
    if (ImGui::MenuItem("Wait Blackboard Bool")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL, "Wait Bool");
    ImGui::Separator();
    if (ImGui::MenuItem("Play Action")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION, "Action");
    if (ImGui::MenuItem("Wait")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::WAIT, "Wait");
    if (ImGui::MenuItem("SubGraph")) RequestAddNode(target_, AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH, "SubGraph");

    ImGui::EndPopup();
}

void AIBehaviorEditorWindow::RequestAddNode(
    const BehaviorGraphTarget& target_,
    AI_BEHAVIOR_NODE_TYPE type_,
    const char* name_)
{
    AddNode(target_, type_, name_);
    ImGui::CloseCurrentPopup();
}

void AIBehaviorEditorWindow::UpdatePriorityFromNodePositions(AIBehaviorGraph& graph_)
{
    for (AIBehaviorNode& parentNode_ : graph_.GetNodes())
    {
        if (!IsCompositeNode(parentNode_.type)) continue;

        std::vector<AIBehaviorNode*> children_;
        for (const AIBehaviorLink& link_ : graph_.GetLinks())
        {
            if (link_.fromNodeId != parentNode_.id) continue;
            AIBehaviorNode* childNode_ = graph_.FindNode(link_.toNodeId);
            if (childNode_) children_.push_back(childNode_);
        }

        std::sort(children_.begin(), children_.end(),
            [](const AIBehaviorNode* left_, const AIBehaviorNode* right_)
            {
                if (std::fabs(left_->position.x - right_->position.x) > 0.001f)
                {
                    return left_->position.x < right_->position.x;
                }
                return left_->id < right_->id;
            });

        for (int index_ = 0; index_ < static_cast<int>(children_.size()); ++index_)
        {
            children_[index_]->priority = index_;
        }
    }
}

bool AIBehaviorEditorWindow::GetPinInfo(
    const AIBehaviorGraph& graph_,
    uintptr_t pinId_,
    bool& isInput_,
    uintptr_t& nodeId_) const
{
    for (const AIBehaviorNode& node_ : graph_.GetNodes())
    {
        if (node_.inputPinId == pinId_)
        {
            isInput_ = true;
            nodeId_ = node_.id;
            return true;
        }
        if (node_.outputPinId == pinId_)
        {
            isInput_ = false;
            nodeId_ = node_.id;
            return true;
        }
    }
    return false;
}

AIBehaviorGraph* AIBehaviorEditorWindow::GetGraph(const BehaviorGraphTarget& target_) const
{
    if (target_.aiController) return &target_.aiController->GetGraph();
    if (target_.dragonAI) return &target_.dragonAI->GetBehaviorGraph();
    return nullptr;
}

std::string* AIBehaviorEditorWindow::GetGraphAssetPath(const BehaviorGraphTarget& target_) const
{
    if (target_.aiController) return &target_.aiController->GetGraphAssetPath();
    if (target_.dragonAI) return &target_.dragonAI->GetBehaviorGraphAssetPath();
    return nullptr;
}

void AIBehaviorEditorWindow::ResetGraphRuntime(const BehaviorGraphTarget& target_) const
{
    if (target_.aiController)
    {
        target_.aiController->ResetTree();
        return;
    }
    if (target_.dragonAI)
    {
        target_.dragonAI->ResetBehaviorGraphRuntime();
    }
}

bool AIBehaviorEditorWindow::SaveGraphAsset(
    const BehaviorGraphTarget& target_,
    const std::string& path_) const
{
    if (target_.aiController) return target_.aiController->SaveGraphAsset(path_);
    if (target_.dragonAI) return target_.dragonAI->SaveBehaviorGraphAsset(path_);
    return false;
}

bool AIBehaviorEditorWindow::LoadGraphAsset(
    const BehaviorGraphTarget& target_,
    const std::string& path_) const
{
    if (target_.aiController) return target_.aiController->LoadGraphAsset(path_);
    if (target_.dragonAI) return target_.dragonAI->LoadBehaviorGraphAsset(path_);
    return false;
}

const char* AIBehaviorEditorWindow::GetNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_) const
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
