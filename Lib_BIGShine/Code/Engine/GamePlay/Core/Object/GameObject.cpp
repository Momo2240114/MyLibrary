#include "PrecompiledHeader.h"

#include"GameObject.h"
#include"GameObjectManager.h"
#include"Engine/Physics/ColliderComponent.h"

#include "ImGuizmo.h"

#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <cstdio>

namespace
{
    // Component更新時に、優先度と追加順を一緒に扱うための一時構造体。
    struct ComponentUpdateEntry
    {
        std::shared_ptr<Component> component;
        int order = 0;
    };

    void SortUpdateEntries(std::vector<ComponentUpdateEntry>& entries_, bool lateUpdate_)
    {
        // Priorityが同じ場合は追加順を維持し、既存Sceneの挙動が急に変わらないようにする。
        std::stable_sort(entries_.begin(), entries_.end(),
            [lateUpdate_](const ComponentUpdateEntry& left_, const ComponentUpdateEntry& right_)
            {
                const int leftPriority_ = lateUpdate_
                    ? left_.component->GetLateUpdatePriority()
                    : left_.component->GetUpdatePriority();
                const int rightPriority_ = lateUpdate_
                    ? right_.component->GetLateUpdatePriority()
                    : right_.component->GetUpdatePriority();

                if (leftPriority_ != rightPriority_) return leftPriority_ < rightPriority_;
                return left_.order < right_.order;
            });
    }

    void AddGameObjectProfileText(const GameObject& object_)
    {
        // Tracy詳細Profileが有効な時だけ、Object状態をFrame情報として付ける。
#if BS_PROFILE_DETAILED_ENABLED
        char profileText_[256]{};
        const int textLength_ = std::snprintf(
            profileText_,
            sizeof(profileText_),
            "Object: %s\nObjectID: %p\nLayer: %u\nSave: %s\nComponentCount: %zu\nColliderCount: %zu",
            object_.GetName().c_str(),
            static_cast<const void*>(&object_),
            static_cast<unsigned int>(object_.GetLayer()),
            object_.saveFlg ? "true" : "false",
            object_.components.size(),
            object_.colliderComponents.size());
        if (textLength_ > 0)
        {
            BS_PROFILE_TEXT(
                profileText_,
                static_cast<size_t>((std::min)(textLength_, static_cast<int>(sizeof(profileText_) - 1))));
        }
#else
        (void)object_;
#endif
    }
}

void GameObject::SetManager(GameObjectManager* manager_)
{
    // Managerが変わる時は、CollisionSystemへの登録先も付け替える。
    if (manager == manager_) return;

    UnregisterAllColliderComponents();
    manager = manager_;
    for (const std::shared_ptr<Component>& component_ : colliderComponents)
    {
        RegisterColliderComponent(
            std::dynamic_pointer_cast<ColliderComponent>(component_));
    }
}

void GameObject::RegisterColliderComponent(
    const std::shared_ptr<ColliderComponent>& collider_)
{
    if (!manager || !collider_) return;
    manager->GetCollisionStepSystem().RegisterCollider(collider_);
}

void GameObject::UnregisterColliderComponent(ColliderComponent* collider_)
{
    if (!manager || !collider_) return;
    manager->GetCollisionStepSystem().UnregisterCollider(collider_);
}

void GameObject::UnregisterAllColliderComponents()
{
    if (!manager) return;
    for (const std::shared_ptr<Component>& component_ : colliderComponents)
    {
        const std::shared_ptr<ColliderComponent> collider_ =
            std::dynamic_pointer_cast<ColliderComponent>(component_);
        UnregisterColliderComponent(collider_.get());
    }
}

void GameObject::RegisterAction(
    const std::string& category_,
    const std::string& name_,
    const ActionRegistry::ActionFunc& func_)
{
    // Object名付きActionとして登録し、同名Actionが複数Objectで衝突しないようにする。
    localRegistry.RegisterAction(category_, BuildActionName(name_), func_);
}

std::string GameObject::BuildActionName(const std::string& actionName_) const
{
    // 既にObject名Prefixが付いている場合は二重に付けない。
    if (actionName_.empty()) return {};
    const std::string& objectName_ = GetName();
    if (objectName_.empty()) return actionName_;
    const std::string prefix_ = objectName_ + "_";
    if (actionName_.starts_with(prefix_)) return actionName_;
    return prefix_ + actionName_;
}

std::string GameObject::RegisterGlobalAction(
    const std::string& category_,
    const std::string& actionName_,
    const ActionRegistry::ActionFunc& function_)
{
    const std::string resolvedName_ = BuildActionName(actionName_);
    if (!resolvedName_.empty())
    {
        ActionRegistry::Instance().RegisterAction(category_, resolvedName_, function_);
    }
    return resolvedName_;
}

void GameObject::DrawGizmo(const float* view, const float* projection)
{
    // Editor上のTransform操作をImGuizmoで行い、操作結果をWorld行列として反映する。
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    // 操作モードの切り替え（任意でUIから変更できるようにすると便利です）
    static ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
    // MODE を WORLD にすることで、ギズモの矢印は常に世界軸を向きます
    static ImGuizmo::MODE currentMode = ImGuizmo::WORLD;

    // 1. 本物のワールド行列を取得（現在の位置・回転・スケールがすべて入っている）
    DirectX::XMMATRIX worldMat = transform->GetWorldMatrix();

    // ImGuizmoが扱えるように float[16] 形式にする
    DirectX::XMFLOAT4X4 gizmoMatrix;
    DirectX::XMStoreFloat4x4(&gizmoMatrix, worldMat);

    // 2. ギズモの描画と操作
    // gizmoMatrix を直接渡す。内部で「現在の状態」から「ドラッグ後の状態」へ更新される。
    if (ImGuizmo::Manipulate(
        view,
        projection,
        currentOperation,
        currentMode, // ここが WORLD なら、見た目の軸は回転しません
        &gizmoMatrix._11))
    {
        // 操作中の場合のみ反映
        if (ImGuizmo::IsUsing()) {
            // 3. 操作後の「位置・回転・スケールがすべて維持された」行列を読み込む
            DirectX::XMMATRIX newWorldMat = DirectX::XMLoadFloat4x4(&gizmoMatrix);

            // これをそのまま Apply すれば、回転を維持したまま移動だけ（あるいは回転だけ）が更新されます
            transform->ApplyWorldMatrix(newWorldMat);
        }
    }
}

std::vector<std::string> GameObject::GetComponentNames() const
{
    std::vector<std::string> names;
    for (const auto& comp : components)
    {
        names.push_back(comp->GetName());
    }

    for (const auto& collider_ : colliderComponents)
    {
        names.push_back(collider_->GetName());
    }

    return names;
}

void GameObject::CallAllComponentStart()
{
    BS_PROFILE_SCOPE("GameObject::CallAllComponentStart");

    // Colliderを先にStartして、通常Componentが初回処理で物理情報を参照できるようにする。
    for (auto& collider_ : colliderComponents)
    {
        if (!collider_->IsStarted())
        {
            {
                BS_PROFILE_SCOPE("GameObject::StartCollider");
                const std::string profileName_ = collider_->GetName();
                {
                    BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
                    collider_->Start();
                }
            }
            collider_->SetStarted(true);
        }
    }

    // ループ中の AddComponent による破壊を防ぐため、コピーを作成して回す
    auto tempComponents = components;

    for (auto& comp : tempComponents)
    {
        // 1. まず NULL チェック
        if (!comp) continue;

        // 2. メモリが有効かどうかの簡易チェック (MSVCデバッグ時)
        // 本来は shared_ptr なので起きにくいはずですが、
        // 0xdddd... を掴んでいる場合は、中身の shared_count が壊れています。
        try {
            if (!comp->IsStarted())
            {
                {
                    BS_PROFILE_SCOPE("GameObject::StartComponent");
                    const std::string profileName_ = comp->GetName();
                    {
                        BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
                        comp->Start();
                    }
                }
                comp->SetStarted(true);
            }
        }
        catch (...) {
            // ここで止まるなら、comp の指す先が既に壊れています
            assert(false && "Component memory is corrupted!");
        }
    }

}

bool GameObject::IsReadyToStart()
{
    BS_PROFILE_SCOPE("GameObject::IsReadyToStart");
    // Scene開始前に、必要なAssetやRuntime準備が終わっているかComponent単位で確認する。
    for (const std::shared_ptr<Component>& collider_ : colliderComponents)
    {
        if (!collider_ || !collider_->IsEnabled()) continue;
        {
            BS_PROFILE_SCOPE("GameObject::CheckColliderLoadReady");
            const std::string profileName_ = collider_->GetName();
            {
                BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
                collider_->UpdateLoadState();
                if (!collider_->IsLoadReady()) return false;
            }
        }
    }

    for (const std::shared_ptr<Component>& comp_ : components)
    {
        if (!comp_ || !comp_->IsEnabled()) continue;
        {
            BS_PROFILE_SCOPE("GameObject::CheckComponentLoadReady");
            const std::string profileName_ = comp_->GetName();
            {
                BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
                comp_->UpdateLoadState();
                if (!comp_->IsLoadReady()) return false;
            }
        }
    }

    return true;
}

bool GameObject::IsRuntimeReady() const
{
    for (const std::shared_ptr<Component>& collider_ : colliderComponents)
    {
        if (!collider_ || !collider_->IsEnabled()) continue;
        collider_->UpdateLoadState();
        if (!collider_->IsLoadReady() || !collider_->IsRuntimeReady()) return false;
    }

    for (const std::shared_ptr<Component>& comp_ : components)
    {
        if (!comp_ || !comp_->IsEnabled()) continue;
        comp_->UpdateLoadState();
        if (!comp_->IsLoadReady() || !comp_->IsRuntimeReady()) return false;
    }

    return true;
}

bool GameObject::HasRuntimeFailure() const
{
    for (const std::shared_ptr<Component>& collider_ : colliderComponents)
    {
        if (collider_ && collider_->IsEnabled() && collider_->HasRuntimeFailure()) return true;
    }

    for (const std::shared_ptr<Component>& comp_ : components)
    {
        if (comp_ && comp_->IsEnabled() && comp_->HasRuntimeFailure()) return true;
    }

    return false;
}

bool GameObject::IsRequiredForSceneStart() const
{
    if (requiredForSceneStart) return true;

    for (const std::shared_ptr<Component>& collider_ : colliderComponents)
    {
        if (collider_ && collider_->IsEnabled() && collider_->IsRequiredForSceneStart()) return true;
    }

    for (const std::shared_ptr<Component>& comp_ : components)
    {
        if (comp_ && comp_->IsEnabled() && comp_->IsRequiredForSceneStart()) return true;
    }

    return false;
}

std::shared_ptr<Component> GameObject::GetComponentByName(const std::string& compName_) const
{
    for (auto& comp : components)
    {
        if (comp->GetName() == compName_)
            return comp;
    }
    //コリジョンは複数のものをもてるようにしたのであわせて無効化
    //for (auto& collider_ : colliderComponents)
    //{
    //    if (collider_->GetName() == compName_) return collider_;
    //}

    return nullptr;
}


void GameObject::SetParent(std::shared_ptr<GameObject> parentGO)
{
    transform->gameObject = shared_from_this();

    if (parentGO) {
        parentGO->transform->gameObject = parentGO;
        // Transformの親子付け
        transform->SetParent(parentGO->transform);
    }
    else {
        transform->SetParent(nullptr);
    }
}

void GameObject::Update(float et)
{
    BS_PROFILE_SCOPE("GameObject::Update");
    AddGameObjectProfileText(*this);

    int updatedComponentCount_ = 0;

    {
        BS_PROFILE_SCOPE("GameObject::UpdateComponents");

        // 通常Componentを先に更新し、CharacterMovementとColliderは後段で専用順に回す。
        std::vector<ComponentUpdateEntry> entries_;
        for (int order_ = 0; order_ < static_cast<int>(components.size()); ++order_)
        {
            auto& comp_ = components[order_];
            if (!comp_ || !comp_->IsEnabled() || !comp_->IsStarted()) continue;
            if (!comp_->ShouldUpdate()) continue;

            const std::string& name_ = comp_->GetName();

            if (name_ != "PhysicsCollisionComponent" &&
                name_ != "CharacterMovementComponent")
            {
                entries_.push_back({ comp_, order_ });
            }
        }

        SortUpdateEntries(entries_, false);

        for (const ComponentUpdateEntry& entry_ : entries_)
        {
            const std::shared_ptr<Component>& comp_ = entry_.component;
            const std::string& name_ = comp_->GetName();
            BS_PROFILE_SCOPE_DYNAMIC(name_.c_str());
            BS_PROFILE_TEXT(name_.c_str(), name_.size());
            comp_->Update(et);
            ++updatedComponentCount_;
        }
    }

    {
        BS_PROFILE_SCOPE("GameObject::CharacterMovementUpdate");

        // CharacterMovementは入力やAIが速度を決めた後にまとめて反映する。
        std::vector<ComponentUpdateEntry> entries_;
        for (int order_ = 0; order_ < static_cast<int>(components.size()); ++order_)
        {
            auto& comp_ = components[order_];
            if (!comp_ || !comp_->IsEnabled() || !comp_->IsStarted()) continue;
            if (!comp_->ShouldUpdate()) continue;

            const std::string& name_ = comp_->GetName();

            if (name_ == "CharacterMovementComponent")
            {
                entries_.push_back({ comp_, order_ });
            }
        }

        SortUpdateEntries(entries_, false);

        for (const ComponentUpdateEntry& entry_ : entries_)
        {
            const std::shared_ptr<Component>& comp_ = entry_.component;
            const std::string& name_ = comp_->GetName();
            BS_PROFILE_SCOPE("CharacterMovementComponent::Update");
            BS_PROFILE_TEXT(name_.c_str(), name_.size());
            comp_->Update(et);
            ++updatedComponentCount_;
        }
    }

    {
        BS_PROFILE_SCOPE("GameObject::ColliderUpdate");

        // Collider系は専用リストから更新し、CollisionSystemとの登録状態を保つ。
        std::vector<ComponentUpdateEntry> entries_;
        for (int order_ = 0; order_ < static_cast<int>(colliderComponents.size()); ++order_)
        {
            auto& collider_ = colliderComponents[order_];
            if (!collider_ || !collider_->IsEnabled() || !collider_->IsStarted()) continue;
            if (!collider_->ShouldUpdate()) continue;

            entries_.push_back({ collider_, order_ });
        }

        SortUpdateEntries(entries_, false);

        for (const ComponentUpdateEntry& entry_ : entries_)
        {
            const std::shared_ptr<Component>& collider_ = entry_.component;
            const std::string& name_ = collider_->GetName();
            BS_PROFILE_SCOPE("ColliderComponent::Update");
            BS_PROFILE_TEXT(name_.c_str(), name_.size());
            collider_->Update(et);
            ++updatedComponentCount_;
        }
    }

    BS_PROFILE_PLOT("Component.UpdatedCount", updatedComponentCount_);
}

void GameObject::LateUpdate(float et)
{
    BS_PROFILE_SCOPE("GameObject::LateUpdate");
    AddGameObjectProfileText(*this);

    int lateUpdatedComponentCount_ = 0;

    {
        BS_PROFILE_SCOPE("GameObject::LateUpdateComponents");

        // 通常LateUpdateを先に回し、PhysicsCollisionComponentは後段へ分ける。
        std::vector<ComponentUpdateEntry> entries_;
        for (int order_ = 0; order_ < static_cast<int>(components.size()); ++order_)
        {
            auto& comp_ = components[order_];
            if (!comp_ || !comp_->IsEnabled() || !comp_->IsStarted()) continue;
            if (!comp_->ShouldLateUpdate()) continue;

            const std::string& name_ = comp_->GetName();

            if (name_ != "PhysicsCollisionComponent")
            {
                entries_.push_back({ comp_, order_ });
            }
        }

        SortUpdateEntries(entries_, true);

        for (const ComponentUpdateEntry& entry_ : entries_)
        {
            const std::shared_ptr<Component>& comp_ = entry_.component;
            const std::string& name_ = comp_->GetName();
            BS_PROFILE_SCOPE_DYNAMIC(name_.c_str());
            BS_PROFILE_TEXT(name_.c_str(), name_.size());
            comp_->LateUpdate(et);
            ++lateUpdatedComponentCount_;
        }
    }

    {
        BS_PROFILE_SCOPE("GameObject::PhysicsLateUpdate");

        // PhysicsCollisionComponentはTransform更新後の状態でLateUpdateする。
        std::vector<ComponentUpdateEntry> entries_;
        for (int order_ = 0; order_ < static_cast<int>(components.size()); ++order_)
        {
            auto& comp_ = components[order_];
            if (!comp_ || !comp_->IsEnabled() || !comp_->IsStarted()) continue;
            if (!comp_->ShouldLateUpdate()) continue;

            const std::string& name_ = comp_->GetName();

            if (name_ == "PhysicsCollisionComponent")
            {
                entries_.push_back({ comp_, order_ });
            }
        }

        SortUpdateEntries(entries_, true);

        for (const ComponentUpdateEntry& entry_ : entries_)
        {
            const std::shared_ptr<Component>& comp_ = entry_.component;
            const std::string& name_ = comp_->GetName();
            BS_PROFILE_SCOPE("PhysicsCollisionComponent::LateUpdate");
            BS_PROFILE_TEXT(name_.c_str(), name_.size());
            comp_->LateUpdate(et);
            ++lateUpdatedComponentCount_;
        }
    }

    BS_PROFILE_PLOT("Component.LateUpdatedCount", lateUpdatedComponentCount_);
}

void GameObject::ExecuteAction(
    const std::string& actionName_,
    std::shared_ptr<GameObject> target_)
{
    // 対象Object、親、子、Globalの順でActionを探し、Animation通知を適切なObjectへ届ける。
    const std::shared_ptr<GameObject> startObject_ =
        target_ ? target_ : shared_from_this();
    if (!startObject_ || actionName_.empty()) return;

    auto callLocal_ = [&actionName_](const std::shared_ptr<GameObject>& object_)
    {
        if (!object_) return false;

        const std::string resolvedActionName_ = object_->BuildActionName(actionName_);
        if (object_->localRegistry.HasAction(resolvedActionName_))
        {
            object_->localRegistry.Call(resolvedActionName_);
            return true;
        }
        if (!object_->localRegistry.HasAction(actionName_)) return false;
        object_->localRegistry.Call(actionName_);
        return true;
    };

    if (callLocal_(startObject_)) return;

    std::shared_ptr<Transform> parentTransform_ =
        startObject_->transform ? startObject_->transform->parent.lock() : nullptr;
    while (parentTransform_)
    {
        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;
        if (callLocal_(parentObject_)) return;
        parentTransform_ = parentObject_->transform
            ? parentObject_->transform->parent.lock()
            : nullptr;
    }

    auto callChildren_ = [&](auto&& self_, const std::shared_ptr<GameObject>& object_) -> bool
    {
        if (!object_ || !object_->transform) return false;
        for (const std::shared_ptr<Transform>& childTransform_ : object_->transform->children)
        {
            if (!childTransform_) continue;
            const std::shared_ptr<GameObject> childObject_ =
                childTransform_->gameObject.lock();
            if (callLocal_(childObject_) || self_(self_, childObject_)) return true;
        }
        return false;
    };
    if (callChildren_(callChildren_, startObject_)) return;

    ActionRegistry::Instance().Call(actionName_);
}


void GameObject::Clear()
{
    // Scene再読み込み前にComponentとCollider登録を外し、Transformを初期状態へ戻す。
    UnregisterAllColliderComponents();
    // 1. 各コンポーネントの参照を外す前に、必要なら終了処理を呼ぶ
    // (コンポーネント側に終了用関数があればここで呼ぶ)

    // 2. 汎用コンポーネントリストをクリア
    // shared_ptr なので、他で参照されていなければ各コンポーネントのデストラクタが走ります
    components.clear();

    // 3. 特別扱いしているコライダーコンポーネントをクリア
    colliderComponents.clear();

    // 4. Transform のリセット（必要に応じて）
    // 通常は次の Load で上書きされますが、初期状態に戻すと安全です
    if (transform) {
        transform->localPosition = { 0, 0, 0 };
        transform->localRotation = { 0, 0, 0 };
        transform->localScale = { 1, 1, 1 };
        transform->UpdateWorldMatrix();
    }
}

void GameObject::TransformGui()
{
}

void GameObject::Gui()
{
    // Inspector用UI。Object名、Layer、Transform、Component追加/削除をここで操作する。
    // --- 名前編集 UI ---
    if (nameComponent)
    {
        // 編集用に一時的なバッファを用意
        char nameBuf[128];
        strncpy_s(nameBuf, nameComponent->name.c_str(), sizeof(nameBuf));

        if (ImGui::InputText(IM_U8("名前##GameObject_NameInput"), nameBuf, sizeof(nameBuf)))
        {
            // 名前が変更されたらNameComponentに反映
            nameComponent->name = std::string(nameBuf);
        }
        ImGui::Separator();
    }

    // -----------------------------
    // Add Component
    // -----------------------------
    if (ImGui::Button(IM_U8("コンポーネント追加##GameObject_AddComponentButton"), ImVec2(-1, 0))) // 横幅いっぱいのボタン
        ImGui::OpenPopup("AddComponentPopup");

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        // 検索窓用のバッファ
        static char searchFilter[64] = "";

        // ポップアップが開いた初回のみ検索窓にフォーカスを当てる
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            memset(searchFilter, 0, sizeof(searchFilter)); // 開くたびにリセット
        }

        ImGui::InputText("##GameObject_AddComponentSearchInput", searchFilter, IM_ARRAYSIZE(searchFilter));
        ImGui::Separator();

        // 検索文字列を小文字に変換して比較するとより使いやすくなりますが、
        // ここではシンプルに部分一致(strstr)で実装します。
        std::string filter(searchFilter);

        for (auto& [name, _] : ComponentRegistry::Instance().GetAll())
        {
            // 検索フィルタによる絞り込み（空文字の場合は常にtrue）
            if (!filter.empty()) {
                // 名前の中に検索語が含まれているか（大文字小文字を区別する簡易版）
                // より高度にするなら std::search とかで見やすく変換してください
                if (name.find(filter) == std::string::npos) {
                    continue;
                }
            }

            bool has = (GetComponentByName(name) != nullptr);

            if (ImGui::MenuItem(name.c_str(), nullptr, false, !has))
            {
                ComponentRegistry::Instance().Create(name, shared_from_this());
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    // 表示用ラベル
    const char* layerNames[] = {
        IM_U8("デフォルト"), IM_U8("プレイヤー"), IM_U8("敵"), IM_U8("プレイヤー弾"),
        IM_U8("敵弾"), IM_U8("UI"), IM_U8("環境"), IM_U8("スニーク対象"), IM_U8("地形"), IM_U8("壁"), IM_U8("アイテム")
    };

    // 対応するEnum値を配列化しておく
    const LayerType layerValues[] = {
        LayerType::Default, LayerType::Player, LayerType::Enemy, LayerType::PlayerBullet,
        LayerType::EnemyBullet, LayerType::UI, LayerType::Environment, LayerType::SneakObject, LayerType::Terrain, LayerType::Wall,LayerType::Item,
    };

    // 現在の値がどのインデックスに該当するか探す
    int currentItem = 0;
    for (int i = 0; i < IM_ARRAYSIZE(layerValues); i++) {
        if (layer == layerValues[i]) {
            currentItem = i;
            break;
        }
    }

    // コンボボックスを表示
    if (ImGui::Combo(IM_U8("レイヤー##GameObject_LayerCombo"), &currentItem, layerNames, IM_ARRAYSIZE(layerNames)))
    {
        // 選択されたインデックスに対応するEnum値を代入
        layer = layerValues[currentItem];
    }

    ImGui::Separator();

    ImGui::SeparatorText(IM_U8("トランスフォーム##GameObject_TransformHeader"));
    transform->Gui();
    ImGui::TextDisabled(IM_U8("ゲーム制御のコライダーはこの値に追従します。物理シミュレーション対象だけ Physics Controls Transform を有効にしてください。"));

    // --- 親子付け UI の追加 ---
    ImGui::Separator();
    ImGui::Text(IM_U8("階層"));

    // 現在の親の名前を取得
    std::string currentParentName = "None (Root)";
    if (auto pTrans = transform->parent.lock()) {
        if (auto pGO = pTrans->gameObject.lock()) {
            currentParentName = pGO->GetName();
        }
    }

    if (ImGui::BeginCombo(IM_U8("親##GameObject_ParentCombo"), currentParentName.c_str()))
    {
        // 親を解除するオプション
        if (ImGui::Selectable(IM_U8("なし（ルート）##GameObject_NoneRootSelectable"), currentParentName == "None (Root)")) {
            // 親を解除する処理（Transformに解除関数を作ると良い）
            transform->SetParent(nullptr);
        }

        // シーン上の全オブジェクトをリストアップ（自分自身と自分の子は除外すべき）
        // GameObjectManager経由で全オブジェクトにアクセス
        auto& allObjects = manager->GetObjects();
        for (auto& obj : allObjects)
        {
            // 自分自身には親子付けできない
            if (obj->shared_from_this() == shared_from_this()) continue;

            bool isSelected = (currentParentName == obj->GetName());
            if (ImGui::Selectable(obj->GetName().c_str(), isSelected))
            {
                SetParent(obj); // GameObject::SetParent を呼び出す

                manager->ResolveHierarchy();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();



    // 削除予約用のポインタ
    Component* componentToRemove = nullptr;
    Component* colliderToRemove = nullptr;

    // -----------------------------
    // Components (可変リスト)
    // -----------------------------
    for (auto& comp : components)
    {
        ImGui::PushID(comp.get());

        // ON / OFF チェックボックス
        bool enabled = comp->IsEnabled();
        if (ImGui::Checkbox("##GameObject_ComponentEnabledCheckbox", &enabled)) comp->SetEnabled(enabled);

        ImGui::SameLine();

        if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);

        // ヘッダー表示
        bool open = ImGui::CollapsingHeader(comp->GetName().c_str());

        // ★ コンテキストメニュー（ヘッダーを右クリック）
        if (ImGui::BeginPopupContextItem("ComponentContextMenu"))
        {
            if (ImGui::MenuItem(IM_U8("コンポーネントを削除##GameObject_RemoveComponentMenuItem")))
            {
                componentToRemove = comp.get();
            }
            ImGui::EndPopup();
        }

        if (!enabled) ImGui::PopStyleVar();

        if (open) comp->DrawGuiProfiled();

        ImGui::PopID();
    }

    // -----------------------------
    // Collider Component (個別メンバ)
    // -----------------------------
    for (auto& collider_ : colliderComponents)
    {
        ImGui::PushID(collider_.get());

        bool enabled = collider_->IsEnabled();
        if (ImGui::Checkbox("##GameObject_ColliderEnabledCheckbox", &enabled)) collider_->SetEnabled(enabled);

        ImGui::SameLine();

        if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);

        std::string colliderHeader_ = collider_->GetName();
        const auto colliderComponent_ = std::dynamic_pointer_cast<ColliderComponent>(collider_);
        if (colliderComponent_ && !colliderComponent_->GetColliderName().empty())
        {
            colliderHeader_ += " - ";
            colliderHeader_ += colliderComponent_->GetColliderName();
        }
        bool open = ImGui::CollapsingHeader(colliderHeader_.c_str());

        // ★ コンテキストメニュー
        if (ImGui::BeginPopupContextItem("ColliderContextMenu"))
        {
            if (ImGui::MenuItem(IM_U8("コライダーコンポーネントを削除##GameObject_RemoveColliderComponentMenuItem")))
            {
                colliderToRemove = collider_.get();
            }
            ImGui::EndPopup();
        }

        if (!enabled) ImGui::PopStyleVar();

        if (open) collider_->DrawGuiProfiled();

        ImGui::PopID();
    }

    // === 実際の削除処理（ループの外で行う） ===

    // 1. リストからの削除
    if (componentToRemove)
    {
        components.erase(std::remove_if(components.begin(), components.end(),
            [&](const auto& comp) { return comp.get() == componentToRemove; }),
            components.end());
    }

    // 2. 個別メンバの削除
    if (colliderToRemove)
    {
        UnregisterColliderComponent(
            dynamic_cast<ColliderComponent*>(colliderToRemove));
        colliderComponents.erase(
            std::remove_if(colliderComponents.begin(), colliderComponents.end(),
                [&](const auto& collider_) { return collider_.get() == colliderToRemove; }),
            colliderComponents.end());
    }
}

nlohmann::json GameObject::Save() const
{
    // Object基本情報、Transform、通常Component、ColliderComponentをJSONへまとめる。
    nlohmann::json j;

    // 1. 基本情報の保存
    j["Name"] = nameComponent->name;
    j["Layer"] = static_cast<uint32_t>(layer);
    j["CollisionMask"] = mask;
    j["RequiredForSceneStart"] = requiredForSceneStart;

    // 親のTransformがある場合、その親が属するGameObjectの名前を保存
    if (auto pTrans = transform->parent.lock()) {
        if (auto pGO = pTrans->gameObject.lock()) {
            j["ParentName"] = pGO->GetName();
        }
    }
    else {
        j["ParentName"] = "";
    }

    // 2. Transformの保存（Serializerクラスのヘルパーを使うと綺麗です）
    j["Transform"]["Pos"] = { transform->localPosition.x, transform->localPosition.y, transform->localPosition.z };
    j["Transform"]["Rot"] = { transform->localRotation.x, transform->localRotation.y, transform->localRotation.z };
    j["Transform"]["Scale"] = { transform->localScale.x, transform->localScale.y, transform->localScale.z };

    // 3. 全コンポーネントを保存
    for (auto it = components.begin(); it != components.end(); ++it) {
        auto& comp = *it;
        nlohmann::json cj;
        cj["TypeName"] = comp->GetName();
        cj["Enabled"] = comp->IsEnabled();
        cj["RequiredForSceneStart"] = comp->IsRequiredForSceneStart();
        cj["UpdateEnabled"] = comp->ShouldUpdate();
        cj["LateUpdateEnabled"] = comp->ShouldLateUpdate();
        cj["UpdatePriority"] = comp->GetUpdatePriority();
        cj["LateUpdatePriority"] = comp->GetLateUpdatePriority();

        // 各コンポーネント固有の変数を保存
        nlohmann::json data;
        comp->Serialize(data);
        cj["Data"] = data;

        j["Components"].push_back(cj);
    }

    // 4. 特別扱いしているコライダーの保存
    for (const auto& collider_ : colliderComponents) {
        nlohmann::json cj;
        cj["TypeName"] = collider_->GetName();
        cj["Enabled"] = collider_->IsEnabled();
        cj["RequiredForSceneStart"] = collider_->IsRequiredForSceneStart();
        cj["UpdateEnabled"] = collider_->ShouldUpdate();
        cj["LateUpdateEnabled"] = collider_->ShouldLateUpdate();
        cj["UpdatePriority"] = collider_->GetUpdatePriority();
        cj["LateUpdatePriority"] = collider_->GetLateUpdatePriority();

        nlohmann::json data;
        collider_->Serialize(data);
        cj["Data"] = data;

        j["ColliderComponents"].push_back(cj);
    }

    return j;
}


void GameObject::Load(const nlohmann::json& j)
{
    BS_PROFILE_SCOPE("GameObject::Load");

    // JSONからObject基本情報とComponent構成を復元する。
    // 1. 基本情報の復元
    nameComponent->name = j.value("Name", "GameObject");
    layer = static_cast<LayerType>(j.value("Layer", 0));
    mask = j.value("CollisionMask", static_cast<uint32_t>(LayerType::Default));
    requiredForSceneStart = j.value("RequiredForSceneStart", false);

    // 所有者の設定を忘れないように
    transform->gameObject = shared_from_this();

    // 親の名前を一時保存
    if (j.contains("Transform"))
    {
        transform->parentNameDuringLoad = j.value("ParentName", "");
    }
    // 2. Transformの復元
    if (j.contains("Transform")) {
        auto& t = j["Transform"];
        transform->localPosition = { t["Pos"][0], t["Pos"][1], t["Pos"][2] };
        transform->localRotation = { t["Rot"][0], t["Rot"][1], t["Rot"][2] };
        transform->localScale = { t["Scale"][0], t["Scale"][1], t["Scale"][2] };
    }
    transform->UpdateWorldMatrix(); // 先に行列を確定させる

    // 3. コンポーネントの形成（復元）
// コンポーネントの復元 (通常のComponentsもColliderComponentも共通の処理へ)
    auto loadComponent = [&](const nlohmann::json& cJson, bool allowDuplicate_) {
        std::string typeName = cJson["TypeName"];
        // 1. インスタンス生成
        auto comp = ComponentRegistry::Instance().Create(typeName, shared_from_this(), false, allowDuplicate_);

        printf("Loading component: %s\n", typeName.c_str());
        if (comp) printf("Instance created successfully.\n");

        if (comp) {
            // 2. 基本設定とデータの復元
            comp->SetEnabled(cJson.value("Enabled", true));
            comp->SetRequiredForSceneStart(
                cJson.value("RequiredForSceneStart", comp->IsRequiredForSceneStart()));
            comp->SetUpdateEnabled(cJson.value("UpdateEnabled", comp->ShouldUpdate()));
            comp->SetLateUpdateEnabled(cJson.value("LateUpdateEnabled", comp->ShouldLateUpdate()));
            comp->SetUpdatePriority(cJson.value("UpdatePriority", comp->GetUpdatePriority()));
            comp->SetLateUpdatePriority(cJson.value("LateUpdatePriority", comp->GetLateUpdatePriority()));
            comp->DeserializeProfiled(cJson["Data"]);

            // 3. 型を判別して適切な場所に格納 (AddComponentのロジックと合わせる)
            // Register経由だと型Tが不明なため、dynamic_pointer_cast で判定します
            // <ColliderComponent> を指定してキャスト
            auto col = std::dynamic_pointer_cast<ColliderComponent>(comp);
            if (col) {
                const auto exists_ = std::find(colliderComponents.begin(), colliderComponents.end(), comp);
                if (exists_ == colliderComponents.end()) colliderComponents.push_back(col);
            }
        }
        };

    // 通常のコンポーネント配列の処理
    if (j.contains("Components")) {
        for (const auto& cJson : j["Components"]) {
              loadComponent(cJson, false);
        }
    }

    // 特別なコライダー枠（もしJSONに分かれて残っているなら）の処理
    if (j.contains("ColliderComponent")) {
        loadComponent(j["ColliderComponent"], false);
    }
    if (j.contains("ColliderComponents")) {
        for (const auto& colliderJson_ : j["ColliderComponents"]) {
            loadComponent(colliderJson_, true);
        }
    }

}
