#pragma once

#include "AIBlackboardKeys.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

#include <DirectXMath.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;

// AIの判断グラフで使用するノード種別
enum class AI_BEHAVIOR_NODE_TYPE
{
    ROOT,
    SELECTOR,
    SEQUENCE,
    RANDOM_SELECTOR,
    BLACKBOARD_CONDITION,
    SET_BLACKBOARD,
    PLAY_ACTION,
    WAIT,
    WAIT_BLACKBOARD_BOOL,
    SUB_GRAPH,
};

enum class AI_BLACKBOARD_COMPARE_TYPE
{
    EXISTS,
    NOT_EXISTS,
    EQUAL,
    NOT_EQUAL,
    GREATER,
    GREATER_EQUAL,
    LESS,
    LESS_EQUAL,
};

enum class AI_BLACKBOARD_VALUE_TYPE
{
    FLOAT,
    INT,
    BOOL,
    VECTOR2,
    VECTOR3,
    STRING,
    TRIGGER,
};

enum class AI_BLACKBOARD_SCOPE
{
    LOCAL,
    GLOBAL,
};

enum class AI_BEHAVIOR_STATUS
{
    SUCCESS,
    FAILURE,
    RUNNING,
};

// AIの判断グラフ上に配置される1つのノード情報
struct AIBehaviorNode
{
    uintptr_t id = 0;
    uintptr_t inputPinId = 0;
    uintptr_t outputPinId = 0;
    AI_BEHAVIOR_NODE_TYPE type = AI_BEHAVIOR_NODE_TYPE::SELECTOR;
    std::string name = "Node";
    std::string comment;
    std::string scopeId = "Root";
    DirectX::XMFLOAT2 position = { 0.0f, 0.0f };

    std::string blackboardCategory = AIBlackboardKeys::CATEGORY_AI;
    std::string blackboardKey;
    AI_BLACKBOARD_SCOPE blackboardScope = AI_BLACKBOARD_SCOPE::LOCAL;
    AI_BLACKBOARD_COMPARE_TYPE compareType = AI_BLACKBOARD_COMPARE_TYPE::EQUAL;
    AI_BLACKBOARD_VALUE_TYPE valueType = AI_BLACKBOARD_VALUE_TYPE::BOOL;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    DirectX::XMFLOAT2 vector2Value = { 0.0f, 0.0f };
    DirectX::XMFLOAT3 vector3Value = { 0.0f, 0.0f, 0.0f };
    std::string stringValue;

    std::string actionName;
    bool waitForActionEnd = true;
    std::string actionEndCategory = AIBlackboardKeys::CATEGORY_AI;
    std::string actionEndKey = AIBlackboardKeys::ACTION_ENDED;

    float waitSeconds = 0.0f;
    std::string subGraphName;
    int priority = 0;
    float weight = 1.0f;
};

// ノード同士の接続情報を保持するリンク
struct AIBehaviorLink
{
    uintptr_t id = 0;
    uintptr_t fromNodeId = 0;
    uintptr_t toNodeId = 0;
    uintptr_t fromPinId = 0;
    uintptr_t toPinId = 0;
};

// 実行中ノードの経過時間や進行位置を保持する
struct AIBehaviorRuntimeState
{
    float elapsedTime = 0.0f;
    size_t childIndex = 0;
    bool actionStarted = false;
};

// AIグラフの実行時に参照する対象やBlackboard情報をまとめる
struct AIBehaviorContext
{
    GameObject* owner = nullptr;
    GameObject* target = nullptr;
    BlackboardComponent* blackboard = nullptr;
    BlackboardComponent* globalBlackboard = nullptr;
    DirectX::XMFLOAT3 ownerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 spawnPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 toTargetDirection = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 toSpawnDirection = { 0.0f, 0.0f, 0.0f };
    float targetDistance = 0.0f;
    float spawnDistance = 0.0f;
    float deltaTime = 0.0f;
    bool hasTarget = false;
};

// BlackboardとActionを使用して敵AIの判断グラフを実行する
class AIBehaviorGraph
{
public:
    void Clear();
    void EnsureDefaultGraph();
    AI_BEHAVIOR_STATUS Tick(AIBehaviorContext& context_);
    void ResetRuntime();
    bool IsNodeRuntimeActive(uintptr_t nodeId_) const;

    AIBehaviorNode* AddNode(AI_BEHAVIOR_NODE_TYPE type_, const std::string& name_, const DirectX::XMFLOAT2& position_);
    bool RemoveNode(uintptr_t nodeId_);
    AIBehaviorLink* AddLink(uintptr_t fromNodeId_, uintptr_t toNodeId_);
    bool RemoveLink(uintptr_t linkId_);

    AIBehaviorNode* FindNode(uintptr_t nodeId_);
    const AIBehaviorNode* FindNode(uintptr_t nodeId_) const;
    AIBehaviorNode* FindNodeByPin(uintptr_t pinId_);
    const AIBehaviorNode* FindNodeByPin(uintptr_t pinId_) const;
    AIBehaviorLink* FindLink(uintptr_t linkId_);

    uintptr_t GenerateId();
    void SetIdCounterFloor(uintptr_t minValue_);

    void Serialize(nlohmann::json& json_) const;
    void Deserialize(const nlohmann::json& json_);
    bool SaveToFile(const std::string& path_) const;
    bool LoadFromFile(const std::string& path_);

    std::vector<AIBehaviorNode>& GetNodes() { return nodes; }
    const std::vector<AIBehaviorNode>& GetNodes() const { return nodes; }
    std::vector<AIBehaviorLink>& GetLinks() { return links; }
    const std::vector<AIBehaviorLink>& GetLinks() const { return links; }
    const std::vector<uintptr_t>& GetRuntimeActiveNodeIds() const { return runtimeActiveNodeIds; }
    AI_BEHAVIOR_STATUS GetLastTickStatus() const { return lastTickStatus; }

private:
    AI_BEHAVIOR_STATUS TickNode(AIBehaviorNode& node_, AIBehaviorContext& context_);
    AI_BEHAVIOR_STATUS TickComposite(AIBehaviorNode& node_, AIBehaviorContext& context_);
    AI_BEHAVIOR_STATUS TickTask(AIBehaviorNode& node_, AIBehaviorContext& context_);
    std::vector<AIBehaviorNode*> GetChildren(const AIBehaviorNode& node_);
    BlackboardComponent* ResolveBlackboard(const AIBehaviorNode& node_, AIBehaviorContext& context_) const;
    bool EvaluateBlackboardCondition(const AIBehaviorNode& node_, AIBehaviorContext& context_) const;
    void SetBlackboardValue(const AIBehaviorNode& node_, AIBehaviorContext& context_) const;
    bool IsActionEndReached(const AIBehaviorNode& node_, BlackboardComponent& blackboard_) const;
    AIBehaviorNode* FindRootNode();

private:
    std::vector<AIBehaviorNode> nodes;
    std::vector<AIBehaviorLink> links;
    std::unordered_map<uintptr_t, AIBehaviorRuntimeState> runtimeStates;
    std::vector<uintptr_t> runtimeActiveNodeIds;
    AI_BEHAVIOR_STATUS lastTickStatus = AI_BEHAVIOR_STATUS::FAILURE;
    uintptr_t nextId = 1;
};
