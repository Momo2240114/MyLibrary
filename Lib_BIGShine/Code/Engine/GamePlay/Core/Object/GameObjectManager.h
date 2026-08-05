#pragma once

#include "GameObject.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/MotionVelocityRenderSystem.h"
#include "Engine/GamePlay/Scene/IScene.h"
#include "Engine/Physics/CollisionStepSystem.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/System/JustDodgeSystem.h"
#include "Engine/GamePlay/System/GuardSystem.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Game/GameMode/GameModeManager.h"

class ColliderComponent;
class BoxCollider;
class GltfModelCommponennt;
class TimeManager;
class CameraManager;
class MotionVelocityComponent;
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


#include"Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include"Engine/Graphics/VFX/MeshVFXRenderSystem.h"
#include"Engine/GamePlay/Core/Resource/ResourceManager.h"


/// GameObjectの生成、更新、描画登録、遅延削除を統括する管理クラス。
/// フレーム更新中のコンテナ変更を避けるため、初期化と削除は待機リストを介して処理する。
class GameObjectManager
{
public:
    void Initialize();

    template<typename... Args>
    std::shared_ptr<GameObject> Create(const std::string& name_, Args&&... args_)
    {
        const std::string& baseName_ = name_; // ← これ追加

        // 同名Objectがある場合は末尾番号を付け、Scene内で名前が衝突しないようにする。
        int count_ = 0;
        for (auto& o : objects)
        {
            const std::string& n_ = o->nameComponent->name;
            if (n_ == baseName_ || n_.rfind(baseName_ + "_", 0) == 0)
                count_++;
        }

        // 最終的に使うObject名を決定する。
        std::string finalName_ = baseName_;
        if (count_ > 0)
        {
            char buf_[32];
            sprintf_s(buf_, sizeof(buf_), "_%02d", count_);   // ← こっち
            finalName_ += buf_;
        }

        // Objectを生成し、ManagerとTransformの所有Object参照を設定する。
        auto newObject_ = std::make_shared<GameObject>();
        newObject_->nameComponent->name = finalName_;
        newObject_->transform->gameObject = newObject_;

        newObject_->SetManager(this);
        objects.emplace_back(newObject_);       // 全体リストに追加
        // StartはUpdate中の追加にも対応できるよう、即時ではなく待機リストで処理する。
        pendingStarts.push_back(newObject_);
        return newObject_;
    }


    // 名前でObjectを検索する。
    std::shared_ptr<GameObject> Find(const std::string& name_)
    {
        for (auto& o : objects)
            if (o->nameComponent->name == name_)
            {
                return o;
            }
        return nullptr;
    }

    template <typename T>
    std::vector<std::shared_ptr<T>> FindComponents()
    {
        std::vector<std::shared_ptr<T>> results_;

        // 全Objectから指定型Componentを1つずつ集める。
        for (auto& object_ : objects)
        {
            if (!object_) continue;

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

    void RenderAll(SpriteRenderSystem& spriteSystem, ModelRenderSystem& modelSystem, ID3D11RenderTargetView* finalRenderTargetView_ = nullptr);
    void SetModelRenderSystem(ModelRenderSystem* modelRenderSystem_) { modelRenderSystem = modelRenderSystem_; }
    void RegisterMotionVelocityComponent(const std::shared_ptr<MotionVelocityComponent>& component_);
    void UnregisterMotionVelocityComponent(MotionVelocityComponent* component_);

    void Update(float elapsedTime_);
    SCENE_RUNTIME_STATE GetSceneRuntimeState() const { return sceneRuntimeState; }
    float GetSceneLoadProgress() const { return sceneLoadProgress; }
    const std::string& GetSceneRuntimeError() const { return sceneRuntimeError; }

    void Submit(ID3D11DeviceContext* context_)
    {
        // 全Objectの描画Submitをまとめて呼ぶ。
        for (auto& object_ : objects)
            object_->Submit(context_);
    }

    // 指定したObjectを削除予約する。
    void Destroy(std::shared_ptr<GameObject> object_);
    void Destroy(std::string objectName_);

    // 全てのObjectを解放する。
    void Clear();

    // 選択中オブジェクト
    void SelectObject(std::shared_ptr<GameObject> object_) { selectedObject = object_; }
    std::shared_ptr<GameObject> GetSelectedObject() { return selectedObject; }

    void AddObject(std::shared_ptr<GameObject> object_);
    void DetachObject(const std::shared_ptr<GameObject>& object_);
    void RemoveObject(const std::string& name_);

    // 状態を尋ねる窓口。
    const std::vector<std::shared_ptr<GameObject>>& GetObjects() const { return objects; }

    // GUIは選択中Objectだけ表示する。
    void Gui();
    void DrawHierarchyNode(std::shared_ptr<GameObject> object_, GameObject** objectToDelete_);

    std::shared_ptr<GameObject> GetSelectedObject() const { return selectedObject; }
    void SetSelectedObject(std::shared_ptr<GameObject> object_) { selectedObject = object_; }

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
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabName_);
    // Prefabsフォルダ内のファイル一覧を更新
    void RefreshPrefabList();

    void ApplyDestroys(std::shared_ptr<GameObject> object_);

    void SetShapeRenderEnabled(bool enabled_) { shapeRenderEnabled = enabled_; }
    bool IsShapeRenderEnabled() const { return shapeRenderEnabled; }
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
    ResourceManager* GetResourceManager() { return resourceManager.get(); }

    void DrawHierarchyGui();   // ヒストリー(階層)GUI
    void DrawInspectorGui();   // インスペクターGUI
    void DrawSceneModals(); // モーダルを描画する専用の儀式
    void RefreshSceneList() { sceneState.list = GetSceneDirectories(); }
    std::shared_ptr<TimeManager> GetTimeManager() { return timeManager; }

    GameModeManager* GetGameModeManager(){return gameModeManager.get();}
    const GameModeManager* GetGameModeManager() const{return gameModeManager.get();}

    CollisionStepSystem& GetCollisionStepSystem() { return *collisionStepSystem; }
    AttackSystem& GetAttackSystem() { return *attackSystem; }
    JustDodgeSystem& GetJustDodgeSystem() { return *justDodgeSystem; }
    GuardSystem& GetGuardSystem() { return *guardSystem; }
    EffectManager& GetEffectManager() { return *effectManager; }
private:
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
    std::unique_ptr<ResourceManager> resourceManager;
    std::vector<std::shared_ptr<GameObject>> objects;
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
public:
        SceneState& GetSceneState() { return sceneState; }
};
