#include "GameObjectManager.h"
#include "GraphicsDeviceManager.h"
#include "ColliderComponent.h"
#include "Collision.h"
#include "BoxCollider.h"
#include <PxPhysicsAPI.h>

#include "DecalComponent.h"
#include"ImGuizmo.h"
#include <nlohmann/json.hpp>
#include"TransformDirector.h"
#include"CameraDirectorComponent.h"
#include"ActionSequence.h"

#include"ColorFilterDirector.h"

#include <fstream>
#include"EventManager.h"

void GameObjectManager::Initialize()
{
    // パーティクルシステムの初期化
    particleSystem = std::make_unique<ParticleRenderSystem>();
    particleSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());

    m_pRayCastManager = std::make_unique<RayCastManager>();
}

void GameObjectManager::Update(float et_)
{
    // ★ 1. 新しく追加されたオブジェクトを一度だけ初期化 (StartObjects)
    if (!pendingStarts.empty()) {
        std::vector<std::shared_ptr<GameObject>> startList = std::move(pendingStarts);
        pendingStarts.clear();

        // STEP 1: Director系（アクションを提供する側）の登録
        for (auto& obj : startList) {
            if (!obj) continue;

            // --- ここでパーティクルコンポーネントをリストに登録 ---
            if (auto pComp = obj->GetComponent<ParticleComponent>()) {
                m_particleComponents.push_back(pComp);
            }

            // 既存のコンポーネント初期化（もしあれば）
            obj->CallAllComponentStart();

            // Director系の登録を優先
            if (auto dir = obj->GetComponent<TransformDirector>()) dir->RegisterActions();
            if (auto dir = obj->GetComponent<ColorFilterDirector>()) dir->RegisterActions();
            if (auto dir = obj->GetComponent<CameraDirector>()) dir->RegisterActions();
        }

        // STEP 2: Sequence系（アクションを利用する側）の紐付け
        // ※ 既に全オブジェクトのDirectorが登録済みなので「空振り」が防げる
        for (auto& obj : startList) {
            if (!obj) continue;
            if (auto seq = obj->GetComponent<ActionSequence>()) {
                seq->RegisterSequence("");
            }
        }
    }


    // 1. 速度の更新（入力 ＋ 重力）
    for (auto& obj : objects) {
        //if (auto col = obj->GetComponent<ColliderComponent>()) {
        //    col->ApplyGravity(et_); // ここで velocity.y を更新し、仮移動(pos += v*dt)を行う
        //}
        obj->Update(et_); // PlayerInputなどで velocity.x, z を設定
    }

    // 2. 衝突判定と「最終補正」
    // ここで座標の押し戻し(pos += sep)と、velocity.y のリセットを行う
    CollisionStep();

    //LateUpdate
    for (auto& obj : objects) {
        if (auto col = obj->GetComponent<ColliderComponent>()) {
            col->ApplyGravity(et_); // ここで velocity.y を更新し、仮移動(pos += v*dt)を行う
        }
        obj->LateUpdate(et_); // PlayerInputなどで velocity.x, z を設定
    }

    // 3. 確定した座標を反映（ここでこれ以上座標を動かさない！）
    for (auto& obj : objects) {
        obj->UpdateTransform();
    }

    if (m_pMainCamera != nullptr)
    {
        std::shared_ptr<CameraComponent> cameraComponent = m_pMainCamera->GetComponent<CameraComponent>();
        cameraComponent->UpdateViewProjection(et_);

        particleSystem->UpdateAll(
            GraphicsDeviceManager::Instance().GetContext(),
            et_,
            cameraComponent->GetViewProjection(),
            m_particleComponents);
    }

    // ★ 最後にまとめて削除を確定させる
    if (!pendingDestroys.empty()) {
        // 削除中にさらにDestroyが呼ばれる可能性に備え、一旦ローカルにコピー
        std::vector<std::shared_ptr<GameObject>> currentDestroys = std::move(pendingDestroys);
        pendingDestroys.clear();

        for (auto& obj : currentDestroys) {
            ApplyDestroys(obj);
        }
    }

    // --- 1. 無効になった（削除された）パーティクルコンポーネントを掃除 ---
    m_particleComponents.erase(
        std::remove_if(m_particleComponents.begin(), m_particleComponents.end(),
            [](const std::weak_ptr<ParticleComponent>& wp) {
                return wp.expired(); // GameObjectと共に破棄されていたら削除
            }),
        m_particleComponents.end()
    );


}

void GameObjectManager::Clear()
{
    GraphicsDeviceManager::Instance().ResetFilter();

    selectedObject = nullptr;
    m_pMainCamera = nullptr;


    m_pRayCastManager->Clear();//レイキャストマネージャークリア処理

    // 各オブジェクトが持っているComPtrを確実に解放させるため、
    // リストをクリアする前に念のため各オブジェクトの終了処理を呼ぶのが理想です
// 1. まず全てのトリガーリストをクリアして循環参照を解く
    for (auto& obj : objects) {
        if (!obj) continue;
        auto col = obj->GetComponent<ColliderComponent>();
        if (col) {
            col->currentTriggers.clear();
        }
    }
    objects.clear(); // ここで shared_ptr の参照カウントが減り、実体が削除される
}


std::vector<std::string> GetSceneDirectories() {
    std::vector<std::string> folders;
    std::string path = "Scenes/";

    // Scenesフォルダが存在するか確認
    if (!std::filesystem::exists(path)) return folders;

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.is_directory()) {
            // フォルダ名だけを抽出して追加
            folders.push_back(entry.path().filename().string());
        }
    }
    return folders;
}


void GameObjectManager::CollisionStep()
{
    struct ColliderPair {
        std::shared_ptr<GameObject> obj;
        std::shared_ptr<ColliderComponent> col;
    };
    std::vector<ColliderPair> activeColliders;

    //コリジョンのコンポーネントがあるゲームオブジェクトを取得する
    for (auto& obj : objects) {
        auto col = obj->GetComponent<ColliderComponent>();
        //if (obj->m_pColliderComponent == nullptr) continue;
        //auto col = obj->m_pColliderComponent;
        if (col && col->IsEnabled()) {
            activeColliders.push_back({ obj, col });
        }
    }

    for (size_t i = 0; i < activeColliders.size(); ++i) {
        for (size_t j = i + 1; j < activeColliders.size(); ++j) {

            ColliderPair A = activeColliders[i];
            ColliderPair B = activeColliders[j];



            //動かないオブジェクト同士なら当たり判定は行わない
            if (A.col->isStatic && B.col->isStatic)
            {
                continue;
            }

            CollisionManifold manifold = A.col->CheckCollision(B.col);

            if (manifold.isHit)
            {

                if (A.col->isTrigger || B.col->isTrigger) {
                    HandlePhysXTrigger(A.col.get(), B.col.get(), true);
                }
                else 
                {
                    //レイヤーがかぶっていたらその時だけは押し出し処理をやめる
                    int a = (uint32_t)B.obj->layer;
                    if ((A.obj->m_Mask & (uint32_t)B.obj->layer) != 0)
                    {
                        HandlePhysXTrigger(A.col.get(), B.col.get(), true);
                    }
                    else
                    {
                        HandlePhysXContact(A.col.get(), B.col.get());

                        // 1. 押し戻し比率
                        float weightA = 0.5f; float weightB = 0.5f;
                        if (A.col->isStatic) 
                        { 
                            weightA = 0.0f; weightB = 1.0f;
                        }
                        else if (B.col->isStatic) 
                        {
                            weightA = 1.0f; weightB = 0.0f;
                        }
                        else
                        {
                            //トータルの重さを求める
                            float total = A.col->m_Mass + B.col->m_Mass;

                            //ゼロ除算をなくすため
                            if (total == 0.0f)
                            {
                                weightA = weightB = 0.5f;
                            }
                            else
                            {
                                weightA = B.col->m_Mass / total;
                                weightB = A.col->m_Mass / total;
                            }
                        }

                        // 2. 座標の押し戻し（めり込み解消）
                        DirectX::XMVECTOR sepA = DirectX::XMVectorScale(manifold.normal, -manifold.depth * weightA);
                        DirectX::XMVECTOR sepB = DirectX::XMVectorScale(manifold.normal, manifold.depth * weightB);

                        auto ApplyMove = [](std::shared_ptr<GameObject> obj, DirectX::XMVECTOR sep) {
                            DirectX::XMFLOAT3 m;
                            DirectX::XMStoreFloat3(&m, sep);
                            obj->transform->localPosition.x += m.x;
                            obj->transform->localPosition.y += m.y;
                            obj->transform->localPosition.z += m.z;
                            obj->transform->MarkDirty();
                            };
                        ApplyMove(A.obj, sepA);
                        ApplyMove(B.obj, sepB);

                        // 3. 垂直・水平の判定と速度リセット
                        // 法線のY成分を確認 (0.7f 程度以上なら「床」とみなす)
                        float normalY = DirectX::XMVectorGetY(manifold.normal);

                        auto ResolveVelocitySplit = [&](std::shared_ptr<ColliderComponent> col, float sign)
                            {
                                if (col->isStatic) return;

                                // signを考慮した法線の向き
                                float adjustedNormalY = DirectX::XMVectorGetY(manifold.normal) * sign;

                                if (adjustedNormalY > 0.7f) { // 床に接触
                                    col->isGrounded = true;   // ★接地フラグをオン
                                    if (col->velocity.y < 0.0f) {
                                        col->velocity.y = 0.0f; // 落下速度をリセット
                                        col->isGrounded = true;
                                    }
                                    else
                                    {
                                        col->isGrounded = false;
                                    }
                                }
                                else if (adjustedNormalY < -0.7f) { // 天井に接触
                                    if (col->velocity.y > 0.0f) {
                                        col->velocity.y = 0.0f; // 上昇速度をリセット
                                        col->isGrounded = false;
                                    }
                                }
                                else {
                                    // 水平方向（壁）
                                    DirectX::XMVECTOR vel = DirectX::XMLoadFloat3(&col->velocity);
                                    DirectX::XMVECTOR dotVec = DirectX::XMVector3Dot(vel, manifold.normal);
                                    float dot = DirectX::XMVectorGetX(dotVec) * sign;

                                    if (dot < 0.0f) {
                                        DirectX::XMVECTOR correction = DirectX::XMVectorScale(manifold.normal, dot * sign);
                                        vel = DirectX::XMVectorSubtract(vel, correction);
                                        DirectX::XMStoreFloat3(&col->velocity, vel);
                                    }
                                }
                            };

                        ResolveVelocitySplit(A.col, 1.0f);
                        ResolveVelocitySplit(B.col, -1.0f);
                    }
         
                }
            }
            else
            {
                HandlePhysXTrigger(A.col.get(), B.col.get(), false);
            }
        }
    }
}


void GameObjectManager::Gui()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    // --- 追加: 画面全体を覆う「背景ダミー」 ---
    // これを置くことで、UI以外の場所をクリックしても「UIを触っている」状態を維持できます
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.0f); // 背景は透明
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // 背景クリック判定用ウィンドウ
    ImGui::Begin("BackgroundSpace", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs | // ← これを外すと完全に背景をクリックできなくなります
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | // 背景なので前面に来させない
        ImGuiWindowFlags_NoFocusOnAppearing);

    ImGui::End();
    ImGui::PopStyleVar(2);

    // --- 静的な変数で横幅を保持（初回のみ初期化） ---
    static float leftPanelWidth = 300.0f;
    static float rightPanelWidth = 350.0f;

    // ウィンドウフラグ（NoMoveは維持し、NoResizeは外す）
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

    // ========================================
    // 左パネル: Objects (Hierarchy)
    // ========================================
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, viewport->WorkSize.y));

    // --- 追加: セーブ・ロード用の管理変数 (staticで保持) ---
    static char sceneName[64] = "MainScene"; // デフォルトのシーン名

    // 左: オブジェクト一覧
    ImGui::Begin("Objects");

    if (ImGui::CollapsingHeader("Debug Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Render Shapes", &shapeRenderFlg); // コライダーなどの描画
        ImGui::Checkbox("Render NaviMesh", &naviMeshRenderFlg); // ナビメッシュの描画
    }

    // ========================================
    // ★ Scene Save / Load UI
    // ========================================
  // 静的な変数として保持（毎フレームスキャンすると重いため）
    static std::vector<std::string> sceneList = GetSceneDirectories();
    static int selectedSceneIndex = -1;

    ImGui::Text("Scene Management");

    // --- シーン選択コンボボックス ---
    if (ImGui::Button("Refresh List")) {
        sceneList = GetSceneDirectories(); // フォルダが増えた時の更新ボタン
    }

    ImGui::InputText("SceneName", sceneName, IM_ARRAYSIZE(sceneName));

    const char* previewValue = (selectedSceneIndex >= 0) ? sceneList[selectedSceneIndex].c_str() : "Select a scene...";

    if (ImGui::BeginCombo("Select Scene", previewValue)) {
        for (int i = 0; i < sceneList.size(); i++) {
            const bool isSelected = (selectedSceneIndex == i);
            if (ImGui::Selectable(sceneList[i].c_str(), isSelected)) {
                selectedSceneIndex = i;
                // 選択された名前をsceneNameバッファにコピー
                strncpy_s(sceneName, sceneList[i].c_str(), IM_ARRAYSIZE(sceneName));
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }


    // --- 保存・読み込みボタン ---
    static bool openSaveConfirm = false;
    static bool openLoadConfirm = false;
    if (ImGui::Button("Save Scene")) {
        openSaveConfirm = true;
        ImGui::OpenPopup("SaveConfirm");
    }

    if (ImGui::BeginPopupModal("SaveConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Saves Scene \"%s\" ?", sceneName);
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            SaveScene(sceneName);
            sceneList = GetSceneDirectories(); // リスト更新
            openSaveConfirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            openSaveConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) {
        if (strlen(sceneName) > 0) {
            openLoadConfirm = true;
            ImGui::OpenPopup("LoadConfirm");
        }
    }

    if (ImGui::BeginPopupModal("LoadConfirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Loading Scene \"%s\" ? \n Now Scenes Destruction", sceneName);
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            LoadScene(sceneName);
            openLoadConfirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("No", ImVec2(120, 0))) {
            openLoadConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();


    // === Create UI（上部）===
    static char createName[64] = "GameObject";

    ImGui::Text("Create GameObject");
    ImGui::InputText("##CreateName", createName, IM_ARRAYSIZE(createName));

    if (ImGui::Button("Create"))
    {
        // ★ 必ず Create() を通す
        selectedObject = Create(createName);
    }

    ImGui::Separator();

    // === ★ プレハブ管理 (Prefab Management) ===
    if (ImGui::CollapsingHeader("Prefabs", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Refresh")) RefreshPrefabList();

        static char newPrefabName[64] = "";

        // プレハブ選択コンボボックス
        if (!m_prefabFiles.empty()) {
            const char* currentItem = m_prefabFiles[m_selectedPrefabIndex].c_str();
            if (ImGui::BeginCombo("Select Prefab", currentItem)) {
                for (int n = 0; n < m_prefabFiles.size(); n++) {
                    bool isSelected = (m_selectedPrefabIndex == n);
                    if (ImGui::Selectable(m_prefabFiles[n].c_str(), isSelected)) {
                        m_selectedPrefabIndex = n;
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Button("Instantiate (Create from Prefab)")) {
                selectedObject = Instantiate(m_prefabFiles[m_selectedPrefabIndex]);
                memset(newPrefabName, 0, sizeof(newPrefabName));
            }
        }
        else {
            ImGui::Text("No prefabs found in /Prefabs");
        }

        // 選択中のオブジェクトをプレハブとして保存するボタン
        if (selectedObject) {

            ImGui::InputText("New Prefab Name", newPrefabName, IM_ARRAYSIZE(newPrefabName));
            if (ImGui::Button("Save Selected as Prefab")) {
                if (strlen(newPrefabName) > 0) {
                    SaveAsPrefab(selectedObject, newPrefabName);
                    RefreshPrefabList();
                    memset(newPrefabName, 0, sizeof(newPrefabName));
                }
            }
        }
    }

    ImGui::Separator();

    // === オブジェクト一覧描画 ===
    GameObject* objectToDelete = nullptr;

    // スクロール可能な領域を作る
    ImGui::BeginChild("HierarchyRegion", ImVec2(0, 0), true);

    for (auto& obj : objects)
    {
        // 親がいない（expired == 親がセットされていない）オブジェクトだけを描画の起点にする
        if (obj->transform->parent.expired())
        {
            DrawHierarchyNode(obj, &objectToDelete);
        }
    }

    ImGui::Dummy(ImGui::GetContentRegionAvail()); // 残りの領域をダミーで埋める

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
            std::shared_ptr<GameObject> draggedObj = *(std::shared_ptr<GameObject>*)payload->Data;

            // 親を nullptr にしてルート階層に戻す
            draggedObj->SetParent(nullptr);
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();

    // 実際の削除処理 (shared_ptr版)
    if (objectToDelete)
    {
        if (selectedObject && selectedObject.get() == objectToDelete) selectedObject = nullptr;

        // 削除されるオブジェクトを特定
        auto it = std::find_if(objects.begin(), objects.end(),
            [&](const std::shared_ptr<GameObject>& ptr) { return ptr.get() == objectToDelete; });

        if (it != objects.end())
        {
            // ★重要：削除されるオブジェクトの子供たちを救出する
            auto targetObj = *it;
            auto& children = targetObj->transform->children;

            // 子供たちの親を nullptr にしてルートへ戻す
            // ループ内で中身が変わる可能性があるのでコピーを取るか、慎重に回す
            auto childrenCopy = children;
            for (auto& childTrans : childrenCopy)
            {
                if (auto childGO = childTrans->gameObject.lock())
                {
                    childGO->SetParent(nullptr); // 親を外してルートへ
                }
            }

            // 選択解除などの既存処理
            if (selectedObject == targetObj) selectedObject = nullptr;
            if (m_pMainCamera == targetObj) m_pMainCamera = nullptr;

            // リストから削除
            objects.erase(it);
        }

    }

    ImGui::End();

    // ========================================
     // 右パネル: Inspector (selectedObject)
     // ========================================
    if (!selectedObject)return;

    // 右側に張り付くように位置を計算
    ImVec2 inspectorPos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - rightPanelWidth, viewport->WorkPos.y);

    ImGui::SetNextWindowPos(inspectorPos);
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, viewport->WorkSize.y));

    if (ImGui::Begin("Inspector", nullptr, windowFlags)) {
        // --- ここで手動リサイズを検知 ---
        rightPanelWidth = ImGui::GetWindowWidth();

        if (ImGui::CollapsingHeader(selectedObject->GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            selectedObject->Gui();
        }
        ImGui::End();
    }
    // ========================================
  // ★ ギズモの描画処理を追加
  // ========================================
  // メインカメラコンポーネントを取得
    auto cameraObj = Find("MainCamera");
    if (cameraObj)
    {
        auto cameraComp = cameraObj->GetComponent<CameraComponent>();
        if (cameraComp)
        {
            // ImGuizmoのフレーム開始準備
            ImGuizmo::BeginFrame();

            // カメラの行列を準備
            DirectX::XMFLOAT4X4 view, proj;
            view = cameraComp->GetView();       // CameraComponentの実装に合わせて適宜変更
            proj = cameraComp->GetProjection();

            // 選択中のオブジェクトに対してギズモを描画
            // &view._11 のように float* 型で渡す
            selectedObject->DrawGizmo(&view._11, &proj._11);
        }
    }
}

// GameObjectManager内の private か、Gui関数の上に追加
void GameObjectManager::DrawHierarchyNode(std::shared_ptr<GameObject> obj, GameObject** toDelete)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (obj == selectedObject) flags |= ImGuiTreeNodeFlags_Selected;
    if (obj->transform->children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

    bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)obj.get(), flags, "%s", obj->GetName().c_str());

    // --- 1. ドラッグの開始 (Source) ---
    if (ImGui::BeginDragDropSource()) {
        // ドラッグ中のデータとして、自身の共有ポインタのアドレスを渡す
        // "DND_GAMEOBJECT" はこの種類のドラッグ＆ドロップを識別する任意の文字列
        ImGui::SetDragDropPayload("DND_GAMEOBJECT", &obj, sizeof(std::shared_ptr<GameObject>));

        // ドラッグ中にマウスカーソルに表示されるテキスト
        ImGui::Text("Move %s", obj->GetName().c_str());
        ImGui::EndDragDropSource();
    }

    // --- 2. ドロップの受け入れ (Target) ---
    if (ImGui::BeginDragDropTarget()) {
        // "DND_GAMEOBJECT" というラベルのデータが重なった時だけ判定
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
            // ペイロードからドラッグされたオブジェクトを取り出す
            std::shared_ptr<GameObject> draggedObj = *(std::shared_ptr<GameObject>*)payload->Data;

            // 自分自身を自分の子にはできない、かつ今の親でない場合のみ実行
            if (draggedObj != obj) {
                draggedObj->SetParent(obj); // 親子付け実行！
            }
        }
        ImGui::EndDragDropTarget();
    }

    // クリックで選択
    if (ImGui::IsItemClicked()) {
        selectedObject = obj;
    }

    // 右クリックメニュー
    if (ImGui::BeginPopupContextItem()) {
        selectedObject = obj;
        if (ImGui::MenuItem("Delete GameObject")) {
            //*toDelete = obj.get();
            Destroy(obj->GetName());
        }
        ImGui::EndPopup();
    }

    // ノードが開いている場合、子供を再帰的に描画
    if (opened) {
        for (auto& childTrans : obj->transform->children) {
            if (auto childGO = childTrans->gameObject.lock()) {
                DrawHierarchyNode(childGO, toDelete);
            }
        }
        ImGui::TreePop();
    }


}

void GameObjectManager::RenderAll(SpriteRenderSystem spriteSystem_, ModelRenderSystem modelSystem_)
{
    auto ctx = GraphicsDeviceManager::Instance().GetContext();
    auto& gdm = GraphicsDeviceManager::Instance();

    // --- 追加: ライト情報の収集 ---
    LightManager lightManager;
    // デフォルトの平行光源設定（必要に応じて）
    DirectionalLight dl;
    dl.direction = { -1, -1, 1 };
    lightManager.SetDirectionalLight(dl);

    DecalManager decalManager; // デカール用マネージャー

    for (auto& o : objects)
    {
        auto lightComp = o->GetComponent<LightComponent>();
        if (lightComp && lightComp->IsEnabled())
        {
            // A. 平行光源
            if (lightComp->type == LightComponent::Type::Directional)
            {
                DirectionalLight dl;
                dl.direction = lightComp->finalWorldDirection;
                dl.color = lightComp->color;
                dl.intensity = lightComp->intensity;
                lightManager.SetDirectionalLight(dl);
            }
            // B. 点光源
            else if (lightComp->type == LightComponent::Type::Point)
            {
                PointLight pl;
                pl.position = lightComp->finalWorldPosition;
                pl.color = lightComp->color;
                pl.intensity = lightComp->intensity;
                pl.range = lightComp->range;
                lightManager.AddPointLight(pl);
            }
            // C. スポットライト
            else if (lightComp->type == LightComponent::Type::Spot)
            {
                SpotLight sl;
                sl.position = lightComp->finalWorldPosition;
                sl.direction = lightComp->finalWorldDirection;
                sl.color = lightComp->color;
                sl.intensity = lightComp->intensity;
                sl.range = lightComp->range;
                sl.innerAngle = lightComp->innerAngle;
                sl.outerAngle = lightComp->outerAngle;
                lightManager.AddSpotLight(sl);
            }
        }

        auto decalComp = o->GetComponent<DecalComponent>();

        // デカールの収集
        if (decalComp && decalComp->IsEnabled())
        {
            decalComp->SubmitDecal(decalManager);
        }


    }


    for (auto& o : objects)
    {
        auto md = o->GetComponent<GltfModelCommponennt>();
        if (md)
            modelSystem_.PushSubmit(md->GetShaderID(), md);
    }
    RenderContext rc;

    rc.deviceContext = ctx;
    rc.camera = Find("MainCamera")->GetComponent<CameraComponent>().get();
    rc.renderState = GraphicsDeviceManager::Instance().GetRenderState();
    rc.lights = &lightManager;
    rc.decal = &decalManager;
    rc.baseColorSRV = GraphicsDeviceManager::Instance().GetSceneSRV();
    rc.depthSRV = GraphicsDeviceManager::Instance().GetMainDepthSRV();
    rc.shapeSystem = &GraphicsDeviceManager::Instance().GetShapeRendererSystem();

    // ここでレンダー状態をリセット
    ctx->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    ctx->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    ID3D11SamplerState* samplers[] = { rc.renderState->GetSamplerState(SamplerState::LinearWrap) };
    ctx->PSSetSamplers(0, 1, samplers);

    // 1. シーンバッファ（HDR）へ描画開始
    gdm.SetSceneRenderTarget();

    modelSystem_.RenderAll(rc);

    if(shapeRenderFlg)
    {
        GraphicsDeviceManager::Instance().GetShapeRendererSystem().Render(ctx, rc.camera->GetView(), rc.camera->GetProjection());
    }

    gdm.SetParticleRenderTarget();
    particleSystem->RenderAll(rc);

    // 2. ブルームをかけてバックバッファへ出力
    gdm.ResolveBloom(rc);

    // SpriteRenderSubmit
    for (auto& o : objects)
    {
        auto sr = o->GetComponent<SpriteRenderer>();
        if (sr && sr->enabled)
            spriteSystem_.Submit(sr, sr->GetSpriteShaderID());
    }

    ctx->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

    spriteSystem_.RenderAll(ctx);
}

void HandleTrigger(
    std::shared_ptr<ColliderComponent> colA,
    std::shared_ptr<ColliderComponent> colB,
    std::shared_ptr<GameObject> objA,
    std::shared_ptr<GameObject> objB,
    bool hit)
{
    auto existsA = colA->currentTriggers.count(objB);
    auto existsB = colB->currentTriggers.count(objA);

    if (hit)
    {
        if (!existsA)
        {
            colA->currentTriggers.insert(objB);
            colA->OnTriggerEnter(objB);
        }
        else
        {
            colA->OnTriggerStay(objB);
        }

        if (!existsB)
        {
            colB->currentTriggers.insert(objA);
            colB->OnTriggerEnter(objA);
        }
        else
        {
            colB->OnTriggerStay(objA);
        }
    }
    else
    {
        if (existsA)
        {
            colA->currentTriggers.erase(objB);
            colA->OnTriggerExit(objB);
        }
        if (existsB)
        {
            colB->currentTriggers.erase(objA);
            colB->OnTriggerExit(objA);
        }
    }
}



void GameObjectManager::Destroy(std::shared_ptr<GameObject> obj)
{
    if (!obj) return;
    // 即座に消さず、予約リストに入れる
    pendingDestroys.push_back(obj);
}

void GameObjectManager::Destroy(std::string objName)
{
    for (auto& obj : objects) {
        if (obj && obj->GetName() == objName) {
            Destroy(obj); // 上のDestroyを呼ぶ
            return; // 1つ見つけたら終了（全部消すならreturnしない）
        }
    }
}

void GameObjectManager::HandlePhysXTrigger(ColliderComponent* colA, ColliderComponent* colB, bool hit)
{
    if (!colA || !colB) return;
    auto objA = colA->GetGameObject();
    auto objB = colB->GetGameObject();
    if (!objA || !objB) return;

    //そのリストの中に存在するかどうかをチェックする
    auto existsA = colA->currentTriggers.count(objB);
    auto existsB = colB->currentTriggers.count(objA);

    //ヒットしているのなら
    if (hit)
    {
        //存在していなければ追加
        if (!existsA)
        {
            colA->currentTriggers.insert(objB);
            colA->OnTriggerEnter(objB); 
        }
        else
        {
            colA->OnTriggerStay(objB); 
        }

        //存在していなければ
        if (!existsB) 
        {
            colB->currentTriggers.insert(objA); 
            colB->OnTriggerEnter(objA);
        }
        else
        {
            colB->OnTriggerStay(objA); 
        }
    }
    //リストにあれば削除する
    else
    {
        if (existsA) 
        { 
            colA->currentTriggers.erase(objB); 
            colA->OnTriggerExit(objB); 
        }
        if (existsB) 
        {
            colB->currentTriggers.erase(objA);
            colB->OnTriggerExit(objA); 
        }
    }
}

void GameObjectManager::HandlePhysXContact(ColliderComponent* colA, ColliderComponent* colB)
{
    if (!colA || !colB) return;
    auto objA = colA->GetGameObject();
    auto objB = colB->GetGameObject();
    if (!objA || !objB) return;

    colA->OnCollisionEnter(objB);
    colB->OnCollisionEnter(objA);
}

// ヘルパー関数：親→子の順番で名前をリストに追加する
void CollectObjectNames(const std::shared_ptr<GameObject>& obj, nlohmann::json& listJson) {
    if (!obj->saveFlg) return;

    // 自分の名前を登録
    listJson["ObjectList"].push_back(obj->name);

    // 子供たちの名前を再帰的に登録
    for (auto& child : obj->transform->children) {
        CollectObjectNames(child->gameObject.lock(), listJson);
    }
}

void GameObjectManager::SaveScene(const std::string& sceneName)
{
    std::string basePath = "Scenes/" + sceneName + "/";
    std::string objectsPath = basePath + "Objects/";
    std::filesystem::create_directories(objectsPath);

    nlohmann::json listJson;
    listJson["SceneName"] = sceneName;
    listJson["ObjectList"] = nlohmann::json::array(); // 配列を初期化

    // --- グラフィックス設定（そのまま） ---
    auto& params = GraphicsDeviceManager::Instance().GetBloomParams();
    listJson["GraphicsSettings"]["Bloom"] = {
        {"threshold", params.threshold},
        {"intensity", params.intensity},
        {"kawaseScale", params.kawaseScale},
        {"blurPasses", params.blurPasses}
    };

    // 1. 各オブジェクトの個別ファイルを保存
    // (ここは順不同でOKなので、まずは全ファイルを書き出す)
    for (auto& obj : objects) {
        if (!obj->saveFlg) continue;

        std::string filePath = objectsPath + obj->name + ".json";
        nlohmann::json objJson = obj->Save();
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << objJson.dump(4);
        }
    }

    // 2. リスト JSON に「親 → 子」の順で名前を記録
    // 親がいない（ルート）オブジェクトを探して、そこから再帰を開始する
    for (auto& obj : objects) {
        if (obj->saveFlg && obj->transform->parent.lock() == nullptr) {
            CollectObjectNames(obj, listJson);
        }
    }

    // 3. 管理用リストファイルを保存
    std::ofstream listFile(basePath + "SceneList.json");
    if (listFile.is_open()) {
        listFile << listJson.dump(4);
    }

    printf("Scene '%s' saved with ordered SceneList.json\n", sceneName.c_str());
}

void GameObjectManager::LoadScene(const std::string& sceneName)
{

    Clear(); // 現在のシーンをクリア
    std::string basePath = "Scenes/" + sceneName + "/";
    std::string listFilePath = basePath + "SceneList.json";

    std::ifstream listFile(listFilePath);
    if (!listFile.is_open()) {
        printf("Error: SceneList.json not found!\n");
        return;
    }
    nlohmann::json listJson;
    listFile >> listJson;

    ActionRegistry::Instance().Clear();
    RegisterAllActions();

    EventManager::Clear();


    // --- ★ グラフィックス設定（ブルーム）の読み込みを追加 ---
    if (listJson.contains("GraphicsSettings") && listJson["GraphicsSettings"].contains("Bloom")) {
        auto& bloomJson = listJson["GraphicsSettings"]["Bloom"];
        auto& params = GraphicsDeviceManager::Instance().GetBloomParams(); // 書き込み先の参照取得

        params.threshold = bloomJson.value("threshold", 1.0f);
        params.intensity = bloomJson.value("intensity", 1.0f);
        params.kawaseScale = bloomJson.value("kawaseScale", 1.0f);
        params.blurPasses = bloomJson.value("blurPasses", 4);
    }



    // SceneListに記載されている名前の順にロード
    for (const auto& name : listJson["ObjectList"]) {
        std::string objName = name.get<std::string>();
        std::string filePath = basePath + "Objects/" + objName + ".json";

        std::ifstream file(filePath);
        if (file.is_open()) {
            nlohmann::json objJson;
            file >> objJson;

            // オブジェクト生成
            auto obj = Create(objName);

            //メインカメラの再設定
            if (objName == "MainCamera") {
                SetMainCamera(obj);
            }

            // オブジェクト自身にコンポーネントを含めて復元させる
            obj->Load(objJson);
        }
    }

    // ★ここで親子関係を復元する
    ResolveHierarchy();

    printf("Scene '%s' loaded from SceneList.json\n", sceneName.c_str());
}

void GameObjectManager::ResolveHierarchy()
{
    for (auto& obj : objects)
    {
        // ロード時に一時保存した「親の名前」を取得
        std::string pName = obj->transform->parentNameDuringLoad;

        if (!pName.empty())
        {
            // 名前でマネージャー内のオブジェクトを検索
            auto parentObj = Find(pName);
            if (parentObj)
            {
                // 親が見つかったら、Transformの親子付けを実行
                obj->SetParent(parentObj);
            }
        }
    }

    // 全体の親子が確定したので、一度行列を正しく計算させる
    for (auto& obj : objects)
    {
        obj->UpdateTransform();
    }
}


// フォルダ内のJSONファイルをスキャンする
void GameObjectManager::RefreshPrefabList()
{
    m_prefabFiles.clear();
    if (!std::filesystem::exists(m_prefabDirectory)) {
        std::filesystem::create_directories(m_prefabDirectory);
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_prefabDirectory)) {
        if (entry.path().extension() == ".json") {
            // ファイル名（拡張子なし）をリストに追加
            m_prefabFiles.push_back(entry.path().stem().string());
        }
    }
}

void GameObjectManager::ApplyDestroys(std::shared_ptr<GameObject> obj)
{
    // 1. 子供を先に全て削除する（再帰）
    auto childrenCopy = obj->transform->children;
    for (auto& childTrans : childrenCopy)
    {
        if (auto childGO = childTrans->gameObject.lock())
        {
            ApplyDestroys(Find(childTrans->gameObject.lock()->GetName()));
        }
    }

    // 2. 特殊な参照を外す
    if (selectedObject == obj) selectedObject = nullptr;
    if (m_pMainCamera == obj) m_pMainCamera = nullptr;

    // 3. マネージャーのメインリストから自分を削除
    auto it = std::remove(objects.begin(), objects.end(), obj);
    if (it != objects.end()) {
        objects.erase(it, objects.end());
    }
}

// プレハブ保存
void GameObjectManager::SaveAsPrefab(std::shared_ptr<GameObject> obj, const std::string& prefabName)
{
    if (!obj) return;

    std::filesystem::create_directories(m_prefabDirectory);
    std::string filePath = m_prefabDirectory + prefabName + ".json";

    nlohmann::json j = obj->Save();
    std::ofstream file(filePath);
    if (file.is_open()) {
        file << j.dump(4);
        RefreshPrefabList(); // リストを更新
    }
}


void GameObjectManager::InitializeAllComponents()
{
    // ★ 現在存在しているオブジェクトをスナップショット
    auto objectsSnapshot = objects;

    for (auto& obj : objectsSnapshot)
    {
        if (!obj) continue;

        // ActionSequence 以外のコンポーネントの Start を呼ぶ
        // (あるいは、ActionSequence を持っていないオブジェクトを先に処理)
        if (!obj->GetComponent<ActionSequence>()) {
            obj->CallAllComponentStart();
        }
    }

    // 2. 次に「アクションを利用する側」の Start を呼ぶ
    // ここで ActionSequence が RegisterSequence を実行する
    for (auto& obj : objectsSnapshot)
    {
        if (!obj) continue;

        if (auto seq = obj->GetComponent<ActionSequence>()) {
            obj->CallAllComponentStart();
        }
    }
}

// プレハブから生成
std::shared_ptr<GameObject> GameObjectManager::Instantiate(const std::string& prefabName)
{
    std::string filePath = m_prefabDirectory + prefabName + ".json";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        printf("Error: Prefab '%s' not found!\n", prefabName.c_str());
        return nullptr;
    }

    nlohmann::json prefabJson;
    file >> prefabJson;

    // 1. 重複しないユニークな名前を生成する
    std::string uniqueName = MakeUniqueName(prefabName);

    // 2. その名前でオブジェクトを作成
    // ※Create内部でも重複チェックがあるかもしれませんが、
    //   ここで確定させた名前を渡すのが一番確実です。
    auto newObj = Create(uniqueName);

    // 3. Jsonからデータをロード
    newObj->Load(prefabJson);

    // 4. LoadするとJson内の"Name"で上書きされてしまうため、
    //    確定したユニーク名に再度設定し直す
    newObj->SetName(uniqueName.c_str());
    Vector3 p(0, 0, 0);
    newObj->transform->SetLocalPosition(p);

    printf("Instantiated: %s\n", uniqueName.c_str());
    return newObj;
}