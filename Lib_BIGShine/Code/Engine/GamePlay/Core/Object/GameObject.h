#pragma once

#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <string>

#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

class ColliderComponent;
class GameObjectManager;


class GameObject : public std::enable_shared_from_this<GameObject>
{
public:
    enum class LayerType : uint32_t
    {
        Default = 0,
        Player = 1 << 0,
        Enemy = 1 << 1,
        PlayerBullet = 1 << 2,
        EnemyBullet = 1 << 3,
        UI = 1 << 4,
        Environment = 1 << 5,
        SneakObject = 1 << 6,
        Terrain = 1 << 7,
        MaxLayers = 1 << 8,
        Wall = 1 << 9,
        Item = 1 << 10,
    };

    LayerType layer = LayerType::Default;   // ★追加
    uint32_t m_Mask = (uint32_t)LayerType::Default;//コリジョンマスク

    bool saveFlg = true;
    std::string name;
    std::shared_ptr<Transform> transform;
    std::vector<std::shared_ptr<Component>> components;

    //コリジョン用のコンポーネント
    //std::vector<std::shared_ptr<Component>> m_pColliderComponents;
    std::shared_ptr<Component> m_pColliderComponent;
    // --- 追加：自分専用のレジスタ ---
    ActionRegistry localRegistry;

    //自身が属する宇宙（マネージャー）へのポインタ
    GameObjectManager* m_pManager = nullptr;
public:
    //このオブジェクトを統べるマネージャーへのアクセス
    GameObjectManager& GetManager() const { return *m_pManager; }

    // マネージャーを設定する（生成時に呼ばれる）
    void SetManager(GameObjectManager* manager) { m_pManager = manager; }

    // ゲッター
    ActionRegistry& GetLocalRegistry() { return localRegistry; };

    GameObject(const std::string& name_ = "GameObject")
        : name(name_)
    {
        transform = std::make_shared<Transform>();
    }

    void DrawGizmo(const float* view, const float* projection);

    template<typename T>
    std::shared_ptr<T> GetOrAddComponent(bool collStartFlg = true)
    {
        // 1. まず現在のリストから検索
        auto comp = GetComponent<T>();

        // 2. 見つかったらそれを返す（二重生成しない）
        if (comp) return comp;

        // 3. なければ新しく作って追加
        return AddComponent<T>(collStartFlg);
    }

    void SetName(const char* name_) { name = name_; }
    std::string GetName() { return name; }


    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args_,bool collStartFlg = true)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");

        auto comp = std::make_shared<T>(std::forward<Args>(args_)...);
        if (!comp) return nullptr; // 生成失敗

        comp->gameObject = shared_from_this();

        //コライダーコンポーネントを来てクラスするクラスがTなのであれば
        if constexpr (std::is_base_of<ColliderComponent, T>::value)
        {
            m_pColliderComponent = comp;
        }
        else
        {
            components.push_back(comp);
        }

        //components.push_back(comp);
        if(collStartFlg)
        {
            if constexpr (std::is_base_of<ColliderComponent, T>::value)
            {
                m_pColliderComponent->gameObject = shared_from_this();
                m_pColliderComponent->Start();
                m_pColliderComponent->SetStarted(true);
            }
            else
            {
                comp->Start();
                comp->SetStarted(true);
            }
        }
        return comp;
    }

    template<typename T>
    std::shared_ptr<T> GetComponent()
    {
        for (auto& comp : components)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(comp))
            {
                return casted;
            }
        }

        //コライダーコンポーネント
        if (m_pColliderComponent != nullptr)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(m_pColliderComponent))
                return casted;
        }
        return nullptr;
    }

    std::vector<std::string> GetComponentNames() const;

    void CallAllComponentStart();
    
    // ========================================
    // 名前でコンポーネントを取得
    // ========================================
    std::shared_ptr<Component> GetComponentByName(const std::string& compName_) const;
  

    // 型付きで取得（名前→Tにキャスト）
    template<typename T>
    std::shared_ptr<T> GetComponentByNameAs(const std::string& compName) const
    {
        auto comp = GetComponentByName(compName);
        if (!comp) return nullptr;
        return std::dynamic_pointer_cast<T>(comp);
    }

    void SetParent(std::shared_ptr<GameObject> parentGO);

    void Update(float deltaTime_);
   
    //遅延更新
    void LateUpdate(float et);

    void UpdateTransform()
    {
        transform->UpdateWorldMatrix();
    }

    void Submit(ID3D11DeviceContext* context_)
    {
        for (auto& comp : components)
            if (comp->IsEnabled())
                comp->Submit(context_);
    }

    void Gui();

    // 選択中オブジェクト用ウィンドウ
    void GuiWindow()
    {
        ImGui::Begin(name.c_str());
        Gui(); // 元の GUI 呼び出し
        ImGui::End();
    }

    void ExecuteAction(const std::string& actionName, std::shared_ptr<GameObject> target = nullptr);

    void Clear();
    nlohmann::json Save() const;
    void Load(const nlohmann::json& j);

    void SetNoSaveObject() { saveFlg = false; };
    void SetLayer(LayerType type) { layer = type; }
    LayerType GetLayer() const { return layer; }
};
