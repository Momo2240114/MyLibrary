#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ModelAnimator.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"


#include <unordered_map>
#include <variant>
#include <memory>

// Blackboardが持てる型の定義
using BlackBoardValue = std::variant<float, int, bool, DirectX::SimpleMath::Vector2>;
// 前方宣言
namespace ax { namespace NodeEditor { struct EditorContext; } }


class AnimationControllerComponent : public Component
{
public:
    const std::string& GetName() const override { static std::string name = "AnimationControllerComponent"; return name; }
    ~AnimationControllerComponent();
    void Start() override;
    void Update(float deltaTime) override;

    void Gui() override;
    void ShowSequencerEditor();
    // エディタ用表示関数
    void OnGuiNodeEditor();

    // エディタ側からポーズライブラリを直接操作・保存できるように、ゲッターを用意すると便利です
    std::shared_ptr<PoseLibraryResource> GetPoseLibrary() { return m_poseLibrary; }
    // 特定のステートへ遷移させる（直接呼び出し用）
    void ChangeState(const std::string& stateName, float duration = 0.2f);

    // セーブ・ロードの実装
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // ステートを手動で追加する
    void AddState(const std::string& stateName, int animIndex);

    // ステートを削除する
    void RemoveState(const std::string& stateName);

    // 外部からトリガーを引く
    void SetTrigger(const std::string& name)
    {
        blackboard[name] = true;
    }
    // 内部でトリガーをリセットする
    void ResetTrigger(const std::string& name)
    {
        if (blackboard.count(name)) {
            blackboard[name] = false;
        }
    }
    AnimGraphNode* AFindNodeByPin(ed::PinId pinId);
    // 宣言
    AnimGraphNode* FindNodeByName(const std::string& name);

    // Blackboard へのアクセス権を授ける
    const std::unordered_map<std::string, BlackBoardValue>& GetBlackboard() const { return blackboard; }
    const AnimState* GetCurrentState() { return currentState; };

    // ーーー 窓にデータを差し出すためのゲッター群 ーーー
    std::vector<AnimGraphNode>& GetGraphNodes() { return graphNodes; }
    std::vector<AnimTransition>& GetTransitions() { return graphTransitions; }
    std::unordered_map<std::string, std::unique_ptr<AnimState>>& GetStates() { return states; }

    uintptr_t GetNextID() { return m_NextID++; }
    void SetIDCounterFloor(uintptr_t minVal);
    // 内部処理用
    void EvaluateGraphTransitions();
    bool EvaluateCondition(const AnimCondition& cond);

private:
    int m_nextId = 1;
    // 【新設】何が選ばれているかを記録する賢者の記録
    struct Selection {
        ed::NodeId nodeId;
        ed::LinkId linkId;
    } m_selection;

    // 一意なIDを発行する秘術
    int GetNextId() { return m_nextId++; }

    ModelAnimator animator;
    CurveComponent* curvecomp;
    GltfModelCommponennt* modelComp = nullptr;

    bool showSequencerWindow = false; // ウィンドウの表示状態を管理
    bool m_ShowNodeEditor = false; // ウィンドウの表示状態を管理

    // 【追加】このコンポーネントが紐づくモデル固有のポーズの記憶
    std::shared_ptr<PoseLibraryResource> m_poseLibrary = nullptr;

    // ノードエディタで管理されるステート群
    std::unordered_map<std::string, std::unique_ptr<AnimState>> states;
    const AnimState* currentState = nullptr;
    AnimState* editingState = nullptr;// 編集中のステートを指す

    std::string m_assetFilePath; // 保存するパス

    // 外部から値を書き換え、グラフがそれを監視する
    std::unordered_map<std::string, BlackBoardValue> blackboard;
    // グラフデータ
    std::vector<AnimGraphNode> graphNodes;
    std::vector<AnimTransition> graphTransitions;
    // エディタのインスタンス状態を保持するコンテキスト
    ed::EditorContext* nodeEditorContext = nullptr;
    // エディタの初期設定用
    ed::Config nodeEditorConfig;
    std::string  _runtimeActiveStateName;
    ImVec2       _newNodeSpawnPos = { 0,0 };
    uintptr_t    _nodeToPlaceHash = 0;
    bool         m_needsPositionRestore = false;

    void LoadAsset(const std::string& path);
    void SaveAsset(const std::string& path);


    // ActionRegistryへの登録名リスト（削除用）
    std::vector<std::string> registeredFullNames;
    uintptr_t m_NextID = 1; // 0は無効値(Invalid)として扱われることが多いため1から開始
public:
    // 外部からのパラメータ入力
    void SetParam(const std::string& name, BlackBoardValue value) { blackboard[name] = value; }
};