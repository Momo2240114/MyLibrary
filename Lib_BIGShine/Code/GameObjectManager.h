#pragma once

#include "GameObject.h"
#include "SpriteRenderSystem.h"
#include "ModelRenderSystem.h"
#include"IScene.h"

class ColliderComponent;
class BoxCollider;
class GltfModelCommponennt;

#include"RayCastManager.h"
#include"ParticleRenderSystem.h"



class GameObjectManager
{
public:
    static GameObjectManager& Instance()
    {
        static GameObjectManager inst;
        return inst;
    }

    static GameObjectManager& LoadingInstance()
    {
        static GameObjectManager loadInst;
        return loadInst;
    }

    void Initialize();

    template<typename... Args>
    std::shared_ptr<GameObject> Create(const std::string& name_, Args && ...args_)
    {
        const std::string& baseName_ = name_; // ← これ追加

        // 重複名数える
        int count_ = 0;
        for (auto& o : objects)
        {
            const std::string& n_ = o->name;
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

        // オブジェクト作成
        auto obj = std::make_shared<GameObject>(finalName_, std::forward<Args>(args_)...);
        objects.emplace_back(obj);       // 全体リストに追加
        pendingStarts.emplace_back(obj); // ★初期化待ちリストに追加
        return obj;
    }


    // name検索
    std::shared_ptr<GameObject> Find(const std::string& name_)
    {
        if (name_ == "MainCamera")
            return m_pMainCamera;

        for (auto& o : objects)
            if (o->name == name_)
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

    void RenderAll(SpriteRenderSystem spriteSystem_, ModelRenderSystem modelSystem_);

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



    //メインカメラをセットする
    void SetMainCamera(std::shared_ptr<GameObject> mainCamera)
    {
        m_pMainCamera = mainCamera;
    }

    GameObject* GetMainCamera()
    {
        return m_pMainCamera.get();
    }


    void ChangeScene(Scene* scene)
    {
        //delete m_pCurrentScene;
        //m_pCurrentScene = nullptr;

        m_pCurrentScene = scene;
    }

    //Scene* GetCurrentScene()
    //{
    //    return m_pCurrentScene;
    //}

    void SaveScene(const std::string& sceneName);
    void LoadScene(const std::string& sceneName);

    void ResolveHierarchy();



    // 指定したオブジェクトをプレハブとして保存
    void SaveAsPrefab(std::shared_ptr<GameObject> obj, const std::string& prefabName);

    RayCastManager* GetRayCastManager()
    {
        return m_pRayCastManager.get();
    }

    //SearchAreaManager* GetSearchAreaManager();

    void InitializeAllComponents();

    std::vector<std::shared_ptr<GameObject>>& GetObjects()
    {
        return objects;
    }

    // プレハブからオブジェクトを生成
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabName);
    // Prefabsフォルダ内のファイル一覧を更新
    void RefreshPrefabList();

    void ApplyDestroys(std::shared_ptr<GameObject> obj);

    void SetShapeRenderFlg(bool f) { shapeRenderFlg = f; }


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
    std::vector<std::shared_ptr<GameObject>> objects;
    // まだ Startが呼ばれていないオブジェクトのリスト
    std::vector<std::shared_ptr<GameObject>> pendingStarts;
    std::vector<std::shared_ptr<GameObject>> pendingDestroys;
    std::shared_ptr<GameObject> selectedObject = nullptr;
    //メインカメラを保持する
    std::shared_ptr<GameObject> m_pMainCamera;

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
};

