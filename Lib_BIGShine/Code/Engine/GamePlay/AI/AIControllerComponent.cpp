#include "AIControllerComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "imgui_node_editor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace ed = ax::NodeEditor;

namespace
{
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

}

AIControllerComponent::~AIControllerComponent()
{
    if (nodeEditorContext)
    {
        ed::DestroyEditor(nodeEditorContext);
        nodeEditorContext = nullptr;
    }
}

void AIControllerComponent::Start()
{
    EnsureBlackboard();
    graph.EnsureDefaultGraph();
}

void AIControllerComponent::Update(float deltaTime_)
{
    if (!isRunning) return;

    EnsureBlackboard();
    if (!blackboardComponent) return;

    AIBehaviorContext context_;
    context_.owner = GetGameObject().get();
    context_.blackboard = blackboardComponent;
    context_.deltaTime = deltaTime_;
    graph.Tick(context_);
}

void AIControllerComponent::Gui()
{
    EnsureBlackboard();
    EnsureNodeEditorContext();

    ImGui::Checkbox("Run AI", &isRunning);
    ImGui::SameLine();
    if (ImGui::Button("Reset Tree")) ResetTree();
    ImGui::Checkbox("Show Node Editor", &showNodeEditor);

    ImGui::InputText("Graph Asset", &graphAssetPath);
    if (ImGui::Button("Save Graph"))
    {
        statusText = SaveGraphAsset(graphAssetPath) ? "Saved" : "Save failed";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Graph"))
    {
        statusText = LoadGraphAsset(graphAssetPath) ? "Loaded" : "Load failed";
    }
    if (!statusText.empty()) ImGui::Text("%s", statusText.c_str());

    if (showNodeEditor)
    {
        DrawNodeEditor();
    }

    DrawInspector();
}

void AIControllerComponent::Serialize(nlohmann::json& json_) const
{
    json_["IsRunning"] = isRunning;
    json_["GraphAssetPath"] = graphAssetPath;
    graph.Serialize(json_["Graph"]);
}

void AIControllerComponent::Deserialize(const nlohmann::json& json_)
{
    isRunning = json_.value("IsRunning", true);
    graphAssetPath = json_.value("GraphAssetPath", std::string("Data/AI/BehaviorGraph/DefaultAI.json"));

    if (json_.contains("Graph"))
    {
        graph.Deserialize(json_["Graph"]);
    }
    else if (!graphAssetPath.empty())
    {
        graph.LoadFromFile(graphAssetPath);
    }

    restoreNodePositions = true;
}

void AIControllerComponent::Play()
{
    isRunning = true;
}

void AIControllerComponent::Stop()
{
    isRunning = false;
}

void AIControllerComponent::ResetTree()
{
    graph.ResetRuntime();
}

bool AIControllerComponent::SaveGraphAsset(const std::string& path_) const
{
    return graph.SaveToFile(path_);
}

bool AIControllerComponent::LoadGraphAsset(const std::string& path_)
{
    const bool result_ = graph.LoadFromFile(path_);
    if (result_)
    {
        graphAssetPath = path_;
        restoreNodePositions = true;
        selectedNodeId = 0;
    }
    return result_;
}

void AIControllerComponent::EnsureBlackboard()
{
    std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    auto blackboard_ = owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_)
    {
        blackboard_ = owner_->AddComponent<BlackboardComponent>();
    }
    blackboardComponent = blackboard_.get();
}

void AIControllerComponent::EnsureNodeEditorContext()
{
    if (nodeEditorContext) return;

    ed::Config config_;
    nodeEditorContext = ed::CreateEditor(&config_);
}

void AIControllerComponent::DrawNodeEditor()
{
    ed::SetCurrentEditor(nodeEditorContext);

    if (ImGui::Button("Add Selector")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::SELECTOR, "Selector");
    ImGui::SameLine();
    if (ImGui::Button("Add Sequence")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::SEQUENCE, "Sequence");
    ImGui::SameLine();
    if (ImGui::Button("Add Random")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR, "Random");
    ImGui::SameLine();
    if (ImGui::Button("Add Condition")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION, "Condition");
    ImGui::SameLine();
    if (ImGui::Button("Add SetBB")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD, "Set Blackboard");
    ImGui::SameLine();
    if (ImGui::Button("Add Action")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION, "Action");
    ImGui::SameLine();
    if (ImGui::Button("Add Wait")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::WAIT, "Wait");
    ImGui::SameLine();
    if (ImGui::Button("Add WaitBool")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL, "Wait Bool");
    ImGui::SameLine();
    if (ImGui::Button("Add SubGraph")) AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH, "SubGraph");

    ed::Begin("AI Behavior Graph", ImVec2(0.0f, 520.0f));

    for (AIBehaviorNode& node_ : graph.GetNodes())
    {
        DrawNode(node_);
    }

    for (const AIBehaviorLink& link_ : graph.GetLinks())
    {
        ed::Link(
            ed::LinkId(link_.id),
            ed::PinId(link_.fromPinId),
            ed::PinId(link_.toPinId),
            ImVec4(0.9f, 0.9f, 0.9f, 1.0f),
            3.0f);
    }

    if (restoreNodePositions)
    {
        for (AIBehaviorNode& node_ : graph.GetNodes())
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

            const bool validStart_ = GetPinInfo(static_cast<uintptr_t>(startPinId_.Get()), startIsInput_, startNodeId_);
            const bool validEnd_ = GetPinInfo(static_cast<uintptr_t>(endPinId_.Get()), endIsInput_, endNodeId_);

            if (!validStart_ || !validEnd_ || startIsInput_ == endIsInput_)
            {
                ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
            }
            else
            {
                const uintptr_t fromNodeId_ = startIsInput_ ? endNodeId_ : startNodeId_;
                const uintptr_t toNodeId_ = startIsInput_ ? startNodeId_ : endNodeId_;
                AIBehaviorNode* fromNode_ = graph.FindNode(fromNodeId_);
                AIBehaviorNode* toNode_ = graph.FindNode(toNodeId_);
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
                    graph.AddLink(fromNodeId_, toNodeId_);
                    graph.ResetRuntime();
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
                graph.RemoveLink(static_cast<uintptr_t>(linkId_.Get()));
            }
        }

        ed::NodeId nodeId_;
        while (ed::QueryDeletedNode(&nodeId_))
        {
            if (ed::AcceptDeletedItem())
            {
                graph.RemoveNode(static_cast<uintptr_t>(nodeId_.Get()));
            }
        }
    }
    ed::EndDelete();

    ed::NodeId selectedNodes_[1];
    if (ed::GetSelectedNodes(selectedNodes_, 1) > 0)
    {
        selectedNodeId = static_cast<uintptr_t>(selectedNodes_[0].Get());
    }

    for (AIBehaviorNode& node_ : graph.GetNodes())
    {
        const ImVec2 position_ = ed::GetNodePosition(ed::NodeId(node_.id));
        node_.position = { position_.x, position_.y };
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);
}

void AIControllerComponent::DrawInspector()
{
    AIBehaviorNode* node_ = graph.FindNode(selectedNodeId);
    if (!node_) return;

    if (!ImGui::CollapsingHeader("AI Node Inspector", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::Text("Type: %s", GetNodeTypeName(node_->type));
    ImGui::InputText("Name", &node_->name);
    ImGui::InputTextMultiline("Comment", &node_->comment, ImVec2(0.0f, 60.0f));
    ImGui::InputInt("Priority", &node_->priority);
    ImGui::DragFloat("Weight", &node_->weight, 0.05f, 0.0f, 100.0f);

    if (node_->type == AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION ||
        node_->type == AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD ||
        node_->type == AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL)
    {
        ImGui::SeparatorText("Blackboard");
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

void AIControllerComponent::DrawNode(AIBehaviorNode& node_)
{
    const bool isSelected_ = selectedNodeId == node_.id;
    const ImU32 nodeColor_ = GetNodeColor(node_.type);
    const ImU32 borderColor_ = isSelected_ ? IM_COL32(255, 245, 120, 255) : IM_COL32(35, 35, 35, 255);

    ed::PushStyleVar(ed::StyleVar_NodeRounding, 8.0f);
    ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(8.0f, 8.0f, 8.0f, 8.0f));
    ed::PushStyleColor(ed::StyleColor_NodeBg, ImGui::ColorConvertU32ToFloat4(nodeColor_));
    ed::PushStyleColor(ed::StyleColor_NodeBorder, ImGui::ColorConvertU32ToFloat4(borderColor_));

    ed::BeginNode(ed::NodeId(node_.id));
    ImGui::PushID(static_cast<int>(node_.id));

    ImGui::Text("%s", node_.name.c_str());
    ImGui::TextDisabled("%s", GetNodeTypeName(node_.type));

    if (node_.inputPinId != 0)
    {
        ed::BeginPin(ed::PinId(node_.inputPinId), ed::PinKind::Input);
        ImGui::Text("-> In");
        ed::EndPin();
    }

    if (node_.outputPinId != 0)
    {
        ed::BeginPin(ed::PinId(node_.outputPinId), ed::PinKind::Output);
        ImGui::Text("Out ->");
        ed::EndPin();
    }

    ImGui::PopID();
    ed::EndNode();

    ed::PopStyleColor(2);
    ed::PopStyleVar(2);
}

void AIControllerComponent::AddNodeFromMenu(AI_BEHAVIOR_NODE_TYPE type_, const char* name_)
{
    AIBehaviorNode* node_ = graph.AddNode(type_, name_, nextNodePosition);
    if (!node_) return;

    nodeToPlaceId = node_->id;
    selectedNodeId = node_->id;
    nextNodePosition.x += 40.0f;
    nextNodePosition.y += 40.0f;
}

const char* AIControllerComponent::GetNodeTypeName(AI_BEHAVIOR_NODE_TYPE type_) const
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

bool AIControllerComponent::GetPinInfo(uintptr_t pinId_, bool& isInput_, uintptr_t& nodeId_) const
{
    for (const AIBehaviorNode& node_ : graph.GetNodes())
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

REGISTER_COMPONENT(AIControllerComponent, "AIControllerComponent")
