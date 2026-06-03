#pragma once

#include "GameObject.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/GamePlay/Scene/IScene.h"

class ColliderComponent;
class BoxCollider;
class GltfModelCommponennt;
class TimeManager;
class CameraManager;

#include"RayCastManager.h"
#include"Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include"Engine/GamePlay/Core/Resource/ResourceManager.h"


class GameObjectManager
{
public:
    void Initialize();

    template<typename... Args>
    std::shared_ptr<GameObject> Create(const std::string& name_, Args && ...args_)
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
        auto newObj = std::make_shared<GameObject>();
        newObj->nameComponent->name = finalName_;

        //ここで「自身」をオブジェクトに授ける！
        newObj->SetManager(this);
        objects.emplace_back(newObj);       // 全体リストに追加
        // --- 既存の pendingStarts への追加や初期化処理 ---
        pendingStarts.push_back(newObj);
        return newObj;
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
        std::vector<std::shared_ptr<T>> results;

        // 全オブジェクトを走査
        for (auto& obj : objects)
        {
            if (!obj) continue;

            // 各オブジェクトのコンポーネントリストを走査
            // ※GameObject側にコンポーネントを取得する GetComponent<T> がある前提です
            auto comp = obj->GetComponent<T>();
            if (comp)
            {
                results.push_back(comp);
            }
        }
        return results;
    }

    void RenderAll(SpriteRenderSystem& spriteSystem_, ModelRenderSystem& modelSystem_);

    void Update(float et_);

    void Submit(ID3D11DeviceContext* context_)
    {
        for (auto& obj : objects)
            obj->Submit(context_);
    }

    // 1. 指定したオブジェクトを削除する
    void Destroy(std::shared_ptr<GameObject> obj);
    void Destroy(std::string objName);

    // 2. 全てのオブジェクトを解放する（Live Object対策）
    void Clear();

    // 選択中オブジェクト
    void SelectObject(std::shared_ptr<GameObject> obj) { selectedObject = obj; }
    std::shared_ptr<GameObject> GetSelectedObject() { return selectedObject; }

    void AddObject(std::shared_ptr<GameObject> obj);
    void RemoveObject(const std::string& name);

    // 状態を尋ねる窓口
    const std::vector<std::shared_ptr<GameObject>>& GetObjects() const { return objects; }

    void CollisionStep();

    // GUI は選択中オブジェクトだけ表示
    void Gui();
    void DrawHierarchyNode(std::shared_ptr<GameObject> obj, GameObject** toDelete);

    void HandlePhysXTrigger(
        ColliderComponent* colA, ColliderComponent* colB,
        bool hit
    );
    void HandlePhysXContact(
        ColliderComponent* colA, ColliderComponent* colB
    );

    std::shared_ptr<GameObject> GetSelectedObject() const { return selectedObject; }
    void SetSelectedObject(std::shared_ptr<GameObject> obj) { selectedObject = obj; }

    void ChangeScene(Scene* scene)
    {
        //delete m_pCurrentScene;
        //m_pCurrentScene = nullptr;

        m_pCurrentScene = scene;
    }

    void SaveScene(const std::string& sceneName);
    void LoadScene(const std::string& sceneName);

    void ResolveHierarchy();

    std::vector<std::string> GetSceneDirectories();

    // 指定したオブジェクトをプレハブとして保存
    void SaveAsPrefab(std::shared_ptr<GameObject> obj, const std::string& prefabName);

    RayCastManager* GetRayCastManager()
    {
        return m_pRayCastManager.get();
    }

    void InitializeAllComponents();

    std::vector<std::shared_ptr<GameObject>>& GetObjects()
    {
        return objects;
    }

    CameraManager* GetCameraManager();

    // プレハブからオブジェクトを生成
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabName);
    // Prefabsフォルダ内のファイル一覧を更新
    void RefreshPrefabList();

    void ApplyDestroys(std::shared_ptr<GameObject> obj);

    void SetShapeRenderFlg(bool f) { shapeRenderFlg = f; }

    //すべてのオブジェクトやコンポーネントが平等にアクセスするための窓口
    ResourceManager* GetResourceManager() { return _ResourceManager.get(); }

    void DrawHierarchyGui();   // ヒストリー(階層)GUI
    void DrawInspectorGui();   // インスペクターGUI
    void DrawSceneModals(); // モーダルを描画する専用の儀式
    void RefreshSceneList() { m_sceneState.list = GetSceneDirectories(); }
    std::shared_ptr<TimeManager> GetTimeManger() { return _TimeManager; }
private:
    // unique name helper
    std::string MakeUniqueName(std::string base)
    {
        std::string name = base;
        int index = 1;

        // 名前が既存のオブジェクトと重複している間、ずっとループする
        while (Find(name))
        {
            char buf[32];
            sprintf_s(buf, sizeof(buf), "_%02d", index);
            name = base + buf; // 例: Enemy -> Enemy_01 -> Enemy_02 ...
            index++;
        }
        return name;
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
    }    m_sceneState;


    std::unique_ptr<ResourceManager> _ResourceManager;
    std::vector<std::shared_ptr<GameObject>> objects;
    // まだ Startが呼ばれていないオブジェクトのリスト
    std::vector<std::shared_ptr<GameObject>> pendingStarts;
    std::vector<std::shared_ptr<GameObject>> pendingDestroys;
    std::shared_ptr<GameObject> selectedObject = nullptr;

    std::shared_ptr<CameraManager> _CameraManager;

    std::shared_ptr<TimeManager> _TimeManager;
    //ナビメッシュマネージャー
    //NaviMeshManager* m_pNaviMeshManager;
    // 所有権をマネージャ側が持つ
    std::unique_ptr<RayCastManager>m_pRayCastManager;
    std::unique_ptr<ParticleRenderSystem> particleSystem;
    // パーティクルコンポーネントのみを保持するリスト
    std::vector<std::weak_ptr<ParticleComponent>> m_particleComponents;
    //現在のシーン
    Scene* m_pCurrentScene;
    std::vector<std::string> m_prefabFiles; // プレハブ名のリスト
    int m_selectedPrefabIndex = 0;          // GUIでの選択位置
    const std::string m_prefabDirectory = "Prefabs/";
    bool shapeRenderFlg = true;
    bool naviMeshRenderFlg = false;
public:
        SceneState& GetSceneState() { return m_sceneState; }
};

