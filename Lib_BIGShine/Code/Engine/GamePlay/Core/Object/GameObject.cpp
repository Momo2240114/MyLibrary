#include"GameObject.h"
#include"GameObjectManager.h"
#include"Engine/Physics/ColliderComponent.h"

#include "ImGuizmo.h"


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

    //コライダーコンポーネント
    if (m_pColliderComponent != nullptr)
    {
        names.push_back(m_pColliderComponent->GetName());
    }

    return names;
}

void GameObject::CallAllComponentStart()
{

    if (m_pColliderComponent)
    {
        if (!m_pColliderComponent->IsStarted())
        {
            m_pColliderComponent->Start();
            m_pColliderComponent->SetStarted(true);
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

    if (m_pColliderComponent)
    {
        if (m_pColliderComponent->GetName() == compName_)
            return m_pColliderComponent;
    }

    return nullptr;
}


void GameObject::SetParent(std::shared_ptr<GameObject> parentGO)
{
    if (parentGO) {
        // Transformの親子付け
        transform->SetParent(parentGO->transform);
        // 重要：親側のTransformに自分（のGameObject）を教える必要はないが
        // 自分のTransformは自分のGameObjectを知っておく必要がある
        transform->gameObject = shared_from_this();
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
        if (comp->GetName() != "PhysicsCollisionComponent")
            comp->Update(et);
    }


    //コライダーのコンポーネントを更新
    if (m_pColliderComponent != nullptr)
    {
        m_pColliderComponent->Update(et);
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

void GameObject::ExecuteAction(const std::string& actionName, std::shared_ptr<GameObject> target)
{
    if (target) {
        target->localRegistry.Call(actionName);
    }
    else {
        localRegistry.Call(actionName);
    }
}


void GameObject::Clear()
{
    // 1. 各コンポーネントの参照を外す前に、必要なら終了処理を呼ぶ
    // (コンポーネント側に終了用関数があればここで呼ぶ)

    // 2. 汎用コンポーネントリストをクリア
    // shared_ptr なので、他で参照されていなければ各コンポーネントのデストラクタが走ります
    components.clear();

    // 3. 特別扱いしているコライダーコンポーネントをクリア
    m_pColliderComponent.reset();

    // 4. Transform のリセット（必要に応じて）
    // 通常は次の Load で上書きされますが、初期状態に戻すと安全です
    if (transform) {
        transform->localPosition = { 0, 0, 0 };
        transform->localRotation = { 0, 0, 0 };
        transform->localScale = { 1, 1, 1 };
        transform->UpdateWorldMatrix();
    }
}

void GameObject::Gui()
{
    // -----------------------------
    // Add Component
    // -----------------------------
    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponentPopup");

    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        for (auto& [name, _] : ComponentRegistry::Instance().GetAll())
        {
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

    transform->Gui();

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
        auto& allObjects = GameObjectManager::Instance().GetObjects();
        for (auto& obj : allObjects)
        {
            // 自分自身には親子付けできない
            if (obj->shared_from_this() == shared_from_this()) continue;

            bool isSelected = (currentParentName == obj->GetName());
            if (ImGui::Selectable(obj->GetName().c_str(), isSelected))
            {
                SetParent(obj); // GameObject::SetParent を呼び出す

                GameObjectManager::Instance().ResolveHierarchy();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();



    // 削除予約用のポインタ
    Component* componentToRemove = nullptr;
    bool removeCollider = false;

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

        if (open && enabled) comp->Gui();

        ImGui::PopID();
    }

    // -----------------------------
    // Collider Component (個別メンバ)
    // -----------------------------
    if (m_pColliderComponent != nullptr)
    {
        ImGui::PushID(m_pColliderComponent.get());

        bool enabled = m_pColliderComponent->IsEnabled();
        if (ImGui::Checkbox("##Enabled", &enabled)) m_pColliderComponent->SetEnabled(enabled);

        ImGui::SameLine();

        if (!enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);

        bool open = ImGui::CollapsingHeader(m_pColliderComponent->GetName().c_str());

        // ★ コンテキストメニュー
        if (ImGui::BeginPopupContextItem("ColliderContextMenu"))
        {
            if (ImGui::MenuItem("Remove Component"))
            {
                removeCollider = true;
            }
            ImGui::EndPopup();
        }

        if (!enabled) ImGui::PopStyleVar();

        if (open && enabled) m_pColliderComponent->Gui();

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
    if (removeCollider)
    {
        m_pColliderComponent.reset(); // shared_ptr なら reset() で解放
    }
}

nlohmann::json GameObject::Save() const
{
    nlohmann::json j;

    // 1. 基本情報の保存
    j["Name"] = name;
    j["Layer"] = static_cast<uint32_t>(layer);

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
    if (m_pColliderComponent) {
        nlohmann::json cj;
        cj["TypeName"] = m_pColliderComponent->GetName();
        cj["Enabled"] = m_pColliderComponent->IsEnabled();

        nlohmann::json data;
        m_pColliderComponent->Serialize(data);
        cj["Data"] = data;

        j["ColliderComponent"] = cj;
    }

    return j;
}


void GameObject::Load(const nlohmann::json& j)
{
    // 1. 基本情報の復元
    name = j.value("Name", "GameObject");
    layer = static_cast<LayerType>(j.value("Layer", 0));

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

        if (comp) {
            // 2. 基本設定とデータの復元
            comp->SetEnabled(cJson.value("Enabled", true));
            comp->Deserialize(cJson["Data"]);

            // 3. 型を判別して適切な場所に格納 (AddComponentのロジックと合わせる)
            // Register経由だと型Tが不明なため、dynamic_pointer_cast で判定します
            // <ColliderComponent> を指定してキャスト
            auto col = std::dynamic_pointer_cast<ColliderComponent>(comp);
            if (col) {
                m_pColliderComponent = col;
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

}