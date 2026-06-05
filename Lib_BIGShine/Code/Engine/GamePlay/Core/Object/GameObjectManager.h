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


#include"Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include"Engine/GamePlay/Core/Resource/ResourceManager.h"


class GameObjectManager
{
public:
    void Initialize();

    template<typename... Args>
    std::shared_ptr<GameObject> Create(const std::string& name, Args && ...args)
    {
        const std::string& baseName = name; // ← これ追加

        // 重複名数える
        int count_ = 0;
        for (auto& o : _Objects)
        {
            const std::string& n_ = o->nameComponent->name;
            if (n_ == baseName || n_.rfind(baseName + "_", 0) == 0)
                count_++;
        }

        // final name 決定
        std::string finalName = baseName;
        if (count_ > 0)
        {
            char buf_[32];
            sprintf_s(buf_, sizeof(buf_), "_%02d", count_);   // ← こっち
            finalName += buf_;
        }

        // オブジェクトの生成
        auto newObj = std::make_shared<GameObject>();
        newObj->nameComponent->name = finalName;
        newObj->transform->gameObject = newObj;

        //ここで「自身」をオブジェクトに授ける！
        newObj->SetManager(this);
        _Objects.emplace_back(newObj);       // 全体リストに追加
        // --- 既存の pendingStarts への追加や初期化処理 ---
        _PendingStarts.push_back(newObj);
        return newObj;
    }


    // name検索
    std::shared_ptr<GameObject> Find(const std::string& name)
    {
        for (auto& o : _Objects)
            if (o->nameComponent->name == name)
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
        for (auto& obj : _Objects)
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

    void RenderAll(SpriteRenderSystem& spriteSystem, ModelRenderSystem& modelSystem);

    void Update(float et);

    void Submit(ID3D11DeviceContext* context)
    {
        for (auto& obj : _Objects)
            obj->Submit(context);
    }

    // 1. 指定したオブジェクトを削除する
    void Destroy(std::shared_ptr<GameObject> obj);
    void Destroy(std::string objName);

    // 2. 全てのオブジェクトを解放する（Live Object対策）
    void Clear();

    // 選択中オブジェクト
    void SelectObject(std::shared_ptr<GameObject> obj) { _SelectedObject = obj; }
    std::shared_ptr<GameObject> GetSelectedObject() { return _SelectedObject; }

    void AddObject(std::shared_ptr<GameObject> obj);
    void RemoveObject(const std::string& name);

    // 状態を尋ねる窓口
    const std::vector<std::shared_ptr<GameObject>>& GetObjects() const { return _Objects; }

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

    std::shared_ptr<GameObject> GetSelectedObject() const { return _SelectedObject; }
    void SetSelectedObject(std::shared_ptr<GameObject> obj) { _SelectedObject = obj; }

    void ChangeScene(Scene* scene)
    {
        _CurrentScene = scene;
    }

    void SaveScene(const std::string& sceneName);
    void LoadScene(const std::string& sceneName);

    void ResolveHierarchy();

    std::vector<std::string> GetSceneDirectories();

    // 指定したオブジェクトをプレハブとして保存
    void SaveAsPrefab(std::shared_ptr<GameObject> obj, const std::string& prefabName);


    void InitializeAllComponents();

    std::vector<std::shared_ptr<GameObject>>& GetObjects()
    {
        return _Objects;
    }

    CameraManager* GetCameraManager();

    // プレハブからオブジェクトを生成
    std::shared_ptr<GameObject> Instantiate(const std::string& prefabName);
    // Prefabsフォルダ内のファイル一覧を更新
    void RefreshPrefabList();

    void ApplyDestroys(std::shared_ptr<GameObject> obj);

    void SetShapeRenderFlg(bool f) { _ShapeRenderFlg = f; }

    //すべてのオブジェクトやコンポーネントが平等にアクセスするための窓口
    ResourceManager* GetResourceManager() { return _ResourceManager.get(); }

    void DrawHierarchyGui();   // ヒストリー(階層)GUI
    void DrawInspectorGui();   // インスペクターGUI
    void DrawSceneModals(); // モーダルを描画する専用の儀式
    void RefreshSceneList() { _SceneState.list = GetSceneDirectories(); }
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
    }    _SceneState;


    std::unique_ptr<ResourceManager> _ResourceManager;
    std::vector<std::shared_ptr<GameObject>> _Objects;
    // まだ Startが呼ばれていないオブジェクトのリスト
    std::vector<std::shared_ptr<GameObject>> _PendingStarts;
    std::vector<std::shared_ptr<GameObject>> _PendingDestroys;
    std::shared_ptr<GameObject> _SelectedObject = nullptr;

    std::shared_ptr<CameraManager> _CameraManager;

    std::shared_ptr<TimeManager> _TimeManager;

    // 所有権をマネージャ側が持つ
    std::unique_ptr<ParticleRenderSystem> _ParticleSystem;
    // パーティクルコンポーネントのみを保持するリスト
    std::vector<std::weak_ptr<ParticleComponent>> _ParticleComponents;
    //現在のシーン
    Scene* _CurrentScene;
    std::vector<std::string> _PrefabFiles; // プレハブ名のリスト
    int _SelectedPrefabIndex = 0;          // GUIでの選択位置
    const std::string _PrefabDirectory = "Data/Prefabs/";
    bool _ShapeRenderFlg = true;
    bool _NaviMeshRenderFlg = false;
public:
        SceneState& GetSceneState() { return _SceneState; }
};

