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
    const std::string& GetName() const override { static std::string name_ = "AnimationControllerComponent"; return name_; }
    ~AnimationControllerComponent();
    void Start() override;
    void Update(float deltaTime_) override;

    void Gui() override;
    void ShowSequencerEditor();
    void SetSequencerWindowVisible(bool visible_) { showSequencerWindow = visible_; }
    // エディタ用表示関数
    void OnGuiNodeEditor();
    void DrawNodeEditorContent();

    // エディタ側からポーズライブラリを直接操作・保存できるように、ゲッターを用意すると便利です
    std::shared_ptr<PoseLibraryResource> GetPoseLibrary() { return poseLibrary; }
    // 特定のステートへ遷移させる（直接呼び出し用）
    void ChangeState(const std::string& stateName_, float duration_ = 0.2f);

    // セーブ・ロードの実装
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // ステートを手動で追加する
    void AddState(const std::string& stateName_, int animationIndex_);
    bool AddOrUpdateCustomState(int customAnimIndex_);
    bool PreviewCustomAnimation(int customAnimIndex_, float time_);
    bool SelectStateForEditing(const std::string& stateName_);
    bool RenameState(const std::string& oldName_, const std::string& newName_);

    // ステートを削除する
    void RemoveState(const std::string& stateName_);
    void SetTrigger(const std::string& name_);
    void ResetTrigger(const std::string& name_);
    AnimGraphNode* FindNodeByPin(ed::PinId pinId_);
    // 宣言
    AnimGraphNode* FindNodeByName(const std::string& name_);

    // Blackboard へのアクセス権を授ける
    const BlackboardCategory& GetBlackboard() const;
    const AnimState* GetCurrentState() { return currentState; }
    float GetAnimatorTime() const { return animator.GetCurrentFadeTime(); }
    void SetAnimatorTime(float time_) { animator.SetCurrentTime(time_); }
    void SetAnimationPaused(bool paused_) { animationPaused = paused_; }
    bool IsAnimationPaused() const { return animationPaused; }
    void SetEditorPoseOverride(bool enabled_) { editorPoseOverride = enabled_; }
    bool IsEditorPoseOverride() const { return editorPoseOverride; }

    // ーーー 窓にデータを差し出すためのゲッター群 ーーー
    std::vector<AnimGraphNode>& GetGraphNodes() { return graphNodes; }
    std::vector<AnimTransition>& GetTransitions() { return graphTransitions; }
    std::unordered_map<std::string, std::unique_ptr<AnimState>>& GetStates() { return states; }

    uintptr_t GetNextId() { return nextGraphElementId++; }
    void SetIdCounterFloor(uintptr_t minValue_);
    // 内部処理用
    void EvaluateGraphTransitions();
    bool EvaluateCondition(const AnimCondition& condition_);

    void SetUseTransitionGraph(bool useTransitionGraph_) { useTransitionGraph = useTransitionGraph_; }
private:
    std::string graphAssetFilePath;
    std::string graphAssetStatus;

    void SaveGraphAsset(const std::string& assetName_);
    bool LoadGraphAsset(const std::string& assetName_);
    bool SaveStateAsset(const std::string& stateName_, const std::string& assetPath_ = "");
    bool LoadStateAsset(const std::string& assetName_);
    std::string BuildDefaultStateAssetPath(const std::string& stateName_) const;
    void ApplyPendingStateAssets();
    void RegisterStateAction(const std::string& stateName_);
    void EnsureGraphNodeForState(const std::string& stateName_);
    void EnsureNodeEditorContext();
    void EnsureStatesFromModel();


    ModelAnimator animator;
    CurveComponent* curveComponent = nullptr;
    GltfModelCommponennt* modelComponent = nullptr;

    bool showSequencerWindow = false; // ウィンドウの表示状態を管理
    bool showNodeEditor = false; // ウィンドウの表示状態を管理

    // 【追加】このコンポーネントが紐づくモデル固有のポーズの記憶
    std::shared_ptr<PoseLibraryResource> poseLibrary = nullptr;
    bool editorPoseOverride = false;
    bool animationPaused = false;

    // ノードエディタで管理されるステート群
    std::unordered_map<std::string, std::unique_ptr<AnimState>> states;
    std::unordered_map<std::string, std::string> stateAssetPaths;
    std::unordered_map<std::string, std::string> pendingStateAssetPaths;
    const AnimState* currentState = nullptr;
    AnimState* editingState = nullptr;// 編集中のステートを指す
    std::string stateSearchText;
    std::string animationClipSearchText;
    std::string graphStateSearchText;
    std::string stateAssetImportPath;
    std::string stateAssetStatus;
    std::string stateRenameText;
    std::string stateRenameSourceName;

    std::string assetFilePath; // 保存するパス

    BlackboardComponent* blackboardComponent = nullptr;
    std::string animationBlackboardCategory = "Anim";
    // グラフデータ
    std::vector<AnimGraphNode> graphNodes;
    std::vector<AnimTransition> graphTransitions;
    // エディタのインスタンス状態を保持するコンテキスト
    ed::EditorContext* nodeEditorContext = nullptr;
    // エディタの初期設定用
    ed::Config nodeEditorConfig;
    std::string  runtimeActiveStateName;
    ImVec2       newNodeSpawnPosition = { 0,0 };
    uintptr_t    nodeToPlaceId = 0;
    bool         needsPositionRestore = false;
    std::string  currentGraphScopeId = "Root";
    std::vector<std::string> graphScopeStack;

    void EnsureSpecialGraphNodes();
    void TryEnterStartState();
    bool TryEnterSubGraph(const AnimGraphNode& subGraphNode_, float duration_);
    void LoadAsset(const std::string& assetName_);
    void SaveAsset(const std::string& assetName_);

    BlackboardCategory& GetEditableBlackboard();
    const BlackboardCategory& GetCurrentBlackboard() const;

    bool pendingGraphAssetLoad = false;
    std::string pendingGraphAssetPath;

    bool useTransitionGraph = true;

    DirectX::XMFLOAT3 rootMotionDelta;

    // ActionRegistryへの登録名リスト（削除用）
    std::vector<std::string> registeredActionNames;
    uintptr_t nextGraphElementId = 1; // 0は無効値(Invalid)として扱われることが多いため1から開始
public:
    // 外部からのパラメータ入力
    void SetParam(const std::string& name_, BlackBoardValue value_);
    void SetBlackboardCategory(const std::string& category_) { animationBlackboardCategory = category_; }
    const std::string& GetBlackboardCategory() const { return animationBlackboardCategory; }
};

