#pragma once

#include "GameObject.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/GamePlay/Scene/IScene.h"
#include "Engine/Physics/CollisionStepSystem.h"
#include "Engine/GamePlay/System/AttackSystem.h"
#include "Engine/GamePlay/System/JustDodgeSystem.h"
#include "Engine/GamePlay/System/EffectManager.h"

class ColliderComponent;
class BoxCollider;
class GltfModelCommponennt;
class TimeManager;
class CameraManager;


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

        // 重複名数える
        int count_ = 0;
        for (auto& o : objects)
        {
            const std::string& n_ = o->nameComponent->name;
            if (n_ == baseName_ || n_.rfind(baseName_ + "_", 0) == 0)
                count_++;
        }

        // final name 決定
        std::string finalName_ = baseName_;
        if (count_ > 0)
        {
            char buf_[32];
            sprintf_s(buf_, sizeof(buf_), "_%02d", count_);   // ← こっち
            finalName_ += buf_;
        }

        // オブジェクトの生成
        auto newObject_ = std::make_shared<GameObject>();
        newObject_->nameComponent->name = finalName_;
        newObject_->transform->gameObject = newObject_;

        //ここで「自身」をオブジェクトに
        newObject_->SetManager(this);
        objects.emplace_back(newObject_);       // 全体リストに追加
        // --- 既存の pendingStarts への追加や初期化処理 ---
        pendingStarts.push_back(newObject_);
        return newObject_;
    }


    // name検索
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

        // 全オブジェクトを走査
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

    void RenderAll(SpriteRenderSystem& spriteSystem, ModelRenderSystem& modelSystem);
    void SetModelRenderSystem(ModelRenderSystem* modelRenderSystem_) { modelRenderSystem = modelRenderSystem_; }

    void Update(float elapsedTime_);

    void Submit(ID3D11DeviceContext* context_)
    {
        for (auto& object_ : objects)
            object_->Submit(context_);
    }

    // 1. 指定したオブジェクトを削除する
    void Destroy(std::shared_ptr<GameObject> object_);
    void Destroy(std::string objectName_);

    // 2. 全てのオブジェクトを解放する（Live Object対策）
    void Clear();

    // 選択中オブジェクト
    void SelectObject(std::shared_ptr<GameObject> object_) { selectedObject = object_; }
    std::shared_ptr<GameObject> GetSelectedObject() { return selectedObject; }

    void AddObject(std::shared_ptr<GameObject> object_);
    void DetachObject(const std::shared_ptr<GameObject>& object_);
    void RemoveObject(const std::string& name_);

    // 状態を尋ねる窓口
    const std::vector<std::shared_ptr<GameObject>>& GetObjects() const { return objects; }

    // GUI は選択中オブジェクトだけ表示
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

    //すべてのオブジェクトやコンポーネントが平等にアクセスするための窓口
    ResourceManager* GetResourceManager() { return resourceManager.get(); }

    void DrawHierarchyGui();   // ヒストリー(階層)GUI
    void DrawInspectorGui();   // インスペクターGUI
    void DrawSceneModals(); // モーダルを描画する専用の儀式
    void RefreshSceneList() { sceneState.list = GetSceneDirectories(); }
    std::shared_ptr<TimeManager> GetTimeManager() { return timeManager; }
    CollisionStepSystem& GetCollisionStepSystem() { return *collisionStepSystem; }
    AttackSystem& GetAttackSystem() { return *attackSystem; }
    JustDodgeSystem& GetJustDodgeSystem() { return *justDodgeSystem; }
    EffectManager& GetEffectManager() { return *effectManager; }
private:
    // unique name helper
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
    // GUIの状態を保持する専用の構造体
    struct SceneState {
        char name[64] = "MainScene";
        std::vector<std::string> list;
        int selectedIndex = -1;
        std::vector<std::string> sceneFiles; // 読み込まれたJSONファイル名のリスト
        bool showSavePopup = false;
        bool showLoadPopup = false;
    }    sceneState;


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
    std::unique_ptr<CollisionStepSystem> collisionStepSystem =
        std::make_unique<CollisionStepSystem>();
    std::unique_ptr<EffectManager> effectManager =
        std::make_unique<EffectManager>();

    // 描画システムの所有権はマネージャが保持し、シーン全体で共有する。
    std::unique_ptr<ParticleRenderSystem> particleRenderSystem;
    std::unique_ptr<MeshVFXRenderSystem> meshVFXRenderSystem;
    // パーティクルコンポーネントのみを保持するリスト
    std::vector<std::weak_ptr<ParticleComponent>> particleComponents;
    //現在のシーン
    Scene* currentScene = nullptr;
    std::vector<std::string> prefabFiles; // プレハブ名のリスト
    int selectedPrefabIndex = 0;          // GUIでの選択位置
    const std::string prefabDirectory = "Data/Prefabs/";
    bool shapeRenderEnabled = true;
    bool navMeshRenderEnabled = false;
    ModelRenderSystem* modelRenderSystem = nullptr;
    void RegisterAttackCallbacks();
    void EnsureGlobalActionObjects();
public:
        SceneState& GetSceneState() { return sceneState; }
};

