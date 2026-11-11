#pragma once

#include <memory>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <string>
#include <utility>

#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Core/Component/NameComponent.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

class ColliderComponent;
class GameObjectManager;

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

// Scene上に配置されるObject本体。
// Transform、Component群、Collider専用Component、Object固有Actionをまとめて保持する。
class GameObject : public std::enable_shared_from_this<GameObject>
{
public: 

    LayerType layer = LayerType::Default;   // Collisionや描画判定に使うLayer。
    uint32_t mask = (uint32_t)LayerType::Default; // Collision対象を絞るMask。

    bool saveFlg = true;
    bool requiredForSceneStart = false;
    std::shared_ptr<NameComponent> nameComponent;
    std::shared_ptr<Transform> transform;
    std::vector<std::shared_ptr<Component>> components;

    // Collision用Componentは通常Componentと分けて、Physics登録と更新順を管理する。
    std::vector<std::shared_ptr<Component>> colliderComponents;
    // Object専用Action。AnimationやSequenceからObject単位の処理を呼ぶために使う。
    ActionRegistry localRegistry;

    // 自身が属するGameObjectManagerへのポインタ。
    GameObjectManager* manager = nullptr;
private:
    bool pendingDestroy = false;
    bool combatReady = true;
    float runtimePlaybackRate = 1.0f; // 演出・暴走用の共通時計倍率。保存データには書き込まない。
    bool runtimeHierarchyVisible = false; // Scene保存対象とは独立した、実行時ObjectのHierarchy表示。
    std::vector<std::pair<std::string, std::string>> globalActions;
public:
    bool IsPendingDestroy() const { return pendingDestroy; }
    bool IsCombatReady() const { return combatReady && !pendingDestroy; }
    void SetCombatReady(bool ready_) { combatReady = ready_; }
    void SetRuntimePlaybackRate(float rate_) { runtimePlaybackRate = (std::clamp)(rate_, 0.1f, 3.0f); }
    float GetRuntimePlaybackRate() const { return runtimePlaybackRate; }
    bool IsRuntimeHierarchyVisible() const { return runtimeHierarchyVisible; }
    void SetRuntimeHierarchyVisible(bool visible_) { runtimeHierarchyVisible = visible_; }
    // 削除予約時点で更新・戦闘・物理・Actionを停止する。複数回の通知は無視する。
    void PrepareForDestroy();
    // 起動待ちのPrefab階層全体が、演出Actionを受け取れる状態か判定する。
    bool IsHierarchyReady();
    // Objectに所属する、接頭辞なしのGlobal Actionも寿命管理の対象にする。
    void RegisterNamedGlobalAction(const std::string& category_, const std::string& name_, const ActionRegistry::ActionFunc& function_);
    // このObjectを管理しているManagerへのアクセス。
    GameObjectManager& GetManager() const { return *manager; }

    // マネージャーを設定する（生成時に呼ばれる）
    void SetManager(GameObjectManager* manager_);

    void RegisterAction(
        const std::string& category_,
        const std::string& name_,
        const ActionRegistry::ActionFunc& func_
    );
    std::string BuildActionName(const std::string& actionName_) const;
    std::string RegisterGlobalAction(
        const std::string& category_,
        const std::string& actionName_,
        const ActionRegistry::ActionFunc& function_);


    // ゲッター
    ActionRegistry& GetLocalRegistry() { return localRegistry; };

    GameObject(const std::string& name_ = "GameObject")
    {
        transform = std::make_shared<Transform>();

        // コンポーネントとして名前を付与
        nameComponent = std::make_shared<NameComponent>();
        nameComponent->name = name_;
    }

    void DrawGizmo(const float* view, const float* projection);

    template<typename T>
    std::shared_ptr<T> GetOrAddComponent(bool collStartFlg = true)
    {
        // 1. まず現在のリストから検索する。
        auto comp = GetComponent<T>();

        // 2. 見つかったらそれを返し、二重生成を防ぐ。
        if (comp) return comp;

        // 3. なければ新しく作って追加する。
        return AddComponent<T>(collStartFlg);
    }

    // 名前へのアクセスを簡潔に
    const std::string& GetName() const { return nameComponent->name; }
    void SetName(const std::string& newName_);


    template<typename T, typename... Args>
    std::shared_ptr<T> AddComponent(Args&&... args_,bool collStartFlg = true)
    {
        static_assert(std::is_base_of<Component, T>::value, "T must inherit from Component");

        auto comp = std::make_shared<T>(std::forward<Args>(args_)...);
        if (!comp) return nullptr; // 生成失敗

        comp->SetGameObject(shared_from_this());

        // Collider系はPhysics登録が必要なため、専用リストへ分けて保持する。
        if constexpr (std::is_base_of<ColliderComponent, T>::value)
        {
            colliderComponents.push_back(comp);
            RegisterColliderComponent(comp);
        }
        else
        {
            components.push_back(comp);
        }

        NotifyComponentCompositionChanged();

        if(collStartFlg)
        {
            if constexpr (std::is_base_of<ColliderComponent, T>::value)
            {
                comp->Start();
                comp->SetStarted(true);
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
        // 通常ComponentとColliderComponentの両方から、指定型に一致するものを探す。
        for (auto& comp : components)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(comp))
            {
                return casted;
            }
        }

        for (auto& collider_ : colliderComponents)
        {
            if (auto casted = std::dynamic_pointer_cast<T>(collider_))
                return casted;
        }
        return nullptr;
    }

    template<typename T>
    std::vector<std::shared_ptr<T>> GetComponents()
    {
        // 同じ型のComponentを複数持つ場合に備え、該当するものをすべて返す。
        std::vector<std::shared_ptr<T>> results_;
        for (auto& comp_ : components)
        {
            if (auto casted_ = std::dynamic_pointer_cast<T>(comp_)) results_.push_back(casted_);
        }
        for (auto& collider_ : colliderComponents)
        {
            if (auto casted_ = std::dynamic_pointer_cast<T>(collider_)) results_.push_back(casted_);
        }
        return results_;
    }

    std::vector<std::string> GetComponentNames() const;

    void CallAllComponentStart();
    bool IsReadyToStart();
    bool IsRuntimeReady() const;
    bool HasRuntimeFailure() const;
    bool IsRequiredForSceneStart() const;
    void SetRequiredForSceneStart(bool required_) { requiredForSceneStart = required_; }
    
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
   
    // 遅延更新。
    void LateUpdate(float et);

    void UpdateTransform()
    {
        transform->UpdateWorldMatrix();
    }

    void Submit(ID3D11DeviceContext* context_)
    {
        // 描画Submitは通常Componentだけを対象にする。
        for (auto& comp : components)
            if (comp->IsEnabled() && comp->IsStarted())
                comp->Submit(context_);
    }

    void TransformGui();
    void Gui();

    // 選択中オブジェクト用ウィンドウ
    void GuiWindow()
    {
        ImGui::Begin(nameComponent->name.c_str());
        Gui(); // 元の GUI 呼び出し
        ImGui::End();
    }

    void ExecuteAction(const std::string& actionName_, std::shared_ptr<GameObject> target_ = nullptr);

    void Clear();
    nlohmann::json Save() const;
    void Load(const nlohmann::json& j);

    void SetNoSaveObject() { saveFlg = false; };
    void SetLayer(LayerType type) { if (layer != type) { layer = type; NotifyComponentCompositionChanged(); } }
    LayerType GetLayer() const { return layer; }
    uint32_t GetLayerBit() const { return static_cast<uint32_t>(layer); }

private:
    // 一個体の構成変更だけを通知し、監視対象の再発見を毎Frameに持ち込まない。
    void NotifyComponentCompositionChanged();
    void RegisterColliderComponent(const std::shared_ptr<ColliderComponent>& collider_);
    void UnregisterColliderComponent(ColliderComponent* collider_);
    void UnregisterAllColliderComponents();
};
