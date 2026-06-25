#include"GameObject.h"
#include"GameObjectManager.h"
#include"Engine/Physics/ColliderComponent.h"

#include "ImGuizmo.h"


void GameObject::SetManager(GameObjectManager* manager_)
{
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
    localRegistry.RegisterAction(category_, name_, func_);
}

void GameObject::DrawGizmo(const float* view, const float* projection)
{
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

    for (auto& collider_ : colliderComponents)
    {
        if (!collider_->IsStarted())
        {
            collider_->Start();
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
                comp->Start();
                comp->SetStarted(true);
            }
        }
        catch (...) {
            // ここで止まるなら、comp の指す先が既に壊れています
            assert(false && "Component memory is corrupted!");
        }
    }

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
    // 1. まず「物理以外」のコンポーネントを更新（PlayerInput, FollowCamera など）
    // ここで移動の意思（Offset）が決まる
    for (auto& comp : components)
    {
        if (!comp->IsEnabled()) continue;
        if (comp->GetName() != "PhysicsCollisionComponent" &&
            comp->GetName() != "CharacterMovementComponent")
            comp->Update(et);
    }

    // Movement consumes input, RootMotion, and impulses after every producer updated.
    for (auto& comp : components)
    {
        if (!comp->IsEnabled()) continue;
        if (comp->GetName() == "CharacterMovementComponent")
            comp->Update(et);
    }


    for (auto& collider_ : colliderComponents)
    {
        if (collider_->IsEnabled()) collider_->Update(et);
    }
}

void GameObject::LateUpdate(float et)
{
    for (auto& comp : components)
    {
        if (!comp->IsEnabled()) continue;
        if (comp->GetName() != "PhysicsCollisionComponent")
            comp->LateUpdate(et);
    }

    // 2. 次に「物理」コンポーネントを更新
    // 溜まった Offset を PhysX に伝え、シミュレーション結果を Transform に書き戻す
    for (auto& comp : components)
    {
        if (!comp->IsEnabled()) continue;
        if (comp->GetName() == "PhysicsCollisionComponent")
            comp->LateUpdate(et);
    }
}

void GameObject::ExecuteAction(
    const std::string& actionName_,
    std::shared_ptr<GameObject> target_)
{
    const std::shared_ptr<GameObject> startObject_ =
        target_ ? target_ : shared_from_this();
    if (!startObject_ || actionName_.empty()) return;

    auto callLocal_ = [&actionName_](const std::shared_ptr<GameObject>& object_)
    {
        if (!object_ || !object_->localRegistry.HasAction(actionName_)) return false;
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
    // --- 名前編集 UI ---
    if (nameComponent)
    {
        // 編集用に一時的なバッファを用意
        char nameBuf[128];
        strncpy_s(nameBuf, nameComponent->name.c_str(), sizeof(nameBuf));

        if (ImGui::InputText(IM_U8("名前"), nameBuf, sizeof(nameBuf)))
        {
            // 名前が変更されたらNameComponentに反映
            nameComponent->name = std::string(nameBuf);
        }
        ImGui::Separator();
    }

    // -----------------------------
    // Add Component
    // -----------------------------
    if (ImGui::Button("Add Component", ImVec2(-1, 0))) // 横幅いっぱいのボタン
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

        ImGui::InputText("##Search", searchFilter, IM_ARRAYSIZE(searchFilter));
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
        "Default", "Player", "Enemy", "PlayerBullet",
        "EnemyBullet", "UI", "Environment", "SneakObject", "Terrain", "Wall","Item"
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
    if (ImGui::Combo("Layer", &currentItem, layerNames, IM_ARRAYSIZE(layerNames)))
    {
        // 選択されたインデックスに対応するEnum値を代入
        layer = layerValues[currentItem];
    }

    ImGui::Separator();

    ImGui::SeparatorText("Transform");
    transform->Gui();
    ImGui::TextDisabled("Game-controlled colliders follow these values. Enable Physics Controls Transform only for simulated objects.");

    // --- 親子付け UI の追加 ---
    ImGui::Separator();
    ImGui::Text("Hierarchy");

    // 現在の親の名前を取得
    std::string currentParentName = "None (Root)";
    if (auto pTrans = transform->parent.lock()) {
        if (auto pGO = pTrans->gameObject.lock()) {
            currentParentName = pGO->GetName();
        }
    }

    if (ImGui::BeginCombo("Parent", currentParentName.c_str()))
    {
        // 親を解除するオプション
        if (ImGui::Selectable("None (Root)", currentParentName == "None (Root)")) {
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
        if (ImGui::Checkbox("##Enabled", &enabled)) comp->SetEnabled(enabled);

        ImGui::SameLine();

        if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);

        // ヘッダー表示
        bool open = ImGui::CollapsingHeader(comp->GetName().c_str());

        // ★ コンテキストメニュー（ヘッダーを右クリック）
        if (ImGui::BeginPopupContextItem("ComponentContextMenu"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                componentToRemove = comp.get();
            }
            ImGui::EndPopup();
        }

        if (!enabled) ImGui::PopStyleVar();

        if (open) comp->Gui();

        ImGui::PopID();
    }

    // -----------------------------
    // Collider Component (個別メンバ)
    // -----------------------------
    for (auto& collider_ : colliderComponents)
    {
        ImGui::PushID(collider_.get());

        bool enabled = collider_->IsEnabled();
        if (ImGui::Checkbox("##Enabled", &enabled)) collider_->SetEnabled(enabled);

        ImGui::SameLine();

        if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);

        bool open = ImGui::CollapsingHeader(collider_->GetName().c_str());

        // ★ コンテキストメニュー
        if (ImGui::BeginPopupContextItem("ColliderContextMenu"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                colliderToRemove = collider_.get();
            }
            ImGui::EndPopup();
        }

        if (!enabled) ImGui::PopStyleVar();

        if (open) collider_->Gui();

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
    nlohmann::json j;

    // 1. 基本情報の保存
    j["Name"] = nameComponent->name;
    j["Layer"] = static_cast<uint32_t>(layer);
    j["CollisionMask"] = mask;

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

        nlohmann::json data;
        collider_->Serialize(data);
        cj["Data"] = data;

        j["ColliderComponents"].push_back(cj);
    }

    return j;
}


void GameObject::Load(const nlohmann::json& j)
{
    // 1. 基本情報の復元
    nameComponent->name = j.value("Name", "GameObject");
    layer = static_cast<LayerType>(j.value("Layer", 0));
    mask = j.value("CollisionMask", static_cast<uint32_t>(LayerType::Default));

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
    auto loadComponent = [&](const nlohmann::json& cJson) {
        std::string typeName = cJson["TypeName"];
        // 1. インスタンス生成
        auto comp = ComponentRegistry::Instance().Create(typeName, shared_from_this(), false);

        printf("Loading component: %s\n", typeName.c_str());
        if (comp) printf("Instance created successfully.\n");

        if (comp) {
            // 2. 基本設定とデータの復元
            comp->SetEnabled(cJson.value("Enabled", true));
            comp->Deserialize(cJson["Data"]);

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
            loadComponent(cJson);
        }
    }

    // 特別なコライダー枠（もしJSONに分かれて残っているなら）の処理
    if (j.contains("ColliderComponent")) {
        loadComponent(j["ColliderComponent"]);
    }
    if (j.contains("ColliderComponents")) {
        for (const auto& colliderJson_ : j["ColliderComponents"]) {
            loadComponent(colliderJson_);
        }
    }

}
