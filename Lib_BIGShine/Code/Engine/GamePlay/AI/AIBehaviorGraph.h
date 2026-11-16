#pragma once

#include "AIBlackboardKeys.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

#include <DirectXMath.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class GameObject;

// AIの判断グラフで使用するノード種別
// ノードグラフでAIの判断順を表すための種類。
// Composite、Condition、Actionを分けることで、敵ごとの差分をJSON側に逃がしやすくしている。
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

// BlackboardConditionノードで使用する比較方法。
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

// Blackboardへ読み書きする値の型。
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

// ノードが参照するBlackboardの範囲。
enum class AI_BLACKBOARD_SCOPE
{
    LOCAL,
    GLOBAL,
};

// ノード実行結果。RUNNINGは次Tickでも処理を継続することを示す。
enum class AI_BEHAVIOR_STATUS
{
    SUCCESS,
    FAILURE,
    RUNNING,
};

// AIの判断グラフ上に配置される1つのノード情報
// Editor上の1ノード分の保存データ。条件用の値、Action名、Wait時間などをここに集約する。
struct AIBehaviorNode
{
    // EditorとRuntimeの両方でノードを識別する一意ID。
    uintptr_t id = 0;
    // 親Linkを受け取る入力PinのID。Rootでは使用しない。
    uintptr_t inputPinId = 0;
    // 子Linkを接続する出力PinのID。Taskノードでは使用しない。
    uintptr_t outputPinId = 0;
    // ノードの実行方法を決める種別。
    AI_BEHAVIOR_NODE_TYPE type = AI_BEHAVIOR_NODE_TYPE::SELECTOR;
    // Editor上に表示するノード名。
    std::string name = "Node";
    // ノードの用途をEditor上で補足する自由記述。
    std::string comment;
    // SubGraphやEditor上の所属範囲を識別する文字列。
    std::string scopeId = "Root";
    // NodeEditor上での表示位置。
    DirectX::XMFLOAT2 position = { 0.0f, 0.0f };

    // ConditionまたはSet処理で使用するBlackboardカテゴリ。
    std::string blackboardCategory = AIBlackboardKeys::CATEGORY_AI;
    // 読み書き対象となるBlackboardキー。
    std::string blackboardKey;
    // LocalとGlobalのどちらのBlackboardを参照するかを指定する。
    AI_BLACKBOARD_SCOPE blackboardScope = AI_BLACKBOARD_SCOPE::LOCAL;
    // Conditionノードで使用する比較方法。
    AI_BLACKBOARD_COMPARE_TYPE compareType = AI_BLACKBOARD_COMPARE_TYPE::EQUAL;
    // 比較値または書き込み値として使用するデータ型。
    AI_BLACKBOARD_VALUE_TYPE valueType = AI_BLACKBOARD_VALUE_TYPE::BOOL;
    float floatValue = 0.0f;
    int intValue = 0;
    bool boolValue = false;
    DirectX::XMFLOAT2 vector2Value = { 0.0f, 0.0f };
    DirectX::XMFLOAT3 vector3Value = { 0.0f, 0.0f, 0.0f };
    std::string stringValue;

    // PLAY_ACTIONノードが実行するAction名。
    std::string actionName;
    // Action終了フラグを待ってからSuccessにするかを指定する。
    bool waitForActionEnd = true;
    // Action終了判定を読むBlackboardカテゴリ。
    std::string actionEndCategory = AIBlackboardKeys::CATEGORY_AI;
    // Action終了判定を読むBoolキー。
    std::string actionEndKey = AIBlackboardKeys::ACTION_ENDED;

    // WAITノードが継続する秒数。
    float waitSeconds = 0.0f;
    // SUB_GRAPHノードが参照するGraph名。
    std::string subGraphName;
    // 同じ親へ接続された子ノード同士の評価順。小さい値を先に評価する。
    int priority = 0;
    // RANDOM_SELECTORで選択確率として使用する重み。
    float weight = 1.0f;
};

// ノード同士の接続情報を保持するリンク
struct AIBehaviorLink
{
    // EditorとRuntimeの両方でノードを識別する一意ID。
    uintptr_t id = 0;
    uintptr_t fromNodeId = 0;
    uintptr_t toNodeId = 0;
    uintptr_t fromPinId = 0;
    uintptr_t toPinId = 0;
};

// 実行順に影響させず、関連ノードを名前と色で囲む編集用の領域。
struct AIBehaviorArea
{
    uintptr_t id = 0;
    std::string name = "Action Area";
    std::string comment;
    DirectX::XMFLOAT2 position{ 0.0f, 0.0f };
    DirectX::XMFLOAT2 size{ 500.0f, 300.0f };
    DirectX::XMFLOAT4 color{ 0.2f, 0.35f, 0.55f, 0.18f };
    std::vector<uintptr_t> nodeIds;
};

// 実行中ノードの経過時間や進行位置を保持する
struct AIBehaviorRuntimeState
{
    // WAITノードで蓄積する経過時間。
    float elapsedTime = 0.0f;
    // Sequenceが次に評価する子ノードのIndex。
    size_t childIndex = 0;
    // PLAY_ACTIONがすでに開始処理を呼び出したかを示す。
    bool actionStarted = false;
};

// AIグラフの実行時に参照する対象やBlackboard情報をまとめる
struct AIBehaviorContext
{
    // AIControllerComponentを保持し、Actionを実行するGameObject。
    GameObject* owner = nullptr;
    // 現在AIが追跡・攻撃対象としているGameObject。
    GameObject* target = nullptr;
    // Ownerが保持するLocal Blackboard。
    BlackboardComponent* blackboard = nullptr;
    // GameMode全体で共有されるGlobal Blackboard。
    BlackboardComponent* globalBlackboard = nullptr;
    DirectX::XMFLOAT3 ownerPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 targetPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 spawnPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 toTargetDirection = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 toSpawnDirection = { 0.0f, 0.0f, 0.0f };
    // OwnerからTargetまでの距離。
    float targetDistance = 0.0f;
    // OwnerからSpawn位置までの距離。
    float spawnDistance = 0.0f;
    // WAITなどの時間更新に使用するフレーム経過時間。
    float deltaTime = 0.0f;
    // targetが有効なObjectを参照しているかを示す。
    bool hasTarget = false;
    // 未設定なら従来の抽選を維持し、設定時だけ行動別の実行時補正を使用する。
    std::function<float(const std::string&)> actionWeightModifier;
    // 単独攻撃の枝で使用した基本重みと補正をデバッグ表示へ通知する。
    std::function<void(const std::string&, float, float)> actionWeightObserver;
    // 実際に通過したノードと結果だけを追跡へ通知し、再評価による副作用を避ける。
    std::function<void(const AIBehaviorNode&, AI_BEHAVIOR_STATUS)> nodeEvaluationObserver;
};

// BlackboardとActionを使用して敵AIの判断グラフを実行する
// Rootからノードを評価し、Blackboard条件とActionRegistry呼び出しをつなぐ実行本体。
class AIBehaviorGraph
{
public:
    void Clear();
    void EnsureDefaultGraph();
    AI_BEHAVIOR_STATUS Tick(AIBehaviorContext& context_);
    // 条件を評価するだけで、ActionやBlackboardへの書き込みは実行しない。
    std::vector<std::string> GetSelectableActions(AIBehaviorContext& context_) const;
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

    // 既存のPriorityを維持して行動の枝を領域ごとに配置する。
    void OrganizeActionAreas();
    void RestoreEditorLayout(const nlohmann::json& json_);
    // 登録済みノードが収まる大きさへ領域を合わせる。
    void FitArea(AIBehaviorArea& area_);
    std::vector<AIBehaviorArea>& GetAreas() { return areas; }
    const std::vector<AIBehaviorArea>& GetAreas() const { return areas; }
    bool& GetPriorityFromPositions() { return priorityFromPositions; }

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
    // 重複所属のない既存Boxを再配置し、共有行動の接続と説明を維持する。
    void ArrangeExistingAreas();
    // 条件を満たす枝に含まれるActionを収集し、循環リンクは評価しない。
    bool CollectSelectableActions(const AIBehaviorNode& node_, AIBehaviorContext& context_,
        std::vector<std::string>& actions_, std::vector<uintptr_t>& path_) const;
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
    // Graph Assetを構成する全ノードの保存データ。
    std::vector<AIBehaviorNode> nodes;
    // 親ノードから子ノードへの接続情報。
    std::vector<AIBehaviorLink> links;
    // 領域はEditor専用で、Runtimeのリンクや条件としては評価しない。
    std::vector<AIBehaviorArea> areas;
    bool priorityFromPositions = true;
    // ノードIDごとの待機時間・Sequence進行位置・Action開始状態。
    std::unordered_map<uintptr_t, AIBehaviorRuntimeState> runtimeStates;
    // 直前のTickで評価されたノードID。Editorの実行中表示に使用する。
    std::vector<uintptr_t> runtimeActiveNodeIds;
    // 直前にRootから評価したGraph全体の結果。
    AI_BEHAVIOR_STATUS lastTickStatus = AI_BEHAVIOR_STATUS::FAILURE;
    // 次に生成するノード・Pin・Linkへ割り当てる一意ID。
    uintptr_t nextId = 1;
};
