#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ModelAnimator.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"


#include <unordered_map>
#include <memory>
#include <vector>

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
    void SetSequencerWindowVisible(bool visible) { showSequencerWindow = visible; }
    // エディタ用表示関数
    void OnGuiNodeEditor();
    void DrawNodeEditorContent();

    // エディタ側からポーズライブラリを直接操作・保存できるように、ゲッターを用意すると便利です
    std::shared_ptr<PoseLibraryResource> GetPoseLibrary() { return m_poseLibrary; }
    // 特定のステートへ遷移させる（直接呼び出し用）
    void ChangeState(const std::string& stateName, float duration = 0.2f);

    // セーブ・ロードの実装
    void Serialize(nlohmann::json& j) const override;
    void Deserialize(const nlohmann::json& j) override;

    // ステートを手動で追加する
    void AddState(const std::string& stateName, int animIndex);
    bool AddOrUpdateCustomState(int customAnimIndex_);
    bool PreviewCustomAnimation(int customAnimIndex_, float time_);
    bool SelectStateForEditing(const std::string& stateName_);

    // ステートを削除する
    void RemoveState(const std::string& stateName);
    void SetTrigger(const std::string& name_);
    void ResetTrigger(const std::string& name_);
    AnimGraphNode* AFindNodeByPin(ed::PinId pinId);
    // 宣言
    AnimGraphNode* FindNodeByName(const std::string& name);
    //const AnimGraphNode* FindNodeByNameCons(const std::string& name) const;

    // Blackboard へのアクセス権を授ける
    const BlackboardCategory& GetBlackboard() const;
    const AnimState* GetCurrentState() { return currentState; };
    float GetAnimatorTime() const { return animator.GetCurrentFadeTime(); }
    void SetEditorPoseOverride(bool enabled) { m_editorPoseOverride = enabled; }
    bool IsEditorPoseOverride() const { return m_editorPoseOverride; }

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
    std::string m_graphAssetFilePath;

    void SaveGraphAsset(const std::string& assetName);
    void LoadGraphAsset(const std::string& assetName);
    bool SaveStateAsset(const std::string& stateName_);
    bool LoadStateAsset(const std::string& assetName_);
    void RegisterStateAction(const std::string& stateName_);
    void EnsureGraphNodeForState(const std::string& stateName_);
    void EnsureNodeEditorContext();
    void EnsureStatesFromModel();

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
    bool m_editorPoseOverride = false;

    // ノードエディタで管理されるステート群
    std::unordered_map<std::string, std::unique_ptr<AnimState>> states;
    const AnimState* currentState = nullptr;
    AnimState* editingState = nullptr;// 編集中のステートを指す

    std::string m_assetFilePath; // 保存するパス

    BlackboardComponent* blackboardComponent = nullptr;
    std::string animationBlackboardCategory = "Anim";
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
    std::string  m_currentGraphScopeId = "Root";
    std::vector<std::string> m_graphScopeStack;

    void EnsureSpecialGraphNodes();
    void TryEnterStartState();
    bool TryEnterSubGraph(const AnimGraphNode& subGraphNode, float duration);
    void LoadAsset(const std::string& path);
    void SaveAsset(const std::string& path);

    BlackboardCategory& GetEditableBlackboard();
    const BlackboardCategory& GetCurrentBlackboard() const;

    bool m_pendingGraphAssetLoad = false;
    std::string m_pendingGraphAssetPath;

    // ActionRegistryへの登録名リスト（削除用）
    std::vector<std::string> registeredFullNames;
    uintptr_t m_NextID = 1; // 0は無効値(Invalid)として扱われることが多いため1から開始
public:
    // 外部からのパラメータ入力
    void SetParam(const std::string& name_, BlackBoardValue value_);
    void SetBlackboardCategory(const std::string& category_) { animationBlackboardCategory = category_; }
    const std::string& GetBlackboardCategory() const { return animationBlackboardCategory; }
};

