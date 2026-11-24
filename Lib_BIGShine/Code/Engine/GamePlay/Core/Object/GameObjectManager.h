#pragma once
#include "Engine/GamePlay/Scene/SceneLoadRequest.h"

#include "GameObject.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/MotionVelocityRenderSystem.h"
#include "Engine/GamePlay/Scene/IScene.h"
#include "Engine/Physics/CollisionStepSystem.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/System/JustDodgeSystem.h"
#include "Engine/GamePlay/System/JustGuardSystem.h"
#include "Engine/GamePlay/System/GuardSystem.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Game/GameMode/GameModeManager.h"
#include "Engine/Common/StringId.h"

#include <unordered_map>
#include <unordered_set>

class ColliderComponent;
class BoxCollider;
class GltfModelCommponennt;
class TimeManager;
class BossFinishDirector;
class PlayerDeathDirector;
class CameraManager;
class CameraComponent;
class MotionVelocityComponent;
class CombatObservationSystem;
struct ID3D11RenderTargetView;

enum class SCENE_RUNTIME_STATE
{
    // Scene Load中。必要Assetや必須Componentの準備完了を待つ。
    LOADING,
    // Load完了後、TransformやRuntime状態を最終確定する。
    INITIALIZING,
    // 通常更新中。
    RUNNING,
    // 必須Asset失敗などでScene実行を止めた状態。
    FAILED
};

// Sceneのロード状態とは独立して、編集とテスト実行を管理する。
enum class EditorPlayMode { EDITOR, PLAY, PAUSED };
// GUIの操作を更新開始時に反映するための要求種別。
enum class EditorPlayCommand { NONE, PLAY, PAUSE, STOP };

#include"Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include"Engine/Graphics/VFX/MeshVFXRenderSystem.h"
#include"Engine/GamePlay/Core/Resource/ResourceManager.h"


/// GameObjectの生成、更新、描画登録、遅延削除を統括する管理クラス。
/// フレーム更新中のコンテナ変更を避けるため、初期化と削除は待機リストを介して処理する。
class GameObjectManager
{
public:
    void Initialize(bool deferEffectPreload_ = false);

    // 表示Componentへ渡す状態は値で保持し、ロード元Sceneの寿命に依存させない。
    void SetPresentationLoadingStatus(const LoadingStatus& status_)
    {
        presentationLoadingStatus = status_;
    }

    LoadingStatus GetPresentationLoadingStatus() const
    {
        return presentationLoadingStatus;
    }

    void SetEngineLoadingPresentation(bool engine_)
    {
        engineLoadingPresentation = engine_;
    }

    bool IsEngineLoadingPresentation() const
    {
        return engineLoadingPresentation;
    }

    void SetPresentationRetryEnabled(bool enabled_)
    {
        presentationRetryEnabled = enabled_;
    }

    bool IsPresentationRetryEnabled() const
    {
        return presentationRetryEnabled;
    }

    void RequestPresentationRetry()
    {
        presentationRetryRequested = true;
    }

    bool TakePresentationRetryRequest()
    {
        const bool requested_ = presentationRetryRequested;
        presentationRetryRequested = false;
        return requested_;
    }

    void SetPresentationWarning(const std::string& warning_)
    {
        presentationWarning = warning_;
    }

    const std::string& GetPresentationWarning() const
    {
        return presentationWarning;
    }

    // 通常描画が失敗した場合も、既に準備したUIだけを提出できる。
    void RenderUI();

    void SetPresentationSilhouetteOverride(bool enabled_,bool white_)
    {
        presentationSilhouetteOverride = enabled_;
        presentationSilhouetteWhite = white_;
    }

    bool HasPresentationSilhouetteOverride() const
    {
        return presentationSilhouetteOverride;
    }

    bool IsPresentationSilhouetteWhite() const
    {
        return presentationSilhouetteWhite;
    }

    // 独立した監視サービスを公開する。観測値・履歴・戦闘結果の処理はSystemが担当する。
    std::shared_ptr<CombatObservationSystem> GetCombatObservationSystem() const;

    template<typename... Args>
    std::shared_ptr<GameObject> Create(const std::string& name_, Args&&... args_)
    {
        const std::string& baseName_ = name_;

        // 同名Objectがある場合は末尾番号を付け、Scene内で名前が衝突しないようにする。
        int count_ = 0;
        for (auto& o : objects)
        {
            const std::string& n_ = o->nameComponent->name;
            if (n_ == baseName_ || n_.rfind(baseName_ + "_", 0) == 0)
            {
                count_++;
            }
        }

        // 最終的に使うObject名を決定する。
        std::string finalName_ = baseName_;
        if (count_ > 0)
        {
            char buf_[32];
            sprintf_s(buf_, sizeof(buf_), "_%02d", count_); 
            finalName_ += buf_;
        }

        // Objectを生成し、ManagerとTransformの所有Object参照を設定する。
        auto newObject_ = std::make_shared<GameObject>();
        newObject_->nameComponent->name = finalName_;
        newObject_->transform->gameObject = newObject_;

        newObject_->SetManager(this);
        objects.emplace_back(newObject_);       // 全体リストに追加
        RegisterObjectNameIndex(newObject_);
        // StartはUpdate中の追加にも対応できるよう、即時ではなく待機リストで処理する。
        pendingStarts.push_back(newObject_);
        return newObject_;
    }


    // 名前でObjectを検索する。
    std::shared_ptr<GameObject> Find(const std::string& name_);

    template <typename T>
    std::vector<std::shared_ptr<T>> FindComponents()
    {
        std::vector<std::shared_ptr<T>> results_;

        // 全Objectから指定型Componentを1つずつ集める。
        for (auto& object_ : objects)
        {
            if (!object_)
            {
                continue;
            }

            // 各オブジェクトのコンポーネントリストを走査
            // ※GameObject側にコンポーネントを取得する GetComponent<T> がある前提です
            auto component_ = object_->GetComponent<T>();
            if (component_)
            {
                results_.push_back(component_);
            }
        }
        return results_;
    }

    void RenderAll(
        SpriteRenderSystem& spriteSystem,
        ModelRenderSystem& modelSystem,
        ID3D11RenderTargetView* finalRenderTargetView_ = nullptr,
        const CameraComponent* overrideCamera_ = nullptr,
        bool clearSceneRenderTargets_ = false);

    void SetModelRenderSystem(ModelRenderSystem* modelRenderSystem_)
    {
        modelRenderSystem = modelRenderSystem_;
    }

    void RegisterMotionVelocityComponent(const std::shared_ptr<MotionVelocityComponent>& component_);
    void UnregisterMotionVelocityComponent(MotionVelocityComponent* component_);

    void Update(float elapsedTime_);
    // followActiveSession_がfalseなら、独立WorkspaceをEditorモードに固定する。
    void EnableEditorPlayMode(bool followActiveSession_ = true);

    bool SupportsEditorPlayMode() const
    {
        return editorPlayModeEnabled;
    }

    EditorPlayMode GetEditorPlayMode() const
    {
        return editorPlayModeEnabled ? editorPlayMode : EditorPlayMode::PLAY;
    }

    bool IsResultPreviewScene() const
    {
        return editorPlayModeEnabled && loadedSceneName == "ResultPreview";
    }

    bool CanStartEditorPlay() const
    {
        return editorPlayModeEnabled && editorPlayMode == EditorPlayMode::EDITOR &&
            sceneRuntimeState == SCENE_RUNTIME_STATE::RUNNING && !IsResultPreviewScene();
    }

    bool ShouldSimulatePhysics() const;

    // Sceneのメニュー停止を物理更新と音声管理にも共有する。
    void SetGameMenuPaused(bool paused_)
    {
        gameMenuPaused = paused_;
    }

    bool IsGameMenuPaused() const
    {
        return gameMenuPaused;
    }

    // ActionSequenceの編集要求をWorkspaceへ渡す。Object走査中は切り替えない。
    void RequestCinematicEditor(const std::string& ownerName_, int sequenceIndex_);
    bool ConsumeCinematicEditorRequest(std::string& ownerName_, int& sequenceIndex_);
    // 編集Sceneをメモリへ退避し、同じActionSequenceを本番の更新経路で試写する。
    bool BeginCinematicSequencePreview(const std::string& ownerName_, int sequenceIndex_, const nlohmann::json& sequenceData_, GameProgressState context_, bool prepareFinish_ = false);
    void StopCinematicSequencePreview();
    void SeekCinematicSequencePreview(float time_);

    bool IsCinematicSequencePreview() const
    {
        return cinematicPreviewActive;
    }

    bool HasCinematicSequenceStopRequest() const
    {
        return cinematicPreviewActive && (cinematicStopRequested || pendingEditorPlayCommand == EditorPlayCommand::STOP);
    }

    bool IsCinematicSequenceSeeking() const
    {
        return cinematicReplayTarget >= 0.0f || cinematicRebuild || cinematicPreparing;
    }

    bool IsCinematicFinishReady() const;

    const std::string& GetCinematicPreviewError() const
    {
        return cinematicPreviewError;
    }

    bool IsCinematicSequencePaused() const
    {
        return cinematicPaused;
    }

    float GetCinematicSequenceTime() const
    {
        return cinematicTime;
    }

    void SetCinematicSequencePaused(bool paused_);
    void SetCinematicSequenceRate(float rate_);
    // 決着中は専用時計だけを進め、物理や通常の行動予約を更新しない。
    bool IsBossFinishActive() const;

    BossFinishDirector* GetBossFinishDirector()
    {
        return bossFinishDirector.get();
    }

    // 死亡演出中は敵の行動予約、当たり判定、物理を通常更新から切り離す。
    bool IsPlayerDeathActive() const;
    void RequestPlayerDeath(const std::shared_ptr<GameObject>& player_);

    PlayerDeathDirector* GetPlayerDeathDirector()
    {
        return playerDeathDirector.get();
    }

    void SetEditorWorkspacePaused(bool paused_)
    {
        editorWorkspacePaused = paused_;
    }

    // Object走査中の破棄を避けるため、GUIからは切替要求だけを記録する。
    void RequestEditorPlayCommand(EditorPlayCommand command_)
    {
        pendingEditorPlayCommand = command_;
    }

    bool IsEditorStopRequested() const
    {
        return pendingEditorPlayCommand == EditorPlayCommand::STOP;
    }

    bool IsEditorPauseRequested() const
    {
        return pendingEditorPlayCommand == EditorPlayCommand::PAUSE;
    }

    void SetPlayInputCaptured(bool captured_)
    {
        playInputCaptured = captured_;
    }

    bool IsPlayInputCaptured() const
    {
        return playInputCaptured && GetEditorPlayMode() == EditorPlayMode::PLAY && !editorWorkspacePaused;
    }

    // 未保存の配置と設定をメモリ上のスナップショットから復元する。
    void RestoreEditorSnapshot(const nlohmann::json& snapshot_);
    // Play中は開始前の編集内容、それ以外は現在の編集内容を配布用に取得する。
    nlohmann::json CaptureStandaloneScene();
    // SceneLoadingからだけ使う段階ロード。CPU読込と主スレッドの復元処理を分離する。
    void LoadSceneAsync(const std::string& sceneName_, bool beginGameSceneFlow_ = false);
    void AdvanceSceneLoading(float deltaTime_);
    // 戦闘・AI・物理を進めず、専用Sceneの見た目だけを更新する。
    void UpdateLoadingPresentation(float deltaTime_);

    const std::string& GetSceneLoadOperation() const
    {
        return sceneLoadOperation;
    }

    void SetLoadingPresentation(bool enabled_)
    {
        loadingPresentation = enabled_;
    }

    SCENE_RUNTIME_STATE GetSceneRuntimeState() const
    {
        return sceneRuntimeState;
    }

    void SetSceneRuntimeState(SCENE_RUNTIME_STATE state)
    {
        sceneRuntimeState = state;
    }

    // 編集用の入力欄ではなく、実際に読み込んだScene名を演出設定へ渡す。
    const std::string& GetLoadedSceneName() const
    {
        return loadedSceneName;
    }

    float GetSceneLoadProgress() const
    {
        return sceneLoadProgress;
    }

    const std::string& GetSceneRuntimeError() const
    {
        return sceneRuntimeError;
    }

    void Submit(ID3D11DeviceContext* context_)
    {
        // 全Objectの描画Submitをまとめて呼ぶ。
        for (auto& object_ : objects)
        {
            object_->Submit(context_);
        }
    }

    // 指定したObjectを削除予約する。
    void Destroy(std::shared_ptr<GameObject> object_);
    void Destroy(std::string objectName_);
    // 演出で実体を破棄しても、Sceneに配置した階層の保存元データを残す。
    void DestroyRuntimeObject(const std::shared_ptr<GameObject>& object_);

    // 全てのObjectを解放する。
    void Clear(bool rebuildEffectPools_ = true);

    // 選択中オブジェクト
    void SelectObject(std::shared_ptr<GameObject> object_)
    {
        selectedObject = object_;
    }

    std::shared_ptr<GameObject> GetSelectedObject()
    {
        return selectedObject;
    }

    void AddObject(std::shared_ptr<GameObject> object_);
    void DetachObject(const std::shared_ptr<GameObject>& object_);
    void RemoveObject(const std::string& name_);
    void NotifyObjectNameChanged(
        GameObject* object_,
        const std::string& oldName_,
        const std::string& newName_);

    // 状態を尋ねる窓口。
    const std::vector<std::shared_ptr<GameObject>>& GetObjects() const
    {
        return objects;
    }

    // GUIは選択中Objectだけ表示する。
    void Gui();
    void DrawHierarchyNode(std::shared_ptr<GameObject> object_, GameObject** objectToDelete_);

    std::shared_ptr<GameObject> GetSelectedObject() const
    {
        return selectedObject;
    }

    void SetSelectedObject(std::shared_ptr<GameObject> object_)
    {
        selectedObject = object_;
    }

    void ChangeScene(Scene* scene_)
    {
        currentScene = scene_;
    }

    void SaveScene(const std::string& sceneName_);
    void LoadScene(const std::string& sceneName_);

    void ResolveHierarchy();

    std::vector<std::string> GetSceneDirectories();

    // 指定したオブジェクトをプレハブとして保存
    void SaveAsPrefab(std::shared_ptr<GameObject> object_, const std::string& prefabName_);


    void InitializeAllComponents();

    std::vector<std::shared_ptr<GameObject>>& GetObjects()
    {
        return objects;
    }

    CameraManager* GetCameraManager();

    // プレハブからオブジェクトを生成
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabName_, bool preserveNames_ = false);
    // Prefabsフォルダ内のファイル一覧を更新
    void RefreshPrefabList();

    const std::vector<std::string>& GetPrefabFiles() const
    {
        return prefabFiles;
    }

    int GetSelectedPrefabIndex() const
    {
        return selectedPrefabIndex;
    }

    void SetSelectedPrefabIndex(int selectedPrefabIndex_);

    void ApplyDestroys(std::shared_ptr<GameObject> object_);

    void SetShapeRenderEnabled(bool enabled_)
    {
        shapeRenderEnabled = enabled_;
    }

    bool IsShapeRenderEnabled() const
    {
        return shapeRenderEnabled;
    }

    size_t SetAllCollisionShapeRenderEnabled(bool enabled_);
    size_t SetCollisionShapeRenderEnabledInHierarchy(
        const std::string& rootObjectName_,
        bool enabled_,
        bool includeChildren_ = true);
    size_t CountAllCollisionShapes();
    size_t CountCollisionShapesInHierarchy(
        const std::string& rootObjectName_,
        bool includeChildren_ = true);

    // 全Object/Componentが共有Resourceへアクセスするための窓口。
    ResourceManager* GetResourceManager()
    {
        return resourceManager.get();
    }

    void DrawHierarchyGui();   // ヒストリー(階層)GUI
    void DrawInspectorGui();   // インスペクターGUI
    void DrawSceneModals(); // モーダルを描画する専用の儀式

    void RefreshSceneList()
    {
        sceneState.list = GetSceneDirectories();
    }

    std::shared_ptr<TimeManager> GetTimeManager()
    {
        return timeManager;
    }

    GameModeManager* GetGameModeManager()
    {
        return gameModeManager.get();
    }

    const GameModeManager* GetGameModeManager() const
    {
        return gameModeManager.get();
    }

    CollisionStepSystem& GetCollisionStepSystem()
    {
        return *collisionStepSystem;
    }

    AttackSystem& GetAttackSystem()
    {
        return *attackSystem;
    }

    JustDodgeSystem& GetJustDodgeSystem()
    {
        return *justDodgeSystem;
    }

    JustGuardSystem& GetJustGuardSystem()
    {
        return *justGuardSystem;
    }

    GuardSystem& GetGuardSystem()
    {
        return *guardSystem;
    }

    EffectManager& GetEffectManager()
    {
        return *effectManager;
    }

private:
    // 再構築・ロード待ち・固定刻みの再評価をフレーム予算内で処理する。
    void UpdateCinematicSequencePreview(float deltaTime_);
    nlohmann::json cinematicOriginalScene;
    nlohmann::json cinematicRuntimeScene;
    std::string cinematicOwnerName;
    int cinematicSequenceIndex = -1;
    int cinematicLeadInIndex = -1;
    bool cinematicPrepareFinish = false;
    bool cinematicPreparing = false;
    float cinematicPreparationTime = 0.0f;
    std::string cinematicPreviewError;
    GameProgressState cinematicContext = GameProgressState::GAME_SCENE_START;
    bool cinematicPreviewActive = false;
    bool cinematicStopRequested = false;
    bool cinematicRebuild = false;
    bool cinematicReloading = false;
    bool cinematicStarted = false;
    bool cinematicPaused = true;
    float cinematicTime = 0.0f;
    float cinematicReplayTarget = -1.0f;
    float cinematicRate = 1.0f;
    float cinematicRemainder = 0.0f;
    std::string requestedCinematicOwner;
    int requestedCinematicSequence = -1;

    // 指定名を基準に、Scene内で重複しない名前を作る。
    std::string MakeUniqueName(std::string baseName_)
    {
        std::string name_ = baseName_;
        int index_ = 1;

        // 名前が既存のオブジェクトと重複している間、ずっとループする
        while (Find(name_))
        {
            char buffer_[32];
            sprintf_s(buffer_, sizeof(buffer_), "_%02d", index_);
            name_ = baseName_ + buffer_; // 例: Enemy -> Enemy_01 -> Enemy_02 ...
            index_++;
        }
        return name_;
    }


private:
    // フレーム境界でPlay/Pause/Stopを反映する。
    void ProcessEditorPlayCommand();
    // ゲームのStartやAIを起動せず、編集に必要な描画データを更新する。
    void UpdateEditorScene(float deltaTime_);
    // ディスクへ書き出さず、編集状態を退避する。
    nlohmann::json CaptureEditorSnapshot();
    // ディスク読込とスナップショットで同じ復元経路を使う。
    void LoadSceneData(const std::string& sceneName_, const nlohmann::json& sceneData_);
    bool PumpSceneLoad();
    std::unique_ptr<SceneLoadRequest> sceneReadRequest;
    nlohmann::json pendingSceneDocument;
    std::size_t sceneObjectCursor = 0;
    bool incrementalSceneLoad = false;
    bool stagedSceneLoad = false;
    bool beginGameSceneFlowAfterRead = false;
    std::size_t sceneFinalizeCursor = 0;
    unsigned int sceneFinalizeStage = 0;
    bool loadingPresentation = false;
    std::string sceneLoadOperation;
    bool editorPlayModeEnabled = false;
    EditorPlayMode editorPlayMode = EditorPlayMode::EDITOR;
    EditorPlayCommand pendingEditorPlayCommand = EditorPlayCommand::NONE;
    bool editorWorkspacePaused = false;
    bool gameMenuPaused = false;
    LoadingStatus presentationLoadingStatus;
    bool engineLoadingPresentation = false;
    bool presentationRetryEnabled = false;
    bool presentationRetryRequested = false;
    std::string presentationWarning;
    bool presentationSilhouetteOverride = false;
    bool presentationSilhouetteWhite = true;
    bool playInputCaptured = false;
    bool editorPlayRoot = false;
    bool editorCameraInitialized = false;
    std::string loadedSceneName;
    std::string pendingEditorSelection;

    // Scene保存/Load GUIの状態を保持する専用構造体。
    struct SceneState {
        char name[64] = "MainScene";
        std::vector<std::string> list;
        int selectedIndex = -1;
        std::vector<std::string> sceneFiles; // 読み込まれたJSONファイル名のリスト
        bool showSavePopup = false;
        bool showLoadPopup = false;
    }    sceneState;


    std::shared_ptr<GameModeManager> gameModeManager;
    std::shared_ptr<BossFinishDirector> bossFinishDirector;
    std::shared_ptr<PlayerDeathDirector> playerDeathDirector;
    std::unique_ptr<ResourceManager> resourceManager;
    std::vector<std::shared_ptr<GameObject>> objects;
    struct LayerMotionTransform
    {
        std::weak_ptr<GameObject> object;
        DirectX::XMFLOAT4X4 world;
    };
    std::unordered_map<const GameObject*, LayerMotionTransform> layerMotionHistory;
    const CameraComponent* layerMotionCamera = nullptr;
    DirectX::XMFLOAT4X4 layerMotionViewProjection{};
    // 読込・保存済みの配置データを、実行時に削除されたObjectの保存に使用する。
    std::unordered_map<std::string, nlohmann::json> sceneObjectSaveBaselines;
    std::vector<std::string> sceneObjectSaveOrder;
    std::unordered_set<std::string> runtimeDeletedSceneObjects;
    bool showRuntimeObjects = false; // 一時VFXを含む、非保存ObjectもHierarchyで調査する。
    std::unordered_map<StringId, std::vector<std::weak_ptr<GameObject>>> objectNameIndex;
    // 更新ループ中の追加・削除を安全に反映するための待機リスト。
    std::vector<std::shared_ptr<GameObject>> pendingStarts;
    std::vector<std::shared_ptr<GameObject>> pendingDestroys;
    std::shared_ptr<GameObject> selectedObject = nullptr;

    std::shared_ptr<CameraManager> cameraManager;

    std::shared_ptr<TimeManager> timeManager;
    std::unique_ptr<AttackSystem> attackSystem =
        std::make_unique<AttackSystem>();
    std::unique_ptr<JustDodgeSystem > justDodgeSystem =
        std::make_unique<JustDodgeSystem >();
    std::unique_ptr<JustGuardSystem> justGuardSystem =
        std::make_unique<JustGuardSystem>();
    std::unique_ptr<GuardSystem> guardSystem =
        std::make_unique<GuardSystem>();
    std::unique_ptr<CollisionStepSystem> collisionStepSystem =
        std::make_unique<CollisionStepSystem>();
    std::unique_ptr<EffectManager> effectManager =
        std::make_unique<EffectManager>();

    // 描画Systemの所有権はManagerが保持し、Scene全体で共有する。
    std::unique_ptr<ParticleRenderSystem> particleRenderSystem;
    std::unique_ptr<MeshVFXRenderSystem> meshVFXRenderSystem;
    std::unique_ptr<MotionVelocityRenderSystem> motionVelocityRenderSystem;
    // ParticleComponentだけを弱参照で保持し、失効したものはFrame終端で掃除する。
    std::vector<std::weak_ptr<ParticleComponent>> particleComponents;
    // 監視サービスと専用ObjectはBossの親子階層・寿命から独立させる。
    std::shared_ptr<CombatObservationSystem> combatObservationSystem;
    std::weak_ptr<GameObject> combatObservationObject;
    void EnsureCombatObservationRuntime();
    // 現在のScene。
    Scene* currentScene = nullptr;
    std::vector<std::string> prefabFiles; // プレハブ名のリスト
    int selectedPrefabIndex = 0;          // GUIでの選択位置
    const std::string prefabDirectory = "Data/Prefabs/";
    bool shapeRenderEnabled = true;
    bool navMeshRenderEnabled = false;
    ModelRenderSystem* modelRenderSystem = nullptr;
    SCENE_RUNTIME_STATE sceneRuntimeState = SCENE_RUNTIME_STATE::RUNNING;
    float sceneLoadProgress = 1.0f;
    std::string sceneRuntimeError;
    void UpdateLoadingRuntime(float elapsedTime_);
    void UpdateRunningRuntime(float elapsedTime_);
    void ProcessPendingStarts();
    bool WaitRequiredAssets();
    bool WaitRuntimeReady();
    bool HasRequiredRuntimeFailure();
    void FinalizeSceneInitialization();
    void FailSceneRuntime(const std::string& message_);
    void RegisterAttackCallbacks();
    void EnsureGlobalActionObjects();
    void RegisterObjectNameIndex(const std::shared_ptr<GameObject>& object_);
    void UnregisterObjectNameIndex(GameObject* object_, const std::string& name_);

public:
        SceneState& GetSceneState()
    {
        return sceneState;
    }
};
