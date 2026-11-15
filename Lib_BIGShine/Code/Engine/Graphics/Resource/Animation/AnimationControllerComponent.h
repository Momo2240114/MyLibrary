#pragma once
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "ModelAnimator.h"
#include "AnimSequenceAdapter.h"
#include "AnimationState.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"


#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>

// 前方宣言
namespace ax { namespace NodeEditor { struct EditorContext; } }


// アニメーション再生、遷移グラフ、SequencerイベントをまとめるComponent。
// PlayerStateやAIが決めた行動を見た目のStateへ変換し、必要なActionRegistry呼び出しもここから流す。
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
    bool IsSequencerWindowVisible() const { return showSequencerWindow; }
    // エディタ用表示関数
    void PrepareGraphEditor();
    void OnGuiNodeEditor();
    void DrawNodeEditorContent(ANIM_GRAPH_CONNECTION_MODE connectionMode_ = ANIM_GRAPH_CONNECTION_MODE::LEGACY_TWO_PIN);
    void DrawTransitionInspectorContent();

    // エディタ側からポーズライブラリを直接操作・保存できるように、ゲッターを用意すると便利です
    std::shared_ptr<PoseLibraryResource> GetPoseLibrary() { return poseLibrary; }
    // 特定のステートへ遷移させる（直接呼び出し用）
    void ChangeState(const std::string& stateName_, float duration_ = 0.2f, bool restart_ = false, float startTime_ = 0.0f);
    // 演出再生中は通常グラフの自動遷移だけを抑制し、Animationとイベントは更新する。
    void SetPresentationControlled(bool controlled_) { presentationControlled = controlled_; }
    // SkillStateがPhaseを選択中も、Animation EventとRootMotionは通常どおり更新する。
    void SetSkillControlled(bool controlled_) { skillControlled = controlled_; }
    bool IsSkillControlled() const { return skillControlled; }

    // セーブ・ロードの実装
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // ステートを手動で追加する
    void AddState(const std::string& stateName_, int animationIndex_);
    // 編集済みStateを維持し、未登録のStateだけを既存Asset読み込み処理で追加する。
    bool LoadMissingStateAsset(const std::string& stateName_, const std::string& assetName_);
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

    // Blackboard へのアクセス権を渡す
    const BlackboardCategory& GetBlackboard() const;
    const AnimState* GetCurrentState() { return currentState; }
    float GetAnimatorTime() const { return animator.GetCurrentFadeTime(); }
    float GetCurrentSampleDuration() const { return animator.GetCurrentSampleDuration(); }
    void SetAnimatorTime(float time_) { animator.SetCurrentTime(time_); }
    void SetAnimationPaused(bool paused_) { animationPaused = paused_; }
    bool IsAnimationPaused() const { return animationPaused; }
    void SetEditorPoseOverride(bool enabled_) { editorPoseOverride = enabled_; }
    bool IsEditorPoseOverride() const { return editorPoseOverride; }
    // AIなど外部制御から、RootMotionのYaw回転が指定角度を超えないように制限する。
    void SetRootMotionYawLimit(float yawDegrees_);
    void ClearRootMotionYawLimit();
    // 単一クリップのRootMotionを区間サンプリングし、AIの到達判定へ予定量を返す。
    bool EstimateRootMotion(const std::string& stateName_, float startNormalizedTime_,
        float endNormalizedTime_, DirectX::XMFLOAT3& localDelta_, float& playbackDuration_) const;

    // ーーー 窓にデータを差し出すためのゲッター群 ーーー
    std::vector<AnimGraphNode>& GetGraphNodes() { return graphNodes; }
    std::vector<AnimTransition>& GetTransitions() { return graphTransitions; }
    std::unordered_map<std::string, std::unique_ptr<AnimState>>& GetStates() { return states; }

    uintptr_t GetNextId() { return nextGraphElementId++; }
    void SetIdCounterFloor(uintptr_t minValue_);
    // 内部処理用
    void EvaluateGraphTransitions();
    bool EvaluateCondition(const AnimCondition& condition_);

    // Blackboardから届いた要求を、グラフ条件と制御権を確認して処理する。
    void ProcessBlackboardGraphRequests(bool transitions_ = true);
    // AIへ公開する現在State・再生時間・制御権をBlackboardへ反映する。
    void PublishGraphFeedback();
    void SetUseTransitionGraph(bool useTransitionGraph_) { useTransitionGraph = useTransitionGraph_; }
private:
    bool processingBlackboardGraphRequests = false; // Actionからの再入時は次の処理へ要求を残す。
    std::string graphAssetFilePath;
    std::string graphAssetStatus;

    void SaveGraphAsset(const std::string& assetName_);
    bool LoadGraphAsset(const std::string& assetName_);
    bool SaveStateAsset(const std::string& stateName_, const std::string& assetPath_ = "");
    bool LoadStateAsset(const std::string& assetName_);
    std::string BuildDefaultStateAssetPath(const std::string& stateName_) const;
    void ApplyPendingStateAssets();
    void LoadPendingGraphAsset();
    void RegisterStateAction(const std::string& stateName_);
    void EnsureGraphNodeForState(const std::string& stateName_);
    void EnsureNodeEditorContext();
    void EnsureStatesFromModel();


    ModelAnimator animator;
    CurveComponent* curveComponent = nullptr;
    GltfModelCommponennt* modelComponent = nullptr;

    bool showSequencerWindow = false; // ウィンドウの表示状態を管理
    bool showNodeEditor = false; // ウィンドウの表示状態を管理

    // このコンポーネントが紐づくモデル固有のポーズの記憶
    std::shared_ptr<PoseLibraryResource> poseLibrary = nullptr;
    bool editorPoseOverride = false;
    bool animationPaused = false;

    // ノードエディタで管理されるステート群
    std::unordered_map<std::string, std::unique_ptr<AnimState>> states;
    bool skillControlled = false; // SkillのPhase中は通常グラフによる上書きを防ぐ。
    bool presentationControlled = false; // 演出Actionが再生Stateを選択している間だけ有効。
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

    struct StateGuiEntry
    {
        std::string name;
        AnimState* state = nullptr;
    };

    struct CachedActionCategory
    {
        std::string category;
        std::string menuLabel;
        std::vector<std::string> actionNames;
    };

    // Sequencer編集データをRuntime再生へ渡すためのAdapter。
    // 攻撃、Particle、MeshVFXなどの時間イベントはここを経由してActionRegistryへつながる。
    AnimSequenceAdapter sequenceAdapter;
    AnimState* sequenceAdapterState = nullptr;
    bool sequenceAdapterDirty = true;
    int sequencerSelectedEntry = -1;
    int sequencerFirstFrame = 0;
    bool sequencerExpanded = true;

    std::vector<CachedActionCategory> cachedActionCategories;
    GameObject* cachedActionOwner = nullptr;
    bool actionCategoryCacheDirty = true;

    std::vector<StateGuiEntry> cachedStateGuiEntries;
    bool stateGuiCacheDirty = true;

    std::vector<std::string> cachedBlackboardKeys;
    bool blackboardGuiCacheDirty = true;

    std::vector<int> cachedNodeDepths;
    std::vector<bool> cachedMaskInheritedFlags;
    std::unordered_set<std::string> cachedMaskRootNodeSet;
    AnimState* animationMaskCacheState = nullptr;
    bool animationMaskCacheDirty = true;

    std::vector<std::string> cachedParticleAssetKeys;
    std::vector<std::string> cachedMeshEffectAssetKeys;
    GameObject* cachedEffectAssetOwner = nullptr;
    bool effectAssetCacheDirty = true;

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
    ed::LinkId   selectedTransitionLinkId;
    std::string  currentGraphScopeId = "Root";
    std::vector<std::string> graphScopeStack;

    void EnsureSpecialGraphNodes();
    void TryEnterStartState();
    bool TryEnterSubGraph(const AnimGraphNode& subGraphNode_, float duration_);
    void LoadAsset(const std::string& assetName_);
    void SaveAsset(const std::string& assetName_);

    BlackboardCategory& GetEditableBlackboard();
    const BlackboardCategory& GetCurrentBlackboard() const;
    void RefreshSequenceAdapter();
    void MarkSequenceAdapterDirty();
    void RefreshActionCategoryCache();
    void MarkActionCategoryCacheDirty();
    void RefreshStateGuiCache();
    void MarkStateGuiCacheDirty();
    void RefreshBlackboardGuiCache();
    void MarkBlackboardGuiCacheDirty();
    void RefreshAnimationMaskCache();
    void MarkAnimationMaskCacheDirty();
    void RefreshEffectAssetCache();
    void MarkEffectAssetCacheDirty();
    void PreviewStateAtTime(AnimState* state_, float time_);
    float ConsumeRootMotionYawLimit(float yawDegrees_);

    bool pendingGraphAssetLoad = false;
    std::string pendingGraphAssetPath;

    bool useTransitionGraph = true;

    DirectX::XMFLOAT3 rootMotionDelta;
    float rootMotionYawLimitRemaining = 0.0f;
    bool rootMotionYawLimitActive = false;

    // ActionRegistryへの登録名リスト（削除用）
    std::vector<std::string> registeredActionNames;
    uintptr_t nextGraphElementId = 1; // 0は無効値(Invalid)として扱われることが多いため1から開始
public:
    // 外部からのパラメータ入力
    void SetParam(const std::string& name_, BlackBoardValue value_);
    void SetBlackboardCategory(const std::string& category_) { animationBlackboardCategory = category_; }
    const std::string& GetBlackboardCategory() const { return animationBlackboardCategory; }
};
