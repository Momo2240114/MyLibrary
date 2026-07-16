#include "PrecompiledHeader.h"

#include "AIBehaviorGraph.h"

#include "AIActionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>

namespace
{
    std::string ToString(AI_BEHAVIOR_NODE_TYPE type_)
    {
        switch (type_)
        {
        case AI_BEHAVIOR_NODE_TYPE::ROOT: return "Root";
        case AI_BEHAVIOR_NODE_TYPE::SELECTOR: return "Selector";
        case AI_BEHAVIOR_NODE_TYPE::SEQUENCE: return "Sequence";
        case AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR: return "RandomSelector";
        case AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION: return "BlackboardCondition";
        case AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD: return "SetBlackboard";
        case AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION: return "PlayAction";
        case AI_BEHAVIOR_NODE_TYPE::WAIT: return "Wait";
        case AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL: return "WaitBlackboardBool";
        case AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH: return "SubGraph";
        default: return "Selector";
        }
    }

    AI_BEHAVIOR_NODE_TYPE NodeTypeFromString(const std::string& value_)
    {
        if (value_ == "Root") return AI_BEHAVIOR_NODE_TYPE::ROOT;
        if (value_ == "Selector") return AI_BEHAVIOR_NODE_TYPE::SELECTOR;
        if (value_ == "Sequence") return AI_BEHAVIOR_NODE_TYPE::SEQUENCE;
        if (value_ == "RandomSelector") return AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR;
        if (value_ == "BlackboardCondition") return AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION;
        if (value_ == "SetBlackboard") return AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD;
        if (value_ == "PlayAction") return AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION;
        if (value_ == "Wait") return AI_BEHAVIOR_NODE_TYPE::WAIT;
        if (value_ == "WaitBlackboardBool") return AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL;
        if (value_ == "SubGraph") return AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH;
        return AI_BEHAVIOR_NODE_TYPE::SELECTOR;
    }

    std::string ToString(AI_BLACKBOARD_COMPARE_TYPE type_)
    {
        switch (type_)
        {
        case AI_BLACKBOARD_COMPARE_TYPE::EXISTS: return "Exists";
        case AI_BLACKBOARD_COMPARE_TYPE::NOT_EXISTS: return "NotExists";
        case AI_BLACKBOARD_COMPARE_TYPE::EQUAL: return "Equal";
        case AI_BLACKBOARD_COMPARE_TYPE::NOT_EQUAL: return "NotEqual";
        case AI_BLACKBOARD_COMPARE_TYPE::GREATER: return "Greater";
        case AI_BLACKBOARD_COMPARE_TYPE::GREATER_EQUAL: return "GreaterEqual";
        case AI_BLACKBOARD_COMPARE_TYPE::LESS: return "Less";
        case AI_BLACKBOARD_COMPARE_TYPE::LESS_EQUAL: return "LessEqual";
        default: return "Equal";
        }
    }

    AI_BLACKBOARD_COMPARE_TYPE CompareTypeFromString(const std::string& value_)
    {
        if (value_ == "Exists") return AI_BLACKBOARD_COMPARE_TYPE::EXISTS;
        if (value_ == "NotExists") return AI_BLACKBOARD_COMPARE_TYPE::NOT_EXISTS;
        if (value_ == "Equal") return AI_BLACKBOARD_COMPARE_TYPE::EQUAL;
        if (value_ == "NotEqual") return AI_BLACKBOARD_COMPARE_TYPE::NOT_EQUAL;
        if (value_ == "Greater") return AI_BLACKBOARD_COMPARE_TYPE::GREATER;
        if (value_ == "GreaterEqual") return AI_BLACKBOARD_COMPARE_TYPE::GREATER_EQUAL;
        if (value_ == "Less") return AI_BLACKBOARD_COMPARE_TYPE::LESS;
        if (value_ == "LessEqual") return AI_BLACKBOARD_COMPARE_TYPE::LESS_EQUAL;
        return AI_BLACKBOARD_COMPARE_TYPE::EQUAL;
    }

    std::string ToString(AI_BLACKBOARD_VALUE_TYPE type_)
    {
        switch (type_)
        {
        case AI_BLACKBOARD_VALUE_TYPE::FLOAT: return "Float";
        case AI_BLACKBOARD_VALUE_TYPE::INT: return "Int";
        case AI_BLACKBOARD_VALUE_TYPE::BOOL: return "Bool";
        case AI_BLACKBOARD_VALUE_TYPE::VECTOR2: return "Vector2";
        case AI_BLACKBOARD_VALUE_TYPE::VECTOR3: return "Vector3";
        case AI_BLACKBOARD_VALUE_TYPE::STRING: return "String";
        case AI_BLACKBOARD_VALUE_TYPE::TRIGGER: return "Trigger";
        default: return "Bool";
        }
    }

    AI_BLACKBOARD_VALUE_TYPE ValueTypeFromString(const std::string& value_)
    {
        if (value_ == "Float") return AI_BLACKBOARD_VALUE_TYPE::FLOAT;
        if (value_ == "Int") return AI_BLACKBOARD_VALUE_TYPE::INT;
        if (value_ == "Bool") return AI_BLACKBOARD_VALUE_TYPE::BOOL;
        if (value_ == "Vector2") return AI_BLACKBOARD_VALUE_TYPE::VECTOR2;
        if (value_ == "Vector3") return AI_BLACKBOARD_VALUE_TYPE::VECTOR3;
        if (value_ == "String") return AI_BLACKBOARD_VALUE_TYPE::STRING;
        if (value_ == "Trigger") return AI_BLACKBOARD_VALUE_TYPE::TRIGGER;
        return AI_BLACKBOARD_VALUE_TYPE::BOOL;
    }

    std::string ToString(AI_BLACKBOARD_SCOPE scope_)
    {
        switch (scope_)
        {
        case AI_BLACKBOARD_SCOPE::LOCAL: return "Local";
        case AI_BLACKBOARD_SCOPE::GLOBAL: return "Global";
        default: return "Local";
        }
    }

    AI_BLACKBOARD_SCOPE BlackboardScopeFromString(const std::string& value_)
    {
        if (value_ == "Global") return AI_BLACKBOARD_SCOPE::GLOBAL;
        return AI_BLACKBOARD_SCOPE::LOCAL;
    }

    bool IsComposite(AI_BEHAVIOR_NODE_TYPE type_)
    {
        return type_ == AI_BEHAVIOR_NODE_TYPE::ROOT ||
            type_ == AI_BEHAVIOR_NODE_TYPE::SELECTOR ||
            type_ == AI_BEHAVIOR_NODE_TYPE::SEQUENCE ||
            type_ == AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR ||
            type_ == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH;
    }
}

void AIBehaviorGraph::Clear()
{
    nodes.clear();
    links.clear();
    runtimeStates.clear();
    nextId = 1;
}

void AIBehaviorGraph::EnsureDefaultGraph()
{
    if (!nodes.empty()) return;

    AddNode(AI_BEHAVIOR_NODE_TYPE::ROOT, "Root", { 40.0f, 120.0f });
    AddNode(AI_BEHAVIOR_NODE_TYPE::SELECTOR, "Selector", { 280.0f, 120.0f });
    AddLink(nodes[0].id, nodes[1].id);
}

AI_BEHAVIOR_STATUS AIBehaviorGraph::Tick(AIBehaviorContext& context_)
{
    if (!context_.owner || !context_.blackboard) return AI_BEHAVIOR_STATUS::FAILURE;

    EnsureDefaultGraph();

    AIBehaviorNode* rootNode_ = FindRootNode();
    if (!rootNode_) return AI_BEHAVIOR_STATUS::FAILURE;

    return TickNode(*rootNode_, context_);
}

void AIBehaviorGraph::ResetRuntime()
{
    runtimeStates.clear();
}

AIBehaviorNode* AIBehaviorGraph::AddNode(
    AI_BEHAVIOR_NODE_TYPE type_,
    const std::string& name_,
    const DirectX::XMFLOAT2& position_)
{
    AIBehaviorNode node_;
    node_.id = GenerateId();
    node_.inputPinId = GenerateId();
    node_.outputPinId = GenerateId();
    node_.type = type_;
    node_.name = name_;
    node_.position = position_;

    if (type_ == AI_BEHAVIOR_NODE_TYPE::ROOT)
    {
        node_.inputPinId = 0;
    }
    if (!IsComposite(type_))
    {
        node_.outputPinId = 0;
    }

    nodes.push_back(node_);
    return &nodes.back();
}

bool AIBehaviorGraph::RemoveNode(uintptr_t nodeId_)
{
    const auto nodeIt_ = std::find_if(nodes.begin(), nodes.end(),
        [nodeId_](const AIBehaviorNode& node_) { return node_.id == nodeId_; });
    if (nodeIt_ == nodes.end()) return false;
    if (nodeIt_->type == AI_BEHAVIOR_NODE_TYPE::ROOT) return false;

    links.erase(
        std::remove_if(links.begin(), links.end(),
            [nodeId_](const AIBehaviorLink& link_)
            {
                return link_.fromNodeId == nodeId_ || link_.toNodeId == nodeId_;
            }),
        links.end());
    runtimeStates.erase(nodeId_);
    nodes.erase(nodeIt_);
    return true;
}

AIBehaviorLink* AIBehaviorGraph::AddLink(uintptr_t fromNodeId_, uintptr_t toNodeId_)
{
    AIBehaviorNode* fromNode_ = FindNode(fromNodeId_);
    AIBehaviorNode* toNode_ = FindNode(toNodeId_);
    if (!fromNode_ || !toNode_) return nullptr;
    if (fromNode_->outputPinId == 0 || toNode_->inputPinId == 0) return nullptr;
    if (fromNode_->id == toNode_->id) return nullptr;

    for (AIBehaviorLink& link_ : links)
    {
        if (link_.fromNodeId == fromNodeId_ && link_.toNodeId == toNodeId_) return &link_;
    }

    AIBehaviorLink link_;
    link_.id = GenerateId();
    link_.fromNodeId = fromNodeId_;
    link_.toNodeId = toNodeId_;
    link_.fromPinId = fromNode_->outputPinId;
    link_.toPinId = toNode_->inputPinId;
    links.push_back(link_);
    return &links.back();
}

bool AIBehaviorGraph::RemoveLink(uintptr_t linkId_)
{
    const auto linkIt_ = std::remove_if(links.begin(), links.end(),
        [linkId_](const AIBehaviorLink& link_) { return link_.id == linkId_; });
    if (linkIt_ == links.end()) return false;

    links.erase(linkIt_, links.end());
    ResetRuntime();
    return true;
}

AIBehaviorNode* AIBehaviorGraph::FindNode(uintptr_t nodeId_)
{
    for (AIBehaviorNode& node_ : nodes)
    {
        if (node_.id == nodeId_) return &node_;
    }
    return nullptr;
}

const AIBehaviorNode* AIBehaviorGraph::FindNode(uintptr_t nodeId_) const
{
    for (const AIBehaviorNode& node_ : nodes)
    {
        if (node_.id == nodeId_) return &node_;
    }
    return nullptr;
}

AIBehaviorNode* AIBehaviorGraph::FindNodeByPin(uintptr_t pinId_)
{
    for (AIBehaviorNode& node_ : nodes)
    {
        if (node_.inputPinId == pinId_ || node_.outputPinId == pinId_) return &node_;
    }
    return nullptr;
}

const AIBehaviorNode* AIBehaviorGraph::FindNodeByPin(uintptr_t pinId_) const
{
    for (const AIBehaviorNode& node_ : nodes)
    {
        if (node_.inputPinId == pinId_ || node_.outputPinId == pinId_) return &node_;
    }
    return nullptr;
}

AIBehaviorLink* AIBehaviorGraph::FindLink(uintptr_t linkId_)
{
    for (AIBehaviorLink& link_ : links)
    {
        if (link_.id == linkId_) return &link_;
    }
    return nullptr;
}

uintptr_t AIBehaviorGraph::GenerateId()
{
    return nextId++;
}

void AIBehaviorGraph::SetIdCounterFloor(uintptr_t minValue_)
{
    if (nextId < minValue_) nextId = minValue_;
}

void AIBehaviorGraph::Serialize(nlohmann::json& json_) const
{
    json_ = nlohmann::json::object();
    json_["Version"] = 1;
    json_["NextId"] = nextId;
    json_["Nodes"] = nlohmann::json::array();
    json_["Links"] = nlohmann::json::array();

    for (const AIBehaviorNode& node_ : nodes)
    {
        nlohmann::json nodeJson_;
        nodeJson_["Id"] = node_.id;
        nodeJson_["InputPinId"] = node_.inputPinId;
        nodeJson_["OutputPinId"] = node_.outputPinId;
        nodeJson_["Type"] = ToString(node_.type);
        nodeJson_["Name"] = node_.name;
        nodeJson_["Comment"] = node_.comment;
        nodeJson_["ScopeId"] = node_.scopeId;
        nodeJson_["Position"] = { node_.position.x, node_.position.y };
        nodeJson_["BlackboardCategory"] = node_.blackboardCategory;
        nodeJson_["BlackboardKey"] = node_.blackboardKey;
        nodeJson_["BlackboardScope"] = ToString(node_.blackboardScope);
        nodeJson_["CompareType"] = ToString(node_.compareType);
        nodeJson_["ValueType"] = ToString(node_.valueType);
        nodeJson_["FloatValue"] = node_.floatValue;
        nodeJson_["IntValue"] = node_.intValue;
        nodeJson_["BoolValue"] = node_.boolValue;
        nodeJson_["Vector2Value"] = { node_.vector2Value.x, node_.vector2Value.y };
        nodeJson_["Vector3Value"] = { node_.vector3Value.x, node_.vector3Value.y, node_.vector3Value.z };
        nodeJson_["StringValue"] = node_.stringValue;
        nodeJson_["ActionName"] = node_.actionName;
        nodeJson_["WaitForActionEnd"] = node_.waitForActionEnd;
        nodeJson_["ActionEndCategory"] = node_.actionEndCategory;
        nodeJson_["ActionEndKey"] = node_.actionEndKey;
        nodeJson_["WaitSeconds"] = node_.waitSeconds;
        nodeJson_["SubGraphName"] = node_.subGraphName;
        nodeJson_["Priority"] = node_.priority;
        nodeJson_["Weight"] = node_.weight;
        json_["Nodes"].push_back(nodeJson_);
    }

    for (const AIBehaviorLink& link_ : links)
    {
        json_["Links"].push_back({
            { "Id", link_.id },
            { "FromNodeId", link_.fromNodeId },
            { "ToNodeId", link_.toNodeId },
            { "FromPinId", link_.fromPinId },
            { "ToPinId", link_.toPinId },
            });
    }
}

void AIBehaviorGraph::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::Deserialize");

    Clear();
    nextId = json_.value("NextId", static_cast<uintptr_t>(1));

    if (json_.contains("Nodes") && json_["Nodes"].is_array())
    {
        for (const nlohmann::json& nodeJson_ : json_["Nodes"])
        {
            AIBehaviorNode node_;
            node_.id = nodeJson_.value("Id", static_cast<uintptr_t>(0));
            node_.inputPinId = nodeJson_.value("InputPinId", static_cast<uintptr_t>(0));
            node_.outputPinId = nodeJson_.value("OutputPinId", static_cast<uintptr_t>(0));
            node_.type = NodeTypeFromString(nodeJson_.value("Type", "Selector"));
            node_.name = nodeJson_.value("Name", ToString(node_.type));
            node_.comment = nodeJson_.value("Comment", std::string{});
            node_.scopeId = nodeJson_.value("ScopeId", std::string("Root"));
            if (nodeJson_.contains("Position") && nodeJson_["Position"].is_array())
            {
                if (nodeJson_["Position"].size() > 0) node_.position.x = nodeJson_["Position"][0].get<float>();
                if (nodeJson_["Position"].size() > 1) node_.position.y = nodeJson_["Position"][1].get<float>();
            }
            node_.blackboardCategory = nodeJson_.value("BlackboardCategory", std::string(AIBlackboardKeys::CATEGORY_AI));
            node_.blackboardKey = nodeJson_.value("BlackboardKey", std::string{});
            node_.blackboardScope = BlackboardScopeFromString(nodeJson_.value("BlackboardScope", "Local"));
            node_.compareType = CompareTypeFromString(nodeJson_.value("CompareType", "Equal"));
            node_.valueType = ValueTypeFromString(nodeJson_.value("ValueType", "Bool"));
            node_.floatValue = nodeJson_.value("FloatValue", 0.0f);
            node_.intValue = nodeJson_.value("IntValue", 0);
            node_.boolValue = nodeJson_.value("BoolValue", false);
            if (nodeJson_.contains("Vector2Value") && nodeJson_["Vector2Value"].is_array())
            {
                if (nodeJson_["Vector2Value"].size() > 0) node_.vector2Value.x = nodeJson_["Vector2Value"][0].get<float>();
                if (nodeJson_["Vector2Value"].size() > 1) node_.vector2Value.y = nodeJson_["Vector2Value"][1].get<float>();
            }
            if (nodeJson_.contains("Vector3Value") && nodeJson_["Vector3Value"].is_array())
            {
                if (nodeJson_["Vector3Value"].size() > 0) node_.vector3Value.x = nodeJson_["Vector3Value"][0].get<float>();
                if (nodeJson_["Vector3Value"].size() > 1) node_.vector3Value.y = nodeJson_["Vector3Value"][1].get<float>();
                if (nodeJson_["Vector3Value"].size() > 2) node_.vector3Value.z = nodeJson_["Vector3Value"][2].get<float>();
            }
            node_.stringValue = nodeJson_.value("StringValue", std::string{});
            node_.actionName = nodeJson_.value("ActionName", std::string{});
            node_.waitForActionEnd = nodeJson_.value("WaitForActionEnd", true);
            node_.actionEndCategory = nodeJson_.value("ActionEndCategory", std::string(AIBlackboardKeys::CATEGORY_AI));
            node_.actionEndKey = nodeJson_.value("ActionEndKey", std::string(AIBlackboardKeys::ACTION_ENDED));
            node_.waitSeconds = nodeJson_.value("WaitSeconds", 0.0f);
            node_.subGraphName = nodeJson_.value("SubGraphName", std::string{});
            node_.priority = nodeJson_.value("Priority", 0);
            node_.weight = nodeJson_.value("Weight", 1.0f);
            nodes.push_back(node_);
            SetIdCounterFloor(node_.id + 1);
            SetIdCounterFloor(node_.inputPinId + 1);
            SetIdCounterFloor(node_.outputPinId + 1);
        }
    }

    if (json_.contains("Links") && json_["Links"].is_array())
    {
        for (const nlohmann::json& linkJson_ : json_["Links"])
        {
            AIBehaviorLink link_;
            link_.id = linkJson_.value("Id", static_cast<uintptr_t>(0));
            link_.fromNodeId = linkJson_.value("FromNodeId", static_cast<uintptr_t>(0));
            link_.toNodeId = linkJson_.value("ToNodeId", static_cast<uintptr_t>(0));
            link_.fromPinId = linkJson_.value("FromPinId", static_cast<uintptr_t>(0));
            link_.toPinId = linkJson_.value("ToPinId", static_cast<uintptr_t>(0));
            if (FindNode(link_.fromNodeId) && FindNode(link_.toNodeId))
            {
                links.push_back(link_);
                SetIdCounterFloor(link_.id + 1);
            }
        }
    }

    EnsureDefaultGraph();
}

bool AIBehaviorGraph::SaveToFile(const std::string& path_) const
{
    const std::filesystem::path filePath_(path_);
    if (!filePath_.parent_path().empty())
    {
        std::filesystem::create_directories(filePath_.parent_path());
    }

    nlohmann::json json_;
    Serialize(json_);

    std::ofstream file_(path_);
    if (!file_.is_open()) return false;
    file_ << json_.dump(4);
    return true;
}

bool AIBehaviorGraph::LoadFromFile(const std::string& path_)
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::LoadFromFile");

    std::ifstream file_(path_);
    if (!file_.is_open()) return false;

    nlohmann::json json_;
    {
        BS_PROFILE_SCOPE("AIBehaviorGraph::ReadJson");
        file_ >> json_;
    }
    Deserialize(json_);
    return true;
}

AI_BEHAVIOR_STATUS AIBehaviorGraph::TickNode(AIBehaviorNode& node_, AIBehaviorContext& context_)
{
    if (IsComposite(node_.type))
    {
        return TickComposite(node_, context_);
    }
    return TickTask(node_, context_);
}

AI_BEHAVIOR_STATUS AIBehaviorGraph::TickComposite(AIBehaviorNode& node_, AIBehaviorContext& context_)
{
    std::vector<AIBehaviorNode*> children_ = GetChildren(node_);
    if (children_.empty()) return AI_BEHAVIOR_STATUS::FAILURE;

    if (node_.type == AI_BEHAVIOR_NODE_TYPE::ROOT || node_.type == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH)
    {
        return TickNode(*children_.front(), context_);
    }

    AIBehaviorRuntimeState& state_ = runtimeStates[node_.id];

    if (node_.type == AI_BEHAVIOR_NODE_TYPE::SEQUENCE)
    {
        while (state_.childIndex < children_.size())
        {
            AI_BEHAVIOR_STATUS result_ = TickNode(*children_[state_.childIndex], context_);
            if (result_ == AI_BEHAVIOR_STATUS::RUNNING) return result_;
            if (result_ == AI_BEHAVIOR_STATUS::FAILURE)
            {
                state_ = {};
                return AI_BEHAVIOR_STATUS::FAILURE;
            }
            ++state_.childIndex;
        }
        state_ = {};
        return AI_BEHAVIOR_STATUS::SUCCESS;
    }

    if (node_.type == AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR)
    {
        static std::mt19937 randomEngine_(std::random_device{}());
        float totalWeight_ = 0.0f;
        for (const AIBehaviorNode* child_ : children_)
        {
            totalWeight_ += (std::max)(0.0f, child_->weight);
        }

        if (totalWeight_ > 0.0001f)
        {
            std::uniform_real_distribution<float> distribution_(0.0f, totalWeight_);
            float pick_ = distribution_(randomEngine_);
            for (AIBehaviorNode* child_ : children_)
            {
                pick_ -= (std::max)(0.0f, child_->weight);
                if (pick_ <= 0.0f)
                {
                    return TickNode(*child_, context_);
                }
            }
            return TickNode(*children_.back(), context_);
        }

        std::shuffle(children_.begin(), children_.end(), randomEngine_);
    }

    for (AIBehaviorNode* child_ : children_)
    {
        AI_BEHAVIOR_STATUS result_ = TickNode(*child_, context_);
        if (result_ == AI_BEHAVIOR_STATUS::RUNNING) return result_;
        if (result_ == AI_BEHAVIOR_STATUS::SUCCESS) return AI_BEHAVIOR_STATUS::SUCCESS;
    }

    return AI_BEHAVIOR_STATUS::FAILURE;
}

AI_BEHAVIOR_STATUS AIBehaviorGraph::TickTask(AIBehaviorNode& node_, AIBehaviorContext& context_)
{
    BlackboardComponent& blackboard_ = *context_.blackboard;
    AIBehaviorRuntimeState& state_ = runtimeStates[node_.id];

    switch (node_.type)
    {
    case AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION:
        return EvaluateBlackboardCondition(node_, context_)
            ? AI_BEHAVIOR_STATUS::SUCCESS
            : AI_BEHAVIOR_STATUS::FAILURE;

    case AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD:
        SetBlackboardValue(node_, context_);
        return AI_BEHAVIOR_STATUS::SUCCESS;

    case AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION:
        if (!state_.actionStarted)
        {
            bool executed_ = false;
            if (auto aiActionComponent_ = context_.owner->GetComponent<AIActionComponent>())
            {
                executed_ = aiActionComponent_->ExecuteAIAction(node_.actionName);
            }
            if (!executed_)
            {
                context_.owner->ExecuteAction(node_.actionName);
            }
            state_.actionStarted = true;
            if (!node_.waitForActionEnd)
            {
                state_ = {};
                return AI_BEHAVIOR_STATUS::SUCCESS;
            }
        }
        if (IsActionEndReached(node_, blackboard_))
        {
            state_ = {};
            return AI_BEHAVIOR_STATUS::SUCCESS;
        }
        return AI_BEHAVIOR_STATUS::RUNNING;

    case AI_BEHAVIOR_NODE_TYPE::WAIT:
        state_.elapsedTime += context_.deltaTime;
        if (state_.elapsedTime >= node_.waitSeconds)
        {
            state_ = {};
            return AI_BEHAVIOR_STATUS::SUCCESS;
        }
        return AI_BEHAVIOR_STATUS::RUNNING;

    case AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL:
    {
        BlackboardComponent* targetBlackboard_ = ResolveBlackboard(node_, context_);
        if (!targetBlackboard_) return AI_BEHAVIOR_STATUS::RUNNING;

        bool value_ = false;
        if (targetBlackboard_->GetBool(node_.blackboardCategory, node_.blackboardKey, value_) && value_ == node_.boolValue)
        {
            state_ = {};
            return AI_BEHAVIOR_STATUS::SUCCESS;
        }
        return AI_BEHAVIOR_STATUS::RUNNING;
    }

    default:
        return AI_BEHAVIOR_STATUS::FAILURE;
    }
}

std::vector<AIBehaviorNode*> AIBehaviorGraph::GetChildren(const AIBehaviorNode& node_)
{
    std::vector<AIBehaviorNode*> children_;
    for (const AIBehaviorLink& link_ : links)
    {
        if (link_.fromNodeId != node_.id) continue;
        AIBehaviorNode* child_ = FindNode(link_.toNodeId);
        if (child_) children_.push_back(child_);
    }

    std::sort(children_.begin(), children_.end(),
        [](const AIBehaviorNode* left_, const AIBehaviorNode* right_)
        {
            if (left_->priority != right_->priority) return left_->priority < right_->priority;
            return left_->id < right_->id;
        });
    return children_;
}

BlackboardComponent* AIBehaviorGraph::ResolveBlackboard(
    const AIBehaviorNode& node_,
    AIBehaviorContext& context_) const
{
    if (node_.blackboardScope == AI_BLACKBOARD_SCOPE::GLOBAL)
    {
        return context_.globalBlackboard;
    }
    return context_.blackboard;
}

bool AIBehaviorGraph::EvaluateBlackboardCondition(
    const AIBehaviorNode& node_,
    AIBehaviorContext& context_) const
{
    BlackboardComponent* blackboard_ = ResolveBlackboard(node_, context_);
    if (!blackboard_) return false;

    const BlackBoardValue* value_ = blackboard_->GetValue(node_.blackboardCategory, node_.blackboardKey);

    if (node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::EXISTS) return value_ != nullptr;
    if (node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::NOT_EXISTS) return value_ == nullptr;
    if (!value_) return false;

    if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::STRING)
    {
        std::string stringValue_;
        if (!blackboard_->GetString(node_.blackboardCategory, node_.blackboardKey, stringValue_)) return false;
        const bool equal_ = stringValue_ == node_.stringValue;
        return node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::NOT_EQUAL ? !equal_ : equal_;
    }

    const float currentValue_ = BlackboardComponent::ToFloat(*value_);
    float compareValue_ = node_.floatValue;
    if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::INT) compareValue_ = static_cast<float>(node_.intValue);
    if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::BOOL) compareValue_ = node_.boolValue ? 1.0f : 0.0f;

    switch (node_.compareType)
    {
    case AI_BLACKBOARD_COMPARE_TYPE::EQUAL: return currentValue_ == compareValue_;
    case AI_BLACKBOARD_COMPARE_TYPE::NOT_EQUAL: return currentValue_ != compareValue_;
    case AI_BLACKBOARD_COMPARE_TYPE::GREATER: return currentValue_ > compareValue_;
    case AI_BLACKBOARD_COMPARE_TYPE::GREATER_EQUAL: return currentValue_ >= compareValue_;
    case AI_BLACKBOARD_COMPARE_TYPE::LESS: return currentValue_ < compareValue_;
    case AI_BLACKBOARD_COMPARE_TYPE::LESS_EQUAL: return currentValue_ <= compareValue_;
    default: return false;
    }
}

void AIBehaviorGraph::SetBlackboardValue(
    const AIBehaviorNode& node_,
    AIBehaviorContext& context_) const
{
    BlackboardComponent* blackboard_ = ResolveBlackboard(node_, context_);
    if (!blackboard_) return;

    switch (node_.valueType)
    {
    case AI_BLACKBOARD_VALUE_TYPE::FLOAT:
        blackboard_->SetFloat(node_.blackboardCategory, node_.blackboardKey, node_.floatValue);
        break;
    case AI_BLACKBOARD_VALUE_TYPE::INT:
        blackboard_->SetInt(node_.blackboardCategory, node_.blackboardKey, node_.intValue);
        break;
    case AI_BLACKBOARD_VALUE_TYPE::BOOL:
        blackboard_->SetBool(node_.blackboardCategory, node_.blackboardKey, node_.boolValue);
        break;
    case AI_BLACKBOARD_VALUE_TYPE::VECTOR2:
        blackboard_->SetVector2(node_.blackboardCategory, node_.blackboardKey, { node_.vector2Value.x, node_.vector2Value.y });
        break;
    case AI_BLACKBOARD_VALUE_TYPE::VECTOR3:
        blackboard_->SetVector3(node_.blackboardCategory, node_.blackboardKey, { node_.vector3Value.x, node_.vector3Value.y, node_.vector3Value.z });
        break;
    case AI_BLACKBOARD_VALUE_TYPE::STRING:
        blackboard_->SetString(node_.blackboardCategory, node_.blackboardKey, node_.stringValue);
        break;
    case AI_BLACKBOARD_VALUE_TYPE::TRIGGER:
        blackboard_->SetTrigger(node_.blackboardCategory, node_.blackboardKey);
        break;
    default:
        break;
    }
}

bool AIBehaviorGraph::IsActionEndReached(const AIBehaviorNode& node_, BlackboardComponent& blackboard_) const
{
    bool ended_ = false;
    if (!blackboard_.GetBool(node_.actionEndCategory, node_.actionEndKey, ended_)) return false;
    return ended_;
}

AIBehaviorNode* AIBehaviorGraph::FindRootNode()
{
    for (AIBehaviorNode& node_ : nodes)
    {
        if (node_.type == AI_BEHAVIOR_NODE_TYPE::ROOT) return &node_;
    }
    return nullptr;
}
