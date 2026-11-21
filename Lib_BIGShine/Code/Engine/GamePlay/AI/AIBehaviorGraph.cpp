#include "PrecompiledHeader.h"

#include "AIBehaviorGraph.h"

#include "AIActionComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <utility>

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
        if (value_ == "Root")
        {
            return AI_BEHAVIOR_NODE_TYPE::ROOT;
        }
        if (value_ == "Selector")
        {
            return AI_BEHAVIOR_NODE_TYPE::SELECTOR;
        }
        if (value_ == "Sequence")
        {
            return AI_BEHAVIOR_NODE_TYPE::SEQUENCE;
        }
        if (value_ == "RandomSelector")
        {
            return AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR;
        }
        if (value_ == "BlackboardCondition")
        {
            return AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION;
        }
        if (value_ == "SetBlackboard")
        {
            return AI_BEHAVIOR_NODE_TYPE::SET_BLACKBOARD;
        }
        if (value_ == "PlayAction")
        {
            return AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION;
        }
        if (value_ == "Wait")
        {
            return AI_BEHAVIOR_NODE_TYPE::WAIT;
        }
        if (value_ == "WaitBlackboardBool")
        {
            return AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL;
        }
        if (value_ == "SubGraph")
        {
            return AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH;
        }
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
        if (value_ == "Exists")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::EXISTS;
        }
        if (value_ == "NotExists")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::NOT_EXISTS;
        }
        if (value_ == "Equal")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::EQUAL;
        }
        if (value_ == "NotEqual")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::NOT_EQUAL;
        }
        if (value_ == "Greater")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::GREATER;
        }
        if (value_ == "GreaterEqual")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::GREATER_EQUAL;
        }
        if (value_ == "Less")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::LESS;
        }
        if (value_ == "LessEqual")
        {
            return AI_BLACKBOARD_COMPARE_TYPE::LESS_EQUAL;
        }
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
        if (value_ == "Float")
        {
            return AI_BLACKBOARD_VALUE_TYPE::FLOAT;
        }
        if (value_ == "Int")
        {
            return AI_BLACKBOARD_VALUE_TYPE::INT;
        }
        if (value_ == "Bool")
        {
            return AI_BLACKBOARD_VALUE_TYPE::BOOL;
        }
        if (value_ == "Vector2")
        {
            return AI_BLACKBOARD_VALUE_TYPE::VECTOR2;
        }
        if (value_ == "Vector3")
        {
            return AI_BLACKBOARD_VALUE_TYPE::VECTOR3;
        }
        if (value_ == "String")
        {
            return AI_BLACKBOARD_VALUE_TYPE::STRING;
        }
        if (value_ == "Trigger")
        {
            return AI_BLACKBOARD_VALUE_TYPE::TRIGGER;
        }
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
        if (value_ == "Global")
        {
            return AI_BLACKBOARD_SCOPE::GLOBAL;
        }
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

// 保存データと実行時データをすべて破棄し、ID発行番号を初期値へ戻す。
void AIBehaviorGraph::Clear()
{
    // Assetとして保持するノードと接続情報を破棄する。
    nodes.clear();
    links.clear();
    areas.clear();
    priorityFromPositions = true;
    // 実行途中の状態も破棄し、新規GraphとしてIDを1から発行し直す。
    runtimeStates.clear();
    nextId = 1;
}

// Graphが空の場合に、Editor表示と実行が成立するRoot→Selectorの最小構成を生成する。
void AIBehaviorGraph::EnsureDefaultGraph()
{
    if (!nodes.empty())
    {
        return;
    }

    // 空のGraphでも実行とEditor表示が成立するよう、RootからSelectorへつながる最小構成を作る。
    AddNode(AI_BEHAVIOR_NODE_TYPE::ROOT, "Root", { 40.0f, 120.0f });
    AddNode(AI_BEHAVIOR_NODE_TYPE::SELECTOR, "Selector", { 280.0f, 120.0f });
    AddLink(nodes[0].id, nodes[1].id);
}

// RootノードからBehaviorGraphを1回評価し、そのTick全体の実行結果を返す。
AI_BEHAVIOR_STATUS AIBehaviorGraph::Tick(AIBehaviorContext& context_)
{
    // 1TickごとにRuntime表示用のActiveノードを作り直し、Rootから評価を開始する。
    // Editorの実行中表示はTick単位で作り直し、未実行時の既定結果をFailureにする。
    runtimeActiveNodeIds.clear();
    lastTickStatus = AI_BEHAVIOR_STATUS::FAILURE;
    // Action実行元または判断値の保存先がなければGraphを安全に中断する。
    if (!context_.owner || !context_.blackboard)
    {
        return lastTickStatus;
    }

    EnsureDefaultGraph();

    // 必ずRootを開始地点とし、Rootが存在しない不正GraphはFailureとして扱う。
    AIBehaviorNode* rootNode_ = FindRootNode();
    if (!rootNode_)
    {
        return lastTickStatus;
    }

    lastTickStatus = TickNode(*rootNode_, context_);
    return lastTickStatus;
}

// ノード構成は維持したまま、実行中の進行状態とEditor表示用情報だけを初期化する。
void AIBehaviorGraph::ResetRuntime()
{
    runtimeStates.clear();
    runtimeActiveNodeIds.clear();
    lastTickStatus = AI_BEHAVIOR_STATUS::FAILURE;
}

// 指定ノードが直前のTickで評価対象になったかを確認する。
bool AIBehaviorGraph::IsNodeRuntimeActive(uintptr_t nodeId_) const
{
    return std::find(runtimeActiveNodeIds.begin(), runtimeActiveNodeIds.end(), nodeId_) != runtimeActiveNodeIds.end();
}

// 新しいノードとPin用IDを発行し、ノード種別に応じて使用しないPinを無効化する。
AIBehaviorNode* AIBehaviorGraph::AddNode(
    AI_BEHAVIOR_NODE_TYPE type_,
    const std::string& name_,
    const DirectX::XMFLOAT2& position_)
{
    AIBehaviorNode node_;
    // ノード本体と各PinはEditor上で個別に識別されるため、それぞれ一意IDを持たせる。
    node_.id = GenerateId();
    node_.inputPinId = GenerateId();
    node_.outputPinId = GenerateId();
    node_.type = type_;
    node_.name = name_;
    node_.position = position_;

    // Rootは親を持たないため入力Pinを、Taskは子を持たないため出力Pinを無効化する。
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

// Root以外の指定ノードと、そのノードへ接続されているLink・Runtime状態を削除する。
bool AIBehaviorGraph::RemoveNode(uintptr_t nodeId_)
{
    const auto nodeIt_ = std::find_if(nodes.begin(), nodes.end(),
        [nodeId_](const AIBehaviorNode& node_)
    {
        return node_.id == nodeId_;
    });
    if (nodeIt_ == nodes.end())
    {
        return false;
    }
    if (nodeIt_->type == AI_BEHAVIOR_NODE_TYPE::ROOT)
    {
        return false;
    }

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

// 2ノード間の接続条件を検証し、重複していなければ新しいLinkを追加する。
AIBehaviorLink* AIBehaviorGraph::AddLink(uintptr_t fromNodeId_, uintptr_t toNodeId_)
{
    // 接続元と接続先が実在し、使用可能なPinを持っているかを先に検証する。
    AIBehaviorNode* fromNode_ = FindNode(fromNodeId_);
    AIBehaviorNode* toNode_ = FindNode(toNodeId_);
    if (!fromNode_ || !toNode_)
    {
        return nullptr;
    }
    if (fromNode_->outputPinId == 0 || toNode_->inputPinId == 0)
    {
        return nullptr;
    }
    if (fromNode_->id == toNode_->id)
    {
        return nullptr;
    }

    // 同じ方向の接続がすでにある場合は新規作成せず、既存Linkを返す。
    for (AIBehaviorLink& link_ : links)
    {
        if (link_.fromNodeId == fromNodeId_ && link_.toNodeId == toNodeId_)
        {
            return &link_;
        }
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

// 指定Linkを削除し、古い進行位置を参照しないようRuntime状態をリセットする。
bool AIBehaviorGraph::RemoveLink(uintptr_t linkId_)
{
    const auto linkIt_ = std::remove_if(links.begin(), links.end(),
        [linkId_](const AIBehaviorLink& link_)
    {
        return link_.id == linkId_;
    });
    if (linkIt_ == links.end())
    {
        return false;
    }

    links.erase(linkIt_, links.end());
    ResetRuntime();
    return true;
}

// IDが一致する編集可能なノードを検索する。
AIBehaviorNode* AIBehaviorGraph::FindNode(uintptr_t nodeId_)
{
    for (AIBehaviorNode& node_ : nodes)
    {
        if (node_.id == nodeId_)
        {
            return &node_;
        }
    }
    return nullptr;
}

// IDが一致する読み取り専用ノードを検索する。
const AIBehaviorNode* AIBehaviorGraph::FindNode(uintptr_t nodeId_) const
{
    for (const AIBehaviorNode& node_ : nodes)
    {
        if (node_.id == nodeId_)
        {
            return &node_;
        }
    }
    return nullptr;
}

// 入力Pinまたは出力PinのIDが一致する編集可能なノードを検索する。
AIBehaviorNode* AIBehaviorGraph::FindNodeByPin(uintptr_t pinId_)
{
    for (AIBehaviorNode& node_ : nodes)
    {
        if (node_.inputPinId == pinId_ || node_.outputPinId == pinId_)
        {
            return &node_;
        }
    }
    return nullptr;
}

// 入力Pinまたは出力PinのIDが一致する読み取り専用ノードを検索する。
const AIBehaviorNode* AIBehaviorGraph::FindNodeByPin(uintptr_t pinId_) const
{
    for (const AIBehaviorNode& node_ : nodes)
    {
        if (node_.inputPinId == pinId_ || node_.outputPinId == pinId_)
        {
            return &node_;
        }
    }
    return nullptr;
}

// IDが一致するLinkを検索する。
AIBehaviorLink* AIBehaviorGraph::FindLink(uintptr_t linkId_)
{
    for (AIBehaviorLink& link_ : links)
    {
        if (link_.id == linkId_)
        {
            return &link_;
        }
    }
    return nullptr;
}

// ノード・Pin・Linkで共有する一意IDを発行し、次回用の値を進める。
uintptr_t AIBehaviorGraph::GenerateId()
{
    return nextId++;
}

// 復元済みIDと衝突しないよう、次回発行IDの下限を引き上げる。
void AIBehaviorGraph::SetIdCounterFloor(uintptr_t minValue_)
{
    if (nextId < minValue_)
    {
        nextId = minValue_;
    }
}

// ノード設定・Editor位置・Link構成をBehaviorGraph Asset用JSONへ変換する。
void AIBehaviorGraph::Serialize(nlohmann::json& json_) const
{
    // NodeEditorで編集したノード位置、条件、Action名、LinkをそのままJSON Assetへ保存する。
    json_ = nlohmann::json::object();
    json_["Version"] = 1;
    json_["NextId"] = nextId;
    json_["PriorityFromPositions"] = priorityFromPositions;
    json_["Areas"] = nlohmann::json::array();
    for (const auto& area_ : areas)
    {
        json_["Areas"].push_back({ { "Id", area_.id }, { "Name", area_.name },
            { "Comment", area_.comment }, { "Position", { area_.position.x, area_.position.y } },
            { "Size", { area_.size.x, area_.size.y } },
            { "Color", { area_.color.x, area_.color.y, area_.color.z, area_.color.w } },
            { "NodeIds", area_.nodeIds } });
    }
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

// JSONからノードとLinkを復元し、次回発行IDと最小Graph構成を整える。
void AIBehaviorGraph::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::Deserialize");

    // 保存済みIDを復元しつつ、次に生成するIDが既存要素と衝突しないよう下限を更新する。
    Clear();
    nextId = json_.value("NextId", static_cast<uintptr_t>(1));
    priorityFromPositions = json_.value("PriorityFromPositions", true);

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
                if (nodeJson_["Position"].size() > 0)
                {
                    node_.position.x = nodeJson_["Position"][0].get<float>();
                }
                if (nodeJson_["Position"].size() > 1)
                {
                    node_.position.y = nodeJson_["Position"][1].get<float>();
                }
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
                if (nodeJson_["Vector2Value"].size() > 0)
                {
                    node_.vector2Value.x = nodeJson_["Vector2Value"][0].get<float>();
                }
                if (nodeJson_["Vector2Value"].size() > 1)
                {
                    node_.vector2Value.y = nodeJson_["Vector2Value"][1].get<float>();
                }
            }
            if (nodeJson_.contains("Vector3Value") && nodeJson_["Vector3Value"].is_array())
            {
                if (nodeJson_["Vector3Value"].size() > 0)
                {
                    node_.vector3Value.x = nodeJson_["Vector3Value"][0].get<float>();
                }
                if (nodeJson_["Vector3Value"].size() > 1)
                {
                    node_.vector3Value.y = nodeJson_["Vector3Value"][1].get<float>();
                }
                if (nodeJson_["Vector3Value"].size() > 2)
                {
                    node_.vector3Value.z = nodeJson_["Vector3Value"][2].get<float>();
                }
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

    // 領域を持たない旧Assetも従来どおり読み込み、IDはノード・ピン・リンクと共有する。
    if (json_.contains("Areas") && json_["Areas"].is_array())
    {
        for (const auto& areaJson_ : json_["Areas"])
        {
            AIBehaviorArea area_;
            area_.id = areaJson_.value("Id", static_cast<uintptr_t>(0));
            if (area_.id == 0 || FindNode(area_.id))
            {
                continue;
            }
            area_.name = areaJson_.value("Name", std::string("Action Area"));
            area_.comment = areaJson_.value("Comment", std::string{});
            const auto position_ = areaJson_.value("Position", std::vector<float>{ 0.0f, 0.0f });
            const auto size_ = areaJson_.value("Size", std::vector<float>{ 500.0f, 300.0f });
            const auto color_ = areaJson_.value("Color", std::vector<float>{ 0.2f, 0.35f, 0.55f, 0.18f });
            if (position_.size() == 2)
            {
                area_.position = { position_[0], position_[1] };
            }
            if (size_.size() == 2)
            {
                area_.size = { (std::max)(size_[0], 240.0f), (std::max)(size_[1], 160.0f) };
            }
            if (color_.size() == 4)
            {
                area_.color = { color_[0], color_[1], color_[2], color_[3] };
            }
            area_.nodeIds = areaJson_.value("NodeIds", std::vector<uintptr_t>{});
            areas.push_back(std::move(area_));
            SetIdCounterFloor(areas.back().id + 1);
        }
    }
    EnsureDefaultGraph();
}

void AIBehaviorGraph::RestoreEditorLayout(const nlohmann::json& json_)
{
    if (!json_.contains("Areas") || !json_["Areas"].is_array())
    {
        return;
    }
    // 外部Assetの行動・接続を優先し、シーンで編集した分類と座標だけを復元する。
    nlohmann::json merged_;
    Serialize(merged_);
    merged_["Areas"] = json_["Areas"];
    for (auto& area_ : merged_["Areas"])
    {
        if (!area_.contains("NodeIds") || !area_["NodeIds"].is_array())
        {
            continue;
        }
        auto& ids_ = area_["NodeIds"];
        ids_.erase(std::remove_if(ids_.begin(), ids_.end(), [this](const nlohmann::json& id_)
            {
            return !id_.is_number_integer() || !FindNode(id_.get<uintptr_t>());
        }), ids_.end());
        area_["Id"] = GenerateId();
    }
    if (json_.contains("Nodes") && json_["Nodes"].is_array())
    {
        for (auto& node_ : merged_["Nodes"])
        {
            for (const auto& saved_ : json_["Nodes"])
            {
                if (saved_.value("Id", uintptr_t{0}) == node_["Id"].get<uintptr_t>() &&
                    saved_.value("Name", std::string{}) == node_.value("Name", std::string{}) &&
                    saved_.contains("Position") && saved_["Position"].is_array() && saved_["Position"].size() == 2)
                {
                    node_["Position"] = saved_["Position"];
                    break;
                }
            }
        }
    }
    Deserialize(merged_);
}

// BehaviorGraphをJSONへ変換し、必要な親フォルダーを作成してファイル保存する。
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
    if (!file_.is_open())
    {
        return false;
    }
    file_ << json_.dump(4);
    return true;
}

// 指定JSONファイルを読み込み、BehaviorGraphの保存データへ復元する。
bool AIBehaviorGraph::LoadFromFile(const std::string& path_)
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::LoadFromFile");

    std::ifstream file_(path_);
    if (!file_.is_open())
    {
        return false;
    }

    nlohmann::json json_;
    {
        BS_PROFILE_SCOPE("AIBehaviorGraph::ReadJson");
        file_ >> json_;
    }
    Deserialize(json_);
    return true;
}

// ノードを実行中として記録し、CompositeまたはTaskの対応処理へ振り分ける。
AI_BEHAVIOR_STATUS AIBehaviorGraph::TickNode(AIBehaviorNode& node_, AIBehaviorContext& context_)
{
    runtimeActiveNodeIds.push_back(node_.id);

    const AI_BEHAVIOR_STATUS status_ = IsComposite(node_.type)
        ? TickComposite(node_, context_) : TickTask(node_, context_);
    if (context_.nodeEvaluationObserver)
    {
        context_.nodeEvaluationObserver(node_, status_);
    }
    return status_;
}

// 条件を満たすAction名を、副作用なしでRootから収集する。
std::vector<std::string> AIBehaviorGraph::GetSelectableActions(AIBehaviorContext& context_) const
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::GetSelectableActions");
    std::vector<std::string> actions_;
    std::vector<uintptr_t> path_;
    for (const AIBehaviorNode& node_ : nodes)
    {
        if (node_.type == AI_BEHAVIOR_NODE_TYPE::ROOT)
        {
            CollectSelectableActions(node_, context_, actions_, path_);
            break;
        }
    }
    return actions_;
}

bool AIBehaviorGraph::CollectSelectableActions(const AIBehaviorNode& node_, AIBehaviorContext& context_,
    std::vector<std::string>& actions_, std::vector<uintptr_t>& path_) const
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::CollectSelectableActions");
    if (std::find(path_.begin(), path_.end(), node_.id) != path_.end())
    {
        return false;
    }
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::BLACKBOARD_CONDITION)
    {
        const bool passed_ = EvaluateBlackboardCondition(node_, context_);
        // 抽選前に除外された枝の条件も、実際の事前評価結果として通知する。
        if (context_.nodeEvaluationObserver)
        {
            context_.nodeEvaluationObserver(node_,
            passed_ ? AI_BEHAVIOR_STATUS::SUCCESS : AI_BEHAVIOR_STATUS::FAILURE);
        }
        return passed_;
    }
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL)
    {
        BlackboardComponent* blackboard_ = ResolveBlackboard(node_, context_);
        bool value_ = false;
        return blackboard_ && blackboard_->GetBool(node_.blackboardCategory, node_.blackboardKey, value_) && value_ == node_.boolValue;
    }
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION)
    {
        actions_.push_back(node_.actionName);
        return true;
    }
    path_.push_back(node_.id);
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::ROOT || node_.type == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH)
    {
        // 実行時と同じPriority・ID順で先頭の子だけを調べる。
        const AIBehaviorNode* firstChild_ = nullptr;
        for (const AIBehaviorLink& link_ : links)
        {
            if (link_.fromNodeId != node_.id)
            {
                continue;
            }
            const AIBehaviorNode* child_ = FindNode(link_.toNodeId);
            if (child_ && (!firstChild_ || child_->priority < firstChild_->priority ||
                (child_->priority == firstChild_->priority && child_->id < firstChild_->id)))
            {
                firstChild_ = child_;
            }
        }
        const bool available_ = firstChild_ && CollectSelectableActions(*firstChild_, context_, actions_, path_);
        path_.pop_back();
        return available_;
    }
    std::vector<std::string> branchActions_;
    bool available_ = false;
    bool hasChildren_ = false;
    for (const AIBehaviorLink& link_ : links)
    {
        if (link_.fromNodeId != node_.id)
        {
            continue;
        }
        const AIBehaviorNode* child_ = FindNode(link_.toNodeId);
        if (!child_)
        {
            continue;
        }
        hasChildren_ = true;
        std::vector<std::string> childActions_;
        const bool childAvailable_ = CollectSelectableActions(*child_, context_, childActions_, path_);
        if (node_.type == AI_BEHAVIOR_NODE_TYPE::SEQUENCE && !childAvailable_)
        {
            path_.pop_back();
            return false;
        }
        if (node_.type == AI_BEHAVIOR_NODE_TYPE::RANDOM_SELECTOR && child_->weight <= 0.0f)
        {
            continue;
        }
        available_ = available_ || childAvailable_;
        if (childAvailable_)
        {
            branchActions_.insert(branchActions_.end(), childActions_.begin(), childActions_.end());
        }
    }
    path_.pop_back();
    // WAITなどの末端は条件を追加しない。事前判定は副作用を伴わないGraphを対象とする。
    if (!hasChildren_)
    {
        return true;
    }
    if (available_)
    {
        actions_.insert(actions_.end(), branchActions_.begin(), branchActions_.end());
    }
    return available_;
}

// 子ノードの結果を集約し、既存の優先順と抽選重みを維持する。
AI_BEHAVIOR_STATUS AIBehaviorGraph::TickComposite(AIBehaviorNode& node_, AIBehaviorContext& context_)
{
    std::vector<AIBehaviorNode*> children_ = GetChildren(node_);
    if (children_.empty())
    {
        return AI_BEHAVIOR_STATUS::FAILURE;
    }

    // Compositeは子ノードの順序や重みに従って、次に実行する枝を決める。
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::ROOT || node_.type == AI_BEHAVIOR_NODE_TYPE::SUB_GRAPH)
    {
        return TickNode(*children_.front(), context_);
    }

    AIBehaviorRuntimeState& state_ = runtimeStates[node_.id];

    // Sequenceは子を順番に実行し、途中状態をchildIndexへ保持して次Tickから再開する。
    if (node_.type == AI_BEHAVIOR_NODE_TYPE::SEQUENCE)
    {
        while (state_.childIndex < children_.size())
        {
            AI_BEHAVIOR_STATUS result_ = TickNode(*children_[state_.childIndex], context_);
            if (result_ == AI_BEHAVIOR_STATUS::RUNNING)
            {
                return result_;
            }
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
        BS_PROFILE_SCOPE("AIBehaviorGraph::TickRandomSelector");

        // 子のWeightを抽選確率として使用し、失敗した子を候補から外しながら再抽選する。
        static std::mt19937 randomEngineFallback_(std::random_device{}());
        std::vector<AIBehaviorNode*> remainingChildren_ = children_;
        std::unordered_map<uintptr_t, float> selectionWeights_;
        for (const AIBehaviorNode* child_ : children_)
        {
            float modifier_ = 1.0f;
            if (context_.actionWeightModifier)
            {
                std::vector<std::string> actions_;
                std::vector<uintptr_t> path_;
                if (!CollectSelectableActions(*child_, context_, actions_, path_))
                {
                    modifier_ = 0.0f;
                }
                else if (!actions_.empty())
                {
                    // 複数攻撃をまとめた枝は最大補正を使い、枝内では再び個別抽選する。
                    modifier_ = 0.0f;
                    for (const std::string& action_ : actions_)
                    {
                        modifier_ = (std::max)(modifier_, context_.actionWeightModifier(action_));
                    }
                    if (actions_.size() == 1 && context_.actionWeightObserver)
                    {
                        context_.actionWeightObserver(actions_.front(), child_->weight, modifier_);
                    }
                }
            }
            selectionWeights_[child_->id] = (std::max)(0.0f, child_->weight) * modifier_;
        }
        if (context_.actionWeightModifier)
        {
            remainingChildren_.erase(std::remove_if(remainingChildren_.begin(), remainingChildren_.end(),
                [&selectionWeights_](const AIBehaviorNode* child_)
            {
                return selectionWeights_[child_->id] <= 0.0f;
            }), remainingChildren_.end());
        }
        while (!remainingChildren_.empty())
        {
            float totalWeight_ = 0.0f;
            for (const AIBehaviorNode* child_ : remainingChildren_)
            {
                totalWeight_ += selectionWeights_[child_->id];
            }

            size_t selectedIndex_ = 0;
            if (totalWeight_ > 0.0001f)
            {
                std::uniform_real_distribution<float> distribution_(0.0f, totalWeight_);
                float pick_ = distribution_(randomEngineFallback_);
                for (size_t childIndex_ = 0; childIndex_ < remainingChildren_.size(); ++childIndex_)
                {
                    pick_ -= selectionWeights_[remainingChildren_[childIndex_]->id];
                    if (pick_ <= 0.0f)
                    {
                        selectedIndex_ = childIndex_;
                        break;
                    }
                }
            }

            AIBehaviorNode* selectedChild_ = remainingChildren_[selectedIndex_];
            remainingChildren_.erase(remainingChildren_.begin() + selectedIndex_);

            const AI_BEHAVIOR_STATUS result_ = TickNode(*selectedChild_, context_);
            if (result_ == AI_BEHAVIOR_STATUS::RUNNING)
            {
                return result_;
            }
            if (result_ == AI_BEHAVIOR_STATUS::SUCCESS)
            {
                return AI_BEHAVIOR_STATUS::SUCCESS;
            }
        }

        return AI_BEHAVIOR_STATUS::FAILURE;

    }

    // 通常のSelectorは優先順に評価し、最初のSuccessまたはRunningを親の結果にする。
    for (AIBehaviorNode* child_ : children_)
    {
        AI_BEHAVIOR_STATUS result_ = TickNode(*child_, context_);
        if (result_ == AI_BEHAVIOR_STATUS::RUNNING)
        {
            return result_;
        }
        if (result_ == AI_BEHAVIOR_STATUS::SUCCESS)
        {
            return AI_BEHAVIOR_STATUS::SUCCESS;
        }
    }

    return AI_BEHAVIOR_STATUS::FAILURE;
}

// Condition、Blackboard操作、Action実行、待機など末端ノードの処理を実行する。
AI_BEHAVIOR_STATUS AIBehaviorGraph::TickTask(AIBehaviorNode& node_, AIBehaviorContext& context_)
{
    BlackboardComponent& blackboard_ = *context_.blackboard;
    AIBehaviorRuntimeState& state_ = runtimeStates[node_.id];

    // TaskノードはBlackboard操作、Action実行、待機など実際の副作用を担当する。
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
        // Actionは最初のTickで一度だけ開始し、以降は終了フラグの監視だけを行う。
        if (!state_.actionStarted)
        {
            // 除外された攻撃を通常Selectorから直接開始する経路も止める。
            if (context_.actionWeightModifier && context_.actionWeightModifier(node_.actionName) <= 0.0f)
            {
                return AI_BEHAVIOR_STATUS::FAILURE;
            }
            bool executed_ = false;
            // AIActionComponentがあればBlackboard・Animation連携込みで実行する。
            if (auto aiActionComponent_ = context_.owner->GetComponent<AIActionComponent>())
            {
                executed_ = aiActionComponent_->ExecuteAIAction(node_.actionName);
            }
            // 専用定義がないActionは、Ownerの通常Actionとして直接実行する。
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
        // 経過時間をRuntimeStateへ蓄積し、指定秒数に到達するまでRunningを返す。
        state_.elapsedTime += context_.deltaTime;
        if (state_.elapsedTime >= node_.waitSeconds)
        {
            state_ = {};
            return AI_BEHAVIOR_STATUS::SUCCESS;
        }
        return AI_BEHAVIOR_STATUS::RUNNING;

    case AI_BEHAVIOR_NODE_TYPE::WAIT_BLACKBOARD_BOOL:
    {
        // Scope設定から監視対象Blackboardを選び、指定Boolが期待値になるまで待機する。
        BlackboardComponent* targetBlackboard_ = ResolveBlackboard(node_, context_);
        if (!targetBlackboard_)
        {
            return AI_BEHAVIOR_STATUS::RUNNING;
        }

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

// Linkから直接接続された子ノードを集め、PriorityとIDの順に並べて返す。
std::vector<AIBehaviorNode*> AIBehaviorGraph::GetChildren(const AIBehaviorNode& node_)
{
    std::vector<AIBehaviorNode*> children_;
    for (const AIBehaviorLink& link_ : links)
    {
        if (link_.fromNodeId != node_.id)
        {
            continue;
        }
        AIBehaviorNode* child_ = FindNode(link_.toNodeId);
        if (child_)
        {
            children_.push_back(child_);
        }
    }

    // Priorityの小さいノードを先に評価し、同値の場合はIDで順序を安定させる。
    std::sort(children_.begin(), children_.end(),
        [](const AIBehaviorNode* left_, const AIBehaviorNode* right_)
        {
            if (left_->priority != right_->priority)
        {
            return left_->priority < right_->priority;
        }
        return left_->id < right_->id;
        });
    return children_;
}

// ノードのScope設定に応じてLocalまたはGlobal Blackboardを選択する。
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

// Blackboardの現在値とノードに設定された比較値を照合し、条件成立可否を返す。
bool AIBehaviorGraph::EvaluateBlackboardCondition(
    const AIBehaviorNode& node_,
    AIBehaviorContext& context_) const
{
    BlackboardComponent* blackboard_ = ResolveBlackboard(node_, context_);
    if (!blackboard_)
    {
        return false;
    }

    // 文字列は文字列として比較し、それ以外は数値へ寄せて大小比較できるようにする。
    // キーの存在確認にも使うため、型変換前のBlackBoardValueを取得する。
    const BlackBoardValue* value_ = blackboard_->GetValue(node_.blackboardCategory, node_.blackboardKey);

    if (node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::EXISTS)
    {
        return value_ != nullptr;
    }
    if (node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::NOT_EXISTS)
    {
        return value_ == nullptr;
    }
    if (!value_)
    {
        return false;
    }

    if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::STRING)
    {
        std::string stringValue_;
        if (!blackboard_->GetString(node_.blackboardCategory, node_.blackboardKey, stringValue_))
        {
            return false;
        }
        const bool equal_ = stringValue_ == node_.stringValue;
        return node_.compareType == AI_BLACKBOARD_COMPARE_TYPE::NOT_EQUAL ? !equal_ : equal_;
    }

    // 数値・Boolはfloatへ正規化し、共通の大小比較処理へ流す。
    const float currentValue_ = BlackboardComponent::ToFloat(*value_);
    float compareValue_ = node_.floatValue;
    if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::INT)
    {
        compareValue_ = static_cast<float>(node_.intValue);
    }
    if (node_.valueType == AI_BLACKBOARD_VALUE_TYPE::BOOL)
    {
        compareValue_ = node_.boolValue ? 1.0f : 0.0f;
    }

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

// ノードに保存された型と値を、選択されたBlackboardへ書き込む。
void AIBehaviorGraph::SetBlackboardValue(
    const AIBehaviorNode& node_,
    AIBehaviorContext& context_) const
{
    BlackboardComponent* blackboard_ = ResolveBlackboard(node_, context_);
    if (!blackboard_)
    {
        return;
    }

    // SET_BLACKBOARDノードで選ばれた型に合わせて、LocalまたはGlobalのBlackboardへ値を書き込む。
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

// Action終了監視用のカテゴリとキーを読み、終了フラグがtrueかを確認する。
bool AIBehaviorGraph::IsActionEndReached(const AIBehaviorNode& node_, BlackboardComponent& blackboard_) const
{
    bool ended_ = false;
    if (!blackboard_.GetBool(node_.actionEndCategory, node_.actionEndKey, ended_))
    {
        return false;
    }
    return ended_;
}

// Graph実行の開始地点となるRootノードを検索する。
AIBehaviorNode* AIBehaviorGraph::FindRootNode()
{
    for (AIBehaviorNode& node_ : nodes)
    {
        if (node_.type == AI_BEHAVIOR_NODE_TYPE::ROOT)
        {
            return &node_;
        }
    }
    return nullptr;
}

// 領域の余白にタイトルを置き、ノードとピンを枠の内側へ収める。
void AIBehaviorGraph::FitArea(AIBehaviorArea& area_)
{
    bool found_ = false;
    float left_ = 0.0f, top_ = 0.0f, right_ = 0.0f, bottom_ = 0.0f;
    for (const uintptr_t nodeId_ : area_.nodeIds)
    {
        const auto node_ = FindNode(nodeId_);
        if (!node_)
        {
            continue;
        }
        if (!found_)
        {
            left_ = right_ = node_->position.x;
            top_ = bottom_ = node_->position.y;
            found_ = true;
        }
        left_ = (std::min)(left_, node_->position.x);
        right_ = (std::max)(right_, node_->position.x + 210.0f);
        top_ = (std::min)(top_, node_->position.y);
        bottom_ = (std::max)(bottom_, node_->position.y + 112.0f);
    }
    if (!found_)
    {
        return;
    }
    // 説明文を置く上側の余白と、入れ子の行動Boxを収める外側の余白を確保する。
    const bool singleNode_ = area_.nodeIds.size() == 1;
    const float sidePadding_ = singleNode_ ? 32.0f : 64.0f;
    const float topPadding_ = singleNode_ ? 100.0f : 120.0f;
    const float bottomPadding_ = singleNode_ ? 32.0f : 64.0f;
    area_.position = { left_ - sidePadding_, top_ - topPadding_ };
    area_.size = { right_ - left_ + sidePadding_ * 2.0f, bottom_ - top_ + topPadding_ + bottomPadding_ };
}

void AIBehaviorGraph::OrganizeActionAreas()
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::OrganizeActionAreas");
    // 行動と条件を一緒に囲んだBoxでは、既存の分類を保って再整列する。
    bool partitioned_ = !areas.empty();
    std::vector<uintptr_t> assigned_;
    for (const auto& area_ : areas)
    {
        if (area_.nodeIds.empty())
        {
            partitioned_ = false;
        }
        for (const uintptr_t id_ : area_.nodeIds)
        {
            if (!FindNode(id_) || std::find(assigned_.begin(), assigned_.end(), id_) != assigned_.end())
            {
                partitioned_ = false;
            }
            assigned_.push_back(id_);
        }
    }
    if (partitioned_)
    {
        ArrangeExistingAreas();
        return;
    }
    auto root_ = FindRootNode();
    if (!root_)
    {
        return;
    }
    auto rootChildren_ = GetChildren(*root_);
    if (rootChildren_.empty())
    {
        return;
    }
    auto selector_ = rootChildren_.front();
    auto branches_ = GetChildren(*selector_);
    const bool selectorIsBranch_ = branches_.empty();
    if (selectorIsBranch_)
    {
        branches_.push_back(selector_);
    }
    // 自動整列では座標と判断順を切り離し、SelectorとSequenceの既存Priorityを一切変更しない。
    priorityFromPositions = false;
    // 再整列しても、行動ごとに編集したタイトル・説明・色は維持する。
    const auto previousAreas_ = areas;
    areas.clear();
    std::vector<uintptr_t> placed_{ root_->id };
    if (selector_ != root_ && !selectorIsBranch_)
    {
        placed_.push_back(selector_->id);
    }
    // 独立した行動を下の段へ折り返さず、条件と派生だけを同じ枝の下方向へ配置する。
    constexpr float AREA_TOP = 500.0f;
    constexpr float BRANCH_NODE_TOP = 620.0f;
    float columnLeft_ = 0.0f;
    for (std::size_t branchIndex_ = 0; branchIndex_ < branches_.size(); ++branchIndex_)
    {
        AIBehaviorArea area_;
        area_.id = GenerateId();
        area_.name = std::to_string(branchIndex_ + 1) + ". " + branches_[branchIndex_]->name;
        area_.comment = "Priority順に評価する行動の枝。配置変更は判断順へ影響しません。";
        const float hue_ = static_cast<float>(branchIndex_ % 4) * 0.08f;
        area_.color = { 0.16f + hue_, 0.30f, 0.48f - hue_ * 0.5f, 0.18f };
        float nextLeafX_ = columnLeft_ + 50.0f;
        std::function<float(AIBehaviorNode&, int)> place_ = [&](AIBehaviorNode& node_, int depth_)
        {
            if (std::find(placed_.begin(), placed_.end(), node_.id) != placed_.end())
            {
                return node_.position.x;
            }
            placed_.push_back(node_.id);
            area_.nodeIds.push_back(node_.id);
            const auto children_ = GetChildren(node_);
            float firstX_ = nextLeafX_, lastX_ = nextLeafX_;
            bool first_ = true;
            for (auto child_ : children_)
            {
                const float childX_ = place_(*child_, depth_ + 1);
                if (first_)
                {
                    firstX_ = childX_;
                    first_ = false;
                }
                lastX_ = childX_;
            }
            const float x_ = children_.empty() ? nextLeafX_ : (firstX_ + lastX_) * 0.5f;
            if (children_.empty())
            {
                nextLeafX_ += 340.0f;
            }
            node_.position = { x_, BRANCH_NODE_TOP + static_cast<float>(depth_) * 280.0f };
            return x_;
        };
        place_(*branches_[branchIndex_], 0);
        FitArea(area_);
        const float alignX_ = columnLeft_ - area_.position.x;
        const float alignY_ = AREA_TOP - area_.position.y;
        for (const uintptr_t id_ : area_.nodeIds)
        {
            if (auto node_ = FindNode(id_))
            {
                node_->position.x += alignX_;
                node_->position.y += alignY_;
            }
        }
        area_.position = { columnLeft_, AREA_TOP };
        columnLeft_ = area_.position.x + area_.size.x + 160.0f;
        areas.push_back(std::move(area_));
    }
    // 大分類の内側に行動ごとのコメントBoxを作り、個々の攻撃や防御を見分けられるようにする。
    std::vector<AIBehaviorArea> actionAreas_;
    for (const auto& node_ : nodes)
    {
        if (node_.type != AI_BEHAVIOR_NODE_TYPE::PLAY_ACTION)
        {
            continue;
        }
        const bool alreadySingle_ = std::any_of(areas.begin(), areas.end(),
            [&node_](const AIBehaviorArea& area_)
            {
            return area_.nodeIds.size() == 1 && area_.nodeIds.front() == node_.id;
        });
        const auto previous_ = std::find_if(previousAreas_.begin(), previousAreas_.end(),
            [&node_](const AIBehaviorArea& area_)
            {
            return area_.nodeIds.size() == 1 && area_.nodeIds.front() == node_.id;
        });
        if (alreadySingle_)
        {
            if (previous_ != previousAreas_.end())
            {
                for (auto& area_ : areas)
                {
                    if (area_.nodeIds.size() == 1 && area_.nodeIds.front() == node_.id)
                    {
                        area_.name = previous_->name;
                        area_.comment = previous_->comment;
                        area_.color = previous_->color;
                        break;
                    }
                }
            }
            continue;
        }
        AIBehaviorArea actionArea_;
        actionArea_.id = GenerateId();
        actionArea_.name = previous_ != previousAreas_.end() ? previous_->name : node_.name;
        actionArea_.comment = previous_ != previousAreas_.end() ? previous_->comment :
            (node_.comment.empty() ? "実行Action: " + node_.actionName : node_.comment);
        actionArea_.nodeIds.push_back(node_.id);
        actionArea_.color = previous_ != previousAreas_.end() ? previous_->color : DirectX::XMFLOAT4{ 0.20f, 0.42f, 0.48f, 0.24f };
        FitArea(actionArea_);
        actionAreas_.push_back(std::move(actionArea_));
    }
    areas.insert(areas.end(), actionAreas_.begin(), actionAreas_.end());
    // 直下の枝だけでなく派生を含むBox全体の幅を使い、Rootの中心を左右の中央へ合わせる。
    float layoutLeft_ = areas.front().position.x;
    float layoutRight_ = layoutLeft_ + areas.front().size.x;
    for (const auto& area_ : areas)
    {
        layoutLeft_ = (std::min)(layoutLeft_, area_.position.x);
        layoutRight_ = (std::max)(layoutRight_, area_.position.x + area_.size.x);
    }
    constexpr float ROOT_NODE_WIDTH = 190.0f;
    const float decisionX_ = (layoutLeft_ + layoutRight_ - ROOT_NODE_WIDTH) * 0.5f;
    root_->position = { decisionX_, 0.0f };
    if (!selectorIsBranch_)
    {
        selector_->position = { decisionX_, 180.0f };
    }
    // 未接続ノードは行動列の右端へ残し、無関係な行動の下へ配置しない。
    float unlinkedX_ = columnLeft_;
    for (auto& node_ : nodes)
    {
        if (std::find(placed_.begin(), placed_.end(), node_.id) == placed_.end())
        {
            node_.position = { unlinkedX_, BRANCH_NODE_TOP };
            unlinkedX_ += 340.0f;
        }
    }
}

void AIBehaviorGraph::ArrangeExistingAreas()
{
    BS_PROFILE_SCOPE("AIBehaviorGraph::ArrangeExistingAreas");
    priorityFromPositions = false;
    std::vector<uintptr_t> placed_;
    // Boxごとの横方向の領域を分け、縦方向はBox内の条件・派生だけに使う。
    constexpr float BOX_TOP = 500.0f;
    constexpr float BOX_NODE_TOP = 620.0f;
    float columnLeft_ = 0.0f;
    for (std::size_t areaIndex_ = 0; areaIndex_ < areas.size(); ++areaIndex_)
    {
        auto& area_ = areas[areaIndex_];
        float leafX_ = columnLeft_ + 80.0f;
        std::function<float(AIBehaviorNode&, int)> place_ = [&](AIBehaviorNode& node_, int depth_)
        {
            if (std::find(placed_.begin(), placed_.end(), node_.id) != placed_.end())
            {
                return node_.position.x;
            }
            placed_.push_back(node_.id);
            std::vector<AIBehaviorNode*> children_;
            for (auto child_ : GetChildren(node_))
            {
                if (std::find(area_.nodeIds.begin(), area_.nodeIds.end(), child_->id) != area_.nodeIds.end())
                {
                    children_.push_back(child_);
                }
            }
            float firstX_ = leafX_, lastX_ = leafX_;
            bool first_ = true;
            for (auto child_ : children_)
            {
                const float childX_ = place_(*child_, depth_ + 1);
                if (first_)
                {
                    firstX_ = childX_;
                    first_ = false;
                }
                lastX_ = childX_;
            }
            const float x_ = children_.empty() ? leafX_ : (firstX_ + lastX_) * 0.5f;
            if (children_.empty())
            {
                leafX_ += 340.0f;
            }
            node_.position = { x_, BOX_NODE_TOP + static_cast<float>(depth_) * 280.0f };
            return x_;
        };
        // Box外から入るリンクは表示用の接続として残し、Box内の根から配置する。
        for (const uintptr_t id_ : area_.nodeIds)
        {
            const bool hasLocalParent_ = std::any_of(links.begin(), links.end(),
                [&area_, id_](const AIBehaviorLink& link_)
                {
                return link_.toNodeId == id_ && std::find(area_.nodeIds.begin(), area_.nodeIds.end(), link_.fromNodeId) != area_.nodeIds.end();
            });
            if (!hasLocalParent_)
            {
                if (auto node_ = FindNode(id_))
                {
                    place_(*node_, 0);
                }
            }
        }
        for (const uintptr_t id_ : area_.nodeIds)
        {
            if (std::find(placed_.begin(), placed_.end(), id_) == placed_.end())
            {
                if (auto node_ = FindNode(id_))
                {
                    place_(*node_, 0);
                }
            }
        }
        FitArea(area_);
        const float alignX_ = columnLeft_ - area_.position.x;
        const float alignY_ = BOX_TOP - area_.position.y;
        for (const uintptr_t id_ : area_.nodeIds)
        {
            if (auto node_ = FindNode(id_))
            {
                node_->position.x += alignX_;
                node_->position.y += alignY_;
            }
        }
        area_.position = { columnLeft_, BOX_TOP };
        columnLeft_ = area_.position.x + area_.size.x + 160.0f;
    }
    // 横方向の専用列は保ち、実際に接続された派生Boxだけを下へ伸ばす。
    std::unordered_map<uintptr_t, std::size_t> nodeAreas_;
    for (std::size_t index_ = 0; index_ < areas.size(); ++index_)
    {
        for (const uintptr_t id_ : areas[index_].nodeIds)
        {
            nodeAreas_[id_] = index_;
        }
    }
    std::vector<std::vector<std::size_t>> successors_(areas.size());
    std::vector<std::size_t> incoming_(areas.size(), 0);
    std::vector<std::size_t> levels_(areas.size(), 0);
    for (const auto& link_ : links)
    {
        const auto from_ = nodeAreas_.find(link_.fromNodeId);
        const auto to_ = nodeAreas_.find(link_.toNodeId);
        if (from_ == nodeAreas_.end() || to_ == nodeAreas_.end() || from_->second == to_->second)
        {
            continue;
        }
        auto& destinations_ = successors_[from_->second];
        if (std::find(destinations_.begin(), destinations_.end(), to_->second) != destinations_.end())
        {
            continue;
        }
        destinations_.push_back(to_->second);
        ++incoming_[to_->second];
    }
    std::vector<std::size_t> ordered_;
    for (std::size_t index_ = 0; index_ < areas.size(); ++index_)
    {
        if (incoming_[index_] == 0)
        {
            ordered_.push_back(index_);
        }
    }
    for (std::size_t cursor_ = 0; cursor_ < ordered_.size(); ++cursor_)
    {
        for (const std::size_t child_ : successors_[ordered_[cursor_]])
        {
            levels_[child_] = (std::max)(levels_[child_], levels_[ordered_[cursor_]] + 1);
            if (--incoming_[child_] == 0)
            {
                ordered_.push_back(child_);
            }
        }
    }
    // 循環するBox関係は段数を増やし続けず、横置きの位置を維持する。
    if (!areas.empty() && ordered_.size() == areas.size())
    {
        std::vector<float> levelHeights_(areas.size(), 0.0f);
        std::vector<float> levelTops_(areas.size(), BOX_TOP);
        for (std::size_t index_ = 0; index_ < areas.size(); ++index_)
        {
            levelHeights_[levels_[index_]] = (std::max)(levelHeights_[levels_[index_]], areas[index_].size.y);
        }
        for (std::size_t level_ = 1; level_ < levelTops_.size(); ++level_)
        {
            levelTops_[level_] = levelTops_[level_ - 1] + levelHeights_[level_ - 1] + 180.0f;
        }
        for (std::size_t index_ = 0; index_ < areas.size(); ++index_)
        {
            auto& area_ = areas[index_];
            const float deltaY_ = levelTops_[levels_[index_]] - area_.position.y;
            for (const uintptr_t id_ : area_.nodeIds)
            {
                if (auto node_ = FindNode(id_))
                {
                    node_->position.y += deltaY_;
                }
            }
            area_.position.y += deltaY_;
        }
    }
    // Boxへ含めていないRootと最上位Selectorは、判断全体の入口として上段へ残す。
    auto root_ = FindRootNode();
    if (root_ && std::find(placed_.begin(), placed_.end(), root_->id) == placed_.end())
    {
        // 空中・地上など直下の判断Boxが左端にあっても、派生した全行動Boxの範囲で中央を決める。
        float layoutLeft_ = areas.empty() ? 0.0f : areas.front().position.x;
        float layoutRight_ = areas.empty() ? 0.0f : layoutLeft_ + areas.front().size.x;
        for (const auto& area_ : areas)
        {
            layoutLeft_ = (std::min)(layoutLeft_, area_.position.x);
            layoutRight_ = (std::max)(layoutRight_, area_.position.x + area_.size.x);
        }
        constexpr float ROOT_NODE_WIDTH = 190.0f;
        const float decisionX_ = areas.empty() ? 600.0f : (layoutLeft_ + layoutRight_ - ROOT_NODE_WIDTH) * 0.5f;
        root_->position = { decisionX_, 0.0f };
        placed_.push_back(root_->id);
        const auto children_ = GetChildren(*root_);
        if (!children_.empty() && std::find(placed_.begin(), placed_.end(), children_.front()->id) == placed_.end())
        {
            children_.front()->position = { decisionX_, 200.0f };
            placed_.push_back(children_.front()->id);
        }
    }
    float unassignedX_ = columnLeft_;
    for (auto& node_ : nodes)
    {
        if (std::find(placed_.begin(), placed_.end(), node_.id) == placed_.end())
        {
            node_.position = { unassignedX_, BOX_NODE_TOP };
            unassignedX_ += 340.0f;
        }
    }
}
