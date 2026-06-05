#include "GameObjectManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/Collision.h"
#include "Engine/Physics/BoxCollider.h"
#include <PxPhysicsAPI.h>

#include "Engine/Graphics/Decal/DecalComponent.h"
#include"ImGuizmo.h"
#include <nlohmann/json.hpp>
#include"Engine/GamePlay/Transform/TransformDirector.h"
#include"Engine/GamePlay/Camera/CameraDirectorComponent.h"
#include"Engine/GamePlay/EventSystem/ActionSequence.h"

#include"Engine/Graphics/PostEffect/ColorFilterDirector.h"

#include <fstream>
#include"Engine/GamePlay/EventSystem/EventManager.h"

#include "Engine/GamePlay/System/TimeManager.h"
#include "Engine/GamePlay/Camera/CameraManager.h"

void GameObjectManager::Initialize()
{
    // パーティクルシステムの初期化
    _ParticleSystem = std::make_unique<ParticleRenderSystem>();
    _ParticleSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());

    _TimeManager = std::make_shared<TimeManager>();

    _CameraManager = std::make_shared<CameraManager>();
    _CameraManager->Initialize();
}

void GameObjectManager::Update(float et_)
{
    _TimeManager->Update(et_);

    // 新しく追加されたオブジェクトを一度だけ初期化 (StartObjects)
    if (!_PendingStarts.empty()) {
        std::vector<std::shared_ptr<GameObject>> startList = std::move(_PendingStarts);
        _PendingStarts.clear();

        // STEP 1: Director系（アクションを提供する側）の登録
        for (auto& obj : startList) {
            if (!obj) continue;

            // --- ここでパーティクルコンポーネントをリストに登録 ---
            if (auto pComp = obj->GetComponent<ParticleComponent>()) {
                _ParticleComponents.push_back(pComp);
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


    _TimeManager->Update(et_);
    _TimeManager->PrepareCache();

    // 2. オブジェクトの更新
    for (auto& obj : _Objects) {
        // ループの外、あるいは事前に取得したキャッシュを参照
        float scale = _TimeManager->GetCachedScale(obj->GetLayerBit());
        obj->Update(et_ * scale);
    }
    // 2. 衝突判定と「最終補正」
    // ここで座標の押し戻し(pos += sep)と、velocity.y のリセットを行う
    CollisionStep();

    //LateUpdate
    for (auto& obj : _Objects) {
        if (auto col = obj->GetComponent<ColliderComponent>()) {
            col->ApplyGravity(et_); // ここで velocity.y を更新し、仮移動(pos += v*dt)を行う
        }
        obj->LateUpdate(et_); // PlayerInputなどで velocity.x, z を設定
    }

    // 3. 確定した座標を反映（ここでこれ以上座標を動かさない！）
    for (auto& obj : _Objects) {
        obj->UpdateTransform();
    }

    int highestPriority = -2147483648; // 最低値から開始

    // ★ 最後にまとめて削除を確定させる
    if (!_PendingDestroys.empty()) {
        // 削除中にさらにDestroyが呼ばれる可能性に備え、一旦ローカルにコピー
        std::vector<std::shared_ptr<GameObject>> currentDestroys = std::move(_PendingDestroys);
        _PendingDestroys.clear();

        for (auto& obj : currentDestroys) {
            ApplyDestroys(obj);
        }
    }

    // --- 1. 無効になった（削除された）パーティクルコンポーネントを掃除 ---
    _ParticleComponents.erase(
        std::remove_if(_ParticleComponents.begin(), _ParticleComponents.end(),
            [](const std::weak_ptr<ParticleComponent>& wp) {
                return wp.expired(); // GameObjectと共に破棄されていたら削除
            }),
        _ParticleComponents.end()
    );

    _CameraManager->Update(et_,_Objects);
}

void GameObjectManager::Clear()
{
    GraphicsDeviceManager::Instance().ResetFilter();

    _SelectedObject = nullptr;



    // 各オブジェクトが持っているComPtrを確実に解放させるため、
    // リストをクリアする前に念のため各オブジェクトの終了処理を呼ぶのが理想です
    // 1. まず全てのトリガーリストをクリアして循環参照を解く
    for (auto& obj : _Objects) {
        if (!obj) continue;
        auto col = obj->GetComponent<ColliderComponent>();
        if (col) {
            col->currentTriggers.clear();
        }
    }
    _Objects.clear(); // ここで shared_ptr の参照カウントが減り、実体が削除される
}

// オブジェクトを追加
void GameObjectManager::AddObject(std::shared_ptr<GameObject> obj)
{
    if (!obj) return; // Nullならreturn

    // 重複を避ける
    for (const auto& existing : _Objects) {
        if (existing->GetName() == obj->GetName()) {
            return;
        }
    }

    _Objects.push_back(obj);
}

// 謁見の間：オブジェクトを追放する
void GameObjectManager::RemoveObject(const std::string& name)
{
    // 追放の儀式
    auto it = std::remove_if(_Objects.begin(), _Objects.end(),
        [&name](const std::shared_ptr<GameObject>& obj) {
            return obj->GetName() == name;
        });

    if (it != _Objects.end()) {
        // もし追放する者が現在選ばれているなら、選択を解除する（世界崩壊を防ぐ）
        if (_SelectedObject && _SelectedObject->GetName() == name) {
            _SelectedObject = nullptr;
        }
        _Objects.erase(it, _Objects.end());
    }
}

std::vector<std::string> GetSceneDirectories() {
    std::vector<std::string> folders;
    std::string path = "Data/Scenes/";

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
    for (auto& obj : _Objects) {
        auto col = obj->GetComponent<ColliderComponent>();

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
                    if ((A.obj->mask & (uint32_t)B.obj->layer) != 0)
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
                            float total = A.col->mass + B.col->mass;

                            //ゼロ除算をなくすため
                            if (total == 0.0f)
                            {
                                weightA = weightB = 0.5f;
                            }
                            else
                            {
                                weightA = B.col->mass / total;
                                weightB = A.col->mass / total;
                            }
                        }

                        // 2. 座標の押し戻し（めり込み解消）
                        DirectX::XMVECTOR sepA = DirectX::XMVectorScale(manifold.normal, -manifold.depth * weightA);
                        DirectX::XMVECTOR sepB = DirectX::XMVectorScale(manifold.normal, manifold.depth * weightB);

                        // 改善案：オフセットを考慮した位置補正
                        auto ApplyMove = [](std::shared_ptr<GameObject> obj, std::shared_ptr<ColliderComponent> col, DirectX::XMVECTOR sep) 
                            {
                                // 修正：const で受け取るか、一度変数に格納して左辺値にする
                                const DirectX::XMFLOAT4X4& worldMatrix = obj->transform->GetWorld();
                                DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&worldMatrix);

                            // オフセットをワールド変換
                            DirectX::XMVECTOR offsetVec = DirectX::XMLoadFloat3(&col->offset);
                            DirectX::XMVECTOR rotatedOffset = DirectX::XMVector3TransformNormal(offsetVec, world);

                            // オフセット分を引いた位置を求めることで、「コライダーの中心」ではなく
                            // 「GameObjectのピボット」を適切に動かす
                            DirectX::XMFLOAT3 sepFloat;
                            DirectX::XMStoreFloat3(&sepFloat, sep);

                            obj->transform->localPosition.x += sepFloat.x;
                            obj->transform->localPosition.y += sepFloat.y;
                            obj->transform->localPosition.z += sepFloat.z;

                            obj->transform->MarkDirty();
                            };
                        ApplyMove(A.obj, A.col, sepA);
                        ApplyMove(B.obj, B.col, sepB);

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

}

// ヒストリー（階層）GUIの儀式
void GameObjectManager::DrawHierarchyGui()
{

    // --- Create UI ---
    static char createName[64] = "GameObject";
    ImGui::Text("Create GameObject");
    ImGui::InputText("##CreateName", createName, IM_ARRAYSIZE(createName));
    if (ImGui::Button("Create")) _SelectedObject = Create(createName);

    ImGui::Separator();

    // --- プレハブ管理 ---
    if (ImGui::CollapsingHeader("Prefabs", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Refresh")) RefreshPrefabList();
        static char newPrefabName[64] = "";
        if (!_PrefabFiles.empty()) {
            const char* currentItem = _PrefabFiles[_SelectedPrefabIndex].c_str();
            if (ImGui::BeginCombo("Select Prefab", currentItem)) {
                for (int n = 0; n < _PrefabFiles.size(); n++) {
                    if (ImGui::Selectable(_PrefabFiles[n].c_str(), _SelectedPrefabIndex == n)) _SelectedPrefabIndex = n;
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Instantiate (Create from Prefab)")) _SelectedObject = Instantiate(_PrefabFiles[_SelectedPrefabIndex]);
        }
        if (_SelectedObject) {
            ImGui::InputText("New Prefab Name", newPrefabName, IM_ARRAYSIZE(newPrefabName));
            if (ImGui::Button("Save Selected as Prefab")) {
                if (strlen(newPrefabName) > 0) { SaveAsPrefab(_SelectedObject, newPrefabName); RefreshPrefabList(); memset(newPrefabName, 0, sizeof(newPrefabName)); }
            }
        }
    }

    ImGui::Separator();

    // --- オブジェクト一覧 ---
    GameObject* objectToDelete = nullptr;
    ImGui::BeginChild("HierarchyRegion", ImVec2(0, 0), true);

    for (auto& obj : _Objects) {
        if (obj->transform->parent.expired()) DrawHierarchyNode(obj, &objectToDelete);
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT")) {
            std::shared_ptr<GameObject> draggedObj = *(std::shared_ptr<GameObject>*)payload->Data;
            draggedObj->SetParent(nullptr);
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::EndChild();

    // 削除の確定
    if (objectToDelete) {
        if (_SelectedObject && _SelectedObject.get() == objectToDelete) _SelectedObject = nullptr;
        auto it = std::find_if(_Objects.begin(), _Objects.end(), [&](const std::shared_ptr<GameObject>& ptr) { return ptr.get() == objectToDelete; });
        if (it != _Objects.end()) {
            auto targetObj = *it;
            for (auto& childTrans : targetObj->transform->children) {
                if (auto childGO = childTrans->gameObject.lock()) childGO->SetParent(nullptr);
            }
            if (_SelectedObject == targetObj) _SelectedObject = nullptr;
            _Objects.erase(it);
        }
    }

}

void GameObjectManager::DrawInspectorGui()
{

    ImGui::SeparatorText(_SelectedObject->GetName().c_str());
    _SelectedObject->Gui();

    auto cameraComp = _CameraManager->GetActiveCamera();
    if (cameraComp) {
        ImGuizmo::BeginFrame();
        DirectX::XMFLOAT4X4 view = cameraComp->GetView();
        DirectX::XMFLOAT4X4 proj = cameraComp->GetProjection();
        _SelectedObject->DrawGizmo(&view._11, &proj._11);
    }

}

void GameObjectManager::DrawSceneModals()
{
    // --- 保存の儀式 ---
    if (_SceneState.showSavePopup) {
        // 保存時にも最新のシーン名簿を読み込む
        GetSceneDirectories();
        ImGui::OpenPopup("SaveScenePopup");
        _SceneState.showSavePopup = false;
    }

    if (ImGui::BeginPopupModal("SaveScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::BeginCombo("Select/Input Scene", _SceneState.name)) {
            for (const auto& file : _SceneState.sceneFiles) {
                bool isSelected = (strcmp(_SceneState.name, file.c_str()) == 0);
                if (ImGui::Selectable(file.c_str(), isSelected)) {
                    strcpy_s(_SceneState.name, file.c_str());
                }
            }
            ImGui::EndCombo();
        }

        ImGui::InputText("Filename", _SceneState.name, IM_ARRAYSIZE(_SceneState.name));

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            SaveScene(_SceneState.name);
            GetSceneDirectories(); // 最新の名簿に更新
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    // --- 読み込み ---
    if (_SceneState.showLoadPopup) {
        // 読み込み時にも最新のシーン名簿を読み込む
        GetSceneDirectories();
        ImGui::OpenPopup("LoadScenePopup");
        _SceneState.showLoadPopup = false;
    }

    if (ImGui::BeginPopupModal("LoadScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::BeginCombo("Select Scene", _SceneState.name)) {
            for (const auto& file : _SceneState.sceneFiles) {
                bool isSelected = (strcmp(_SceneState.name, file.c_str()) == 0);
                if (ImGui::Selectable(file.c_str(), isSelected)) {
                    strcpy_s(_SceneState.name, file.c_str());
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (strlen(_SceneState.name) > 0) {
                LoadScene(_SceneState.name);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

// GameObjectManager内の private か、Gui関数の上に追加
void GameObjectManager::DrawHierarchyNode(std::shared_ptr<GameObject> obj, GameObject** toDelete)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (obj == _SelectedObject) flags |= ImGuiTreeNodeFlags_Selected;
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
        _SelectedObject = obj;
    }

    // 右クリックメニュー
    if (ImGui::BeginPopupContextItem()) {
        _SelectedObject = obj;
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

void GameObjectManager::RenderAll(SpriteRenderSystem& spriteSystem, ModelRenderSystem& modelSystem)
{
    auto ctx = GraphicsDeviceManager::Instance().GetContext();
    auto& gdm = GraphicsDeviceManager::Instance();

    // --- 追加: ライト情報の収集 ---
    LightManager lightManager;
    // デフォルトの平行光源設定（必要に応じて）
    DirectionalLight dl;
    dl.direction = {1, 1, 1 };
    lightManager.SetDirectionalLight(dl);

    DecalManager decalManager; // デカール用マネージャー

    for (auto& o : _Objects)
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
                lightManager.AddDirectionalLight(dl);
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


    for (auto& o : _Objects)
    {
        auto md = o->GetComponent<GltfModelCommponennt>();
        if (md)
            modelSystem.PushSubmit(md->GetShaderID(), md);
    }
    RenderContext rc;

    rc.deviceContext = ctx;
    rc.camera = _CameraManager->GetActiveCamera().get();
    rc.renderState = GraphicsDeviceManager::Instance().GetRenderState();
    rc.lights = &lightManager;
    rc.decal = &decalManager;
    rc.baseColorSRV = GraphicsDeviceManager::Instance().GetSceneSRV();
    rc.depthSRV = GraphicsDeviceManager::Instance().GetMainDepthSRV();
    rc.shapeSystem = &GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    rc.cascadeShadowMap = &GraphicsDeviceManager::Instance().GetCascadeShadowMap();

    // ここでレンダー状態をリセット
    ctx->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    ctx->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    ID3D11SamplerState* samplers[] = { rc.renderState->GetSamplerState(SamplerState::LinearWrap) };
    ctx->PSSetSamplers(0, 1, samplers);

    // 1. シーンバッファ（HDR）へ描画開始
    gdm.SetSceneRenderTarget();

    modelSystem.RenderAll(rc);


    if(_ShapeRenderFlg)
    {
        GraphicsDeviceManager::Instance().GetShapeRendererSystem().Render(ctx, rc.camera->GetView(), rc.camera->GetProjection());
    }

    gdm.SetParticleRenderTarget();

    for (auto& obj : _Objects)
    {
        if (!obj) continue;

        auto particle = obj->GetComponent<ParticleComponent>();
        if (particle && particle->IsEnabled())
        {
            _ParticleSystem->PushSubmit(particle.get());
        }
    }

    _ParticleSystem->RenderAll(rc);

    auto* finalRTV = gdm.GetRenderTargetView();

    // 2. ポストレンダーシステムを実行
    gdm.GetPostRenderSystem().Execute(
        rc,
        gdm.GetSceneSRV(), // 描画結果のSRV
        finalRTV           // 最終出力先
    );


    // SpriteRenderSubmit
    for (auto& o : _Objects)
    {
        auto sr = o->GetComponent<SpriteRenderer>();
        if (sr && sr->enabled)
            spriteSystem.Submit(sr, sr->GetSpriteShaderID());
    }

    ctx->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

    spriteSystem.RenderAll(ctx);
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
    _PendingDestroys.push_back(obj);
}

void GameObjectManager::Destroy(std::string objName)
{
    for (auto& obj : _Objects)
    {
        if (obj && obj->GetName() == objName) 
        {
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
    if (!obj || !obj->saveFlg) return;

    // 自分の名前を登録
    listJson["ObjectList"].push_back(obj->nameComponent->name);

    // 子供たちの名前を再帰的に登録
    for (auto& child : obj->transform->children) {
        CollectObjectNames(child ? child->gameObject.lock() : nullptr, listJson);
    }
}

void GameObjectManager::SaveScene(const std::string& sceneName)
{
    std::string basePath = "Data/Scenes/" + sceneName + "/";
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
    for (auto& obj : _Objects) {
        if (!obj->saveFlg) continue;

        std::string filePath = objectsPath + obj->nameComponent->name + ".json";
        nlohmann::json objJson = obj->Save();
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << objJson.dump(4);
        }
    }

    // 2. リスト JSON に「親 → 子」の順で名前を記録
    // 親がいない（ルート）オブジェクトを探して、そこから再帰を開始する
    for (auto& obj : _Objects) {
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
    std::string basePath = "Data/Scenes/" + sceneName + "/";
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

            obj->SetManager(this);

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
    for (auto& obj : _Objects)
    {
        if (!obj) continue;

        // ロード時に一時保存した「親の名前」を取得
        std::string pName = obj->transform->parentNameDuringLoad;

        if (!pName.empty())
        {
            // 名前でマネージャー内のオブジェクトを検索
            auto parentObj = Find(pName);
            if (parentObj && parentObj != obj)
            {
                // 親が見つかったら、Transformの親子付けを実行
                obj->SetParent(parentObj);
            }
        }

        // ロード用の一時情報を残すと、後でResolveHierarchyを呼んだ時に古い親へ戻ってしまう
        obj->transform->parentNameDuringLoad.clear();
    }

    // 全体の親子が確定したので、一度行列を正しく計算させる
    for (auto& obj : _Objects)
    {
        if (obj) obj->UpdateTransform();
    }
}

std::vector<std::string> GameObjectManager::GetSceneDirectories()
{
    _SceneState.sceneFiles.clear();
    std::string rootPath = "Data/Scenes";

    if (std::filesystem::exists(rootPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(rootPath)) {
            if (entry.is_directory()) {
                std::string sceneName = entry.path().filename().string();

                // シーンフォルダの中に「SceneList.json」が存在するかを確認する
                std::filesystem::path jsonPath = entry.path() / "SceneList.json";

                if (std::filesystem::exists(jsonPath)) {
                    _SceneState.sceneFiles.push_back(sceneName);
                }
            }
        }
    }
    return _SceneState.sceneFiles;
}


// フォルダ内のJSONファイルをスキャンする
void GameObjectManager::RefreshPrefabList()
{
    _PrefabFiles.clear();
    if (!std::filesystem::exists(_PrefabDirectory)) {
        std::filesystem::create_directories(_PrefabDirectory);
    }

    for (const auto& entry : std::filesystem::directory_iterator(_PrefabDirectory)) {
        if (entry.path().extension() == ".json") {
            // ファイル名（拡張子なし）をリストに追加
            _PrefabFiles.push_back(entry.path().stem().string());
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
    if (_SelectedObject == obj) _SelectedObject = nullptr;

    // 3. マネージャーのメインリストから自分を削除
    auto it = std::remove(_Objects.begin(), _Objects.end(), obj);
    if (it != _Objects.end()) {
        _Objects.erase(it, _Objects.end());
    }
}

// プレハブ保存
void GameObjectManager::SaveAsPrefab(std::shared_ptr<GameObject> obj, const std::string& prefabName)
{
    if (!obj) return;

    std::filesystem::create_directories(_PrefabDirectory);
    std::string filePath = _PrefabDirectory + prefabName + ".json";

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
    auto objectsSnapshot = _Objects;

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

CameraManager* GameObjectManager::GetCameraManager(){ return _CameraManager.get(); }

// プレハブから生成
std::shared_ptr<GameObject> GameObjectManager::Instantiate(const std::string& prefabName)
{
    std::string filePath = _PrefabDirectory + prefabName + ".json";
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