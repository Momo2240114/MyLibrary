#include "GameObjectManager.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/ContainmentVolumeComponent.h"
#include "Engine/GamePlay/Transform/ModelAttachmentComponent.h"
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
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include "Engine/Debug/TracyProfiler.h"

void GameObjectManager::Initialize()
{
    collisionStepSystem->SetAttackSystem(attackSystem.get());

    particleRenderSystem = std::make_unique<ParticleRenderSystem>();
    particleRenderSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());

    meshVFXRenderSystem = std::make_unique<MeshVFXRenderSystem>();
    meshVFXRenderSystem->Initialize(GraphicsDeviceManager::Instance().GetDevice());

    resourceManager = std::make_unique<ResourceManager>();
    timeManager = std::make_shared<TimeManager>();
    effectManager->Initialize(this);
    RegisterAttackCallbacks();

    cameraManager = std::make_shared<CameraManager>();
    cameraManager->Initialize();
}

void GameObjectManager::Update(float elapsedTime_)
{
    BS_PROFILE_SCOPE("GameObjectManager::Update");

    {
        BS_PROFILE_SCOPE("AttackSystem::BeginFrame");
        attackSystem->BeginFrame();
    }

    {
        BS_PROFILE_SCOPE("TimeManager::Update");
        timeManager->Update(elapsedTime_);
    }

    if (!pendingStarts.empty())
    {
        BS_PROFILE_SCOPE("GameObjectManager::PendingStarts");

        std::vector<std::shared_ptr<GameObject>> startList_ = std::move(pendingStarts);
        pendingStarts.clear();

        {
            BS_PROFILE_SCOPE("PendingStarts::StartComponents");

            for (auto& object_ : startList_)
            {
                if (!object_) continue;

                if (auto particleComponent_ = object_->GetComponent<ParticleComponent>())
                {
                    particleComponents.push_back(particleComponent_);
                }

                {
                    BS_PROFILE_SCOPE("GameObject::CallAllComponentStart");
                    object_->CallAllComponentStart();
                }

                {
                    BS_PROFILE_SCOPE("Director::RegisterActions");

                    if (auto director_ = object_->GetComponent<TransformDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<ColorFilterDirector>())
                    {
                        director_->RegisterActions();
                    }

                    if (auto director_ = object_->GetComponent<CameraDirector>())
                    {
                        director_->RegisterActions();
                    }
                }
            }
        }

        {
            BS_PROFILE_SCOPE("PendingStarts::RegisterSequence");

            for (auto& object_ : startList_)
            {
                if (!object_) continue;

                if (auto sequence_ = object_->GetComponent<ActionSequence>())
                {
                    sequence_->RegisterSequence("");
                }
            }
        }
    }

    {
        BS_PROFILE_SCOPE("TimeManager::PrepareCache");
        timeManager->PrepareCache();
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::SyncFromPhysics");
        collisionStepSystem->SyncFromPhysics(elapsedTime_);
    }

    {
        BS_PROFILE_SCOPE("GameObjectManager::ObjectsUpdate");

        // Animation events can spawn effects while components are updating.
        // Keep this frame's iteration stable when Create() grows objects.
        const std::vector<std::shared_ptr<GameObject>> updateObjects_ = objects;
        for (const std::shared_ptr<GameObject>& object_ : updateObjects_)
        {
            if (!object_) continue;

            const float scale_ = timeManager->GetCachedScale(object_->GetLayerBit());

            if (auto movement_ = object_->GetComponent<CharacterMovementComponent>())
            {
                movement_->SetUnscaledDeltaTime(elapsedTime_);
            }

            object_->Update(elapsedTime_ * scale_);
        }
    }

    {
        BS_PROFILE_SCOPE("ModelAttachmentComponent::SyncAttachment");

        for (auto& attachment_ : FindComponents<ModelAttachmentComponent>())
        {
            if (attachment_ && attachment_->IsEnabled())
            {
                attachment_->SyncAttachment();
            }
        }
    }

    {
        BS_PROFILE_SCOPE("ContainmentVolumeComponent::ApplyContainment");

        for (auto& containment_ : FindComponents<ContainmentVolumeComponent>())
        {
            if (containment_ && containment_->IsEnabled())
            {
                containment_->ApplyContainment();
            }
        }
    }

    {
        BS_PROFILE_SCOPE("CollisionStepSystem::Step");
        collisionStepSystem->Step(elapsedTime_);
    }

    {
        BS_PROFILE_SCOPE("GameObjectManager::ObjectsLateUpdate");

        // Animation events can spawn effects while components are updating.
        // Keep this frame's iteration stable when Create() grows objects.
        const std::vector<std::shared_ptr<GameObject>> updateObjects_ = objects;
        for (const std::shared_ptr<GameObject>& object_ : updateObjects_)
        {
            if (!object_) continue;

            const float scale_ = timeManager->GetCachedScale(object_->GetLayerBit());
            object_->LateUpdate(elapsedTime_ * scale_);
        }
    }

    {
        BS_PROFILE_SCOPE("GameObjectManager::UpdateTransform");

        for (auto& object_ : objects)
        {
            if (!object_) continue;
            object_->UpdateTransform();
        }
    }

    {
        BS_PROFILE_SCOPE("EffectManager::Update");
        effectManager->Update(elapsedTime_);
    }

    if (!pendingDestroys.empty())
    {
        BS_PROFILE_SCOPE("GameObjectManager::PendingDestroys");

        std::vector<std::shared_ptr<GameObject>> currentDestroys_ = std::move(pendingDestroys);
        pendingDestroys.clear();

        for (auto& object_ : currentDestroys_)
        {
            ApplyDestroys(object_);
        }
    }

    {
        BS_PROFILE_SCOPE("ParticleComponents::EraseExpired");

        particleComponents.erase(
            std::remove_if(
                particleComponents.begin(),
                particleComponents.end(),
                [](const std::weak_ptr<ParticleComponent>& component_)
                {
                    return component_.expired();
                }),
            particleComponents.end()
        );
    }

    {
        BS_PROFILE_SCOPE("CameraManager::Update");
        cameraManager->Update(elapsedTime_, objects);
    }

    {
        BS_PROFILE_SCOPE("AudioManager::UpdateListenerFromCamera");
        AudioManager::Instance().UpdateListenerFromCamera(
            cameraManager->GetActiveCamera().get(),
            elapsedTime_);
    }
}

void GameObjectManager::Clear()
{
    GraphicsDeviceManager::Instance().ResetFilter();

    selectedObject = nullptr;

    collisionStepSystem->Clear();
    attackSystem->Clear();
    effectManager->Clear();

    objects.clear();
    pendingStarts.clear();
    pendingDestroys.clear();
    particleComponents.clear();

    // Scene objects were removed, so cached effect pool objects must be
    // recreated and registered with this manager before the next scene runs.
    effectManager->RebuildRuntimePools();
}

void GameObjectManager::AddObject(std::shared_ptr<GameObject> object_)
{
    if (!object_) return;

    for (const std::shared_ptr<GameObject>& existing_ : objects)
    {
        if (existing_ == object_) return;
        if (existing_ && existing_->saveFlg && object_->saveFlg &&
            existing_->GetName() == object_->GetName())
        {
            return;
        }
    }

    object_->SetManager(this);
    for (const std::shared_ptr<ColliderComponent>& collider_ :
        object_->GetComponents<ColliderComponent>())
    {
        if (collider_) collisionStepSystem->RegisterCollider(collider_);
    }
    objects.push_back(object_);
    pendingStarts.push_back(object_);
}

void GameObjectManager::DetachObject(const std::shared_ptr<GameObject>& object_)
{
    if (!object_) return;

    for (const std::shared_ptr<ColliderComponent>& collider_ :
        object_->GetComponents<ColliderComponent>())
    {
        if (collider_) collisionStepSystem->UnregisterCollider(collider_.get());
    }

    if (selectedObject == object_) selectedObject = nullptr;
    objects.erase(
        std::remove(objects.begin(), objects.end(), object_),
        objects.end());
    pendingStarts.erase(
        std::remove(pendingStarts.begin(), pendingStarts.end(), object_),
        pendingStarts.end());
    pendingDestroys.erase(
        std::remove(pendingDestroys.begin(), pendingDestroys.end(), object_),
        pendingDestroys.end());

    const std::shared_ptr<ParticleComponent> particle_ =
        object_->GetComponent<ParticleComponent>();
    particleComponents.erase(
        std::remove_if(
            particleComponents.begin(),
            particleComponents.end(),
            [&particle_](const std::weak_ptr<ParticleComponent>& component_)
            {
                return component_.expired() || component_.lock() == particle_;
            }),
        particleComponents.end());
}
void GameObjectManager::RemoveObject(const std::string& name_)
{
    for (const std::shared_ptr<GameObject>& object_ : objects)
    {
        if (!object_ || object_->GetName() != name_) continue;
        for (const std::shared_ptr<ColliderComponent>& collider_ :
            object_->GetComponents<ColliderComponent>())
        {
            collisionStepSystem->UnregisterCollider(collider_.get());
        }
    }

    const auto iterator_ = std::remove_if(
        objects.begin(),
        objects.end(),
        [&name_](const std::shared_ptr<GameObject>& object_)
        {
            return object_ && object_->GetName() == name_;
        });

    if (iterator_ == objects.end()) return;
    if (selectedObject && selectedObject->GetName() == name_) selectedObject = nullptr;
    objects.erase(iterator_, objects.end());
}

std::vector<std::string> GetSceneDirectories()
{
    std::vector<std::string> folders_;
    std::string path_ = "Data/Scenes/";

    if (!std::filesystem::exists(path_)) return folders_;

    for (const auto& entry_ : std::filesystem::directory_iterator(path_)) {
        if (entry_.is_directory()) {
            folders_.push_back(entry_.path().filename().string());
        }
    }
    return folders_;
}

void GameObjectManager::Gui()
{
}

void GameObjectManager::DrawHierarchyGui()
{
    static char createName_[64] = "GameObject";

    ImGui::Text(IM_U8("ゲームオブジェクト作成"));
    ImGui::InputText("##GameObjectManager_CreateNameInput", createName_, IM_ARRAYSIZE(createName_));

    if (ImGui::Button(IM_U8("作成##GameObjectManager_CreateObjectButton")))
    {
        selectedObject = Create(createName_);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader(IM_U8("プレハブ##GameObjectManager_PrefabsHeader"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button(IM_U8("更新##GameObjectManager_RefreshPrefabListButton")))
        {
            RefreshPrefabList();
        }

        static char newPrefabName_[64] = "";

        if (!prefabFiles.empty())
        {
            const char* currentItem_ = prefabFiles[selectedPrefabIndex].c_str();

            if (ImGui::BeginCombo(IM_U8("プレハブ選択##GameObjectManager_SelectPrefabCombo"), currentItem_))
            {
                for (int index_ = 0; index_ < prefabFiles.size(); index_++)
                {
                    if (ImGui::Selectable(prefabFiles[index_].c_str(), selectedPrefabIndex == index_))
                    {
                        selectedPrefabIndex = index_;
                    }
                }

                ImGui::EndCombo();
            }

            if (ImGui::Button(IM_U8("プレハブから作成##GameObjectManager_InstantiatePrefabButton")))
            {
                selectedObject = Instantiate(prefabFiles[selectedPrefabIndex]);
            }
        }

        if (selectedObject)
        {
            ImGui::InputText(IM_U8("新規プレハブ名##GameObjectManager_NewPrefabNameInput"), newPrefabName_, IM_ARRAYSIZE(newPrefabName_));

            if (ImGui::Button(IM_U8("選択中のオブジェクトをプレハブとして保存##GameObjectManager_SaveSelectedAsPrefabButton")))
            {
                if (strlen(newPrefabName_) > 0)
                {
                    SaveAsPrefab(selectedObject, newPrefabName_);
                    RefreshPrefabList();
                    memset(newPrefabName_, 0, sizeof(newPrefabName_));
                }
            }
        }
    }

    ImGui::Separator();

    GameObject* objectToDelete_ = nullptr;

    // 内部IDなので英語のまま
    ImGui::BeginChild("HierarchyRegion", ImVec2(0, 0), true);

    for (auto& object_ : objects)
    {
        if (!object_ || !object_->saveFlg) continue;
        if (object_->transform->parent.expired())
        {
            DrawHierarchyNode(object_, &objectToDelete_);
        }
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            std::shared_ptr<GameObject> draggedObject_ = *(std::shared_ptr<GameObject>*)payload_->Data;
            draggedObject_->SetParent(nullptr);
        }

        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();

    if (objectToDelete_)
    {
        if (selectedObject && selectedObject.get() == objectToDelete_)
        {
            selectedObject = nullptr;
        }

        auto iterator_ = std::find_if(
            objects.begin(),
            objects.end(),
            [&](const std::shared_ptr<GameObject>& object_)
            {
                return object_.get() == objectToDelete_;
            });

        if (iterator_ != objects.end())
        {
            auto targetObject_ = *iterator_;

            for (auto& childTransform_ : targetObject_->transform->children)
            {
                if (auto childObject_ = childTransform_->gameObject.lock())
                {
                    childObject_->SetParent(nullptr);
                }
            }

            if (selectedObject == targetObject_)
            {
                selectedObject = nullptr;
            }

            objects.erase(iterator_);
        }
    }
}

void GameObjectManager::DrawInspectorGui()
{
    if (!selectedObject) return;

    ImGui::SeparatorText(selectedObject->GetName().c_str());
    selectedObject->Gui();

    auto cameraComponent_ = cameraManager->GetActiveCamera();
    if (cameraComponent_) {
        ImGuizmo::BeginFrame();
        DirectX::XMFLOAT4X4 view_ = cameraComponent_->GetView();
        DirectX::XMFLOAT4X4 projection_ = cameraComponent_->GetProjection();
        selectedObject->DrawGizmo(&view_._11, &projection_._11);
    }
}
void GameObjectManager::DrawSceneModals()
{
    if (sceneState.showSavePopup)
    {
        GetSceneDirectories();

        // Popup IDなので内部IDとして英語のまま維持
        ImGui::OpenPopup("SaveScenePopup");

        sceneState.showSavePopup = false;
    }

    if (ImGui::BeginPopupModal("SaveScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::BeginCombo(IM_U8("シーン選択 / 入力##SceneModal_SelectInputSceneCombo"), sceneState.name))
        {
            for (const auto& file_ : sceneState.sceneFiles)
            {
                bool isSelected_ = (strcmp(sceneState.name, file_.c_str()) == 0);

                if (ImGui::Selectable(file_.c_str(), isSelected_))
                {
                    strcpy_s(sceneState.name, file_.c_str());
                }
            }

            ImGui::EndCombo();
        }

        ImGui::InputText(IM_U8("ファイル名##SceneModal_FilenameInput"), sceneState.name, IM_ARRAYSIZE(sceneState.name));

        if (ImGui::Button(IM_U8("保存##SceneModal_SaveSceneButton"), ImVec2(120, 0)))
        {
            SaveScene(sceneState.name);
            GetSceneDirectories();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(IM_U8("キャンセル##SceneModal_SaveCancelButton"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (sceneState.showLoadPopup)
    {
        GetSceneDirectories();

        // Popup IDなので内部IDとして英語のまま維持
        ImGui::OpenPopup("LoadScenePopup");

        sceneState.showLoadPopup = false;
    }

    if (ImGui::BeginPopupModal("LoadScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::BeginCombo(IM_U8("シーン選択##SceneModal_SelectSceneCombo"), sceneState.name))
        {
            for (const auto& file_ : sceneState.sceneFiles)
            {
                bool isSelected_ = (strcmp(sceneState.name, file_.c_str()) == 0);

                if (ImGui::Selectable(file_.c_str(), isSelected_))
                {
                    strcpy_s(sceneState.name, file_.c_str());
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::Button(IM_U8("読み込み##SceneModal_LoadSceneButton"), ImVec2(120, 0)))
        {
            if (strlen(sceneState.name) > 0)
            {
                LoadScene(sceneState.name);
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button(IM_U8("キャンセル##SceneModal_LoadCancelButton"), ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
void GameObjectManager::DrawHierarchyNode(std::shared_ptr<GameObject> object_, GameObject** objectToDelete_)
{
    ImGuiTreeNodeFlags flags_ =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth;

    if (object_ == selectedObject)
    {
        flags_ |= ImGuiTreeNodeFlags_Selected;
    }

    if (object_->transform->children.empty())
    {
        flags_ |= ImGuiTreeNodeFlags_Leaf;
    }

    bool opened_ = ImGui::TreeNodeEx(
        (void*)(uintptr_t)object_.get(),
        flags_,
        "%s",
        object_->GetName().c_str());

    static std::weak_ptr<GameObject> pressedObject_;
    static ImVec2 pressedMousePos_ = ImVec2(0.0f, 0.0f);
    static double pressedTime_ = 0.0;

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        pressedObject_ = object_;
        pressedMousePos_ = ImGui::GetIO().MousePos;
        pressedTime_ = ImGui::GetTime();
    }

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("DND_GAMEOBJECT", &object_, sizeof(std::shared_ptr<GameObject>));

        ImGui::Text(IM_U8("移動中: %s"), object_->GetName().c_str());

        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload("DND_GAMEOBJECT"))
        {
            std::shared_ptr<GameObject> draggedObject_ = *(std::shared_ptr<GameObject>*)payload_->Data;

            if (draggedObject_ != object_)
            {
                draggedObject_->SetParent(object_);
            }
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        auto pressedObjectLock_ = pressedObject_.lock();

        ImVec2 releaseMousePos_ = ImGui::GetIO().MousePos;
        float deltaX_ = releaseMousePos_.x - pressedMousePos_.x;
        float deltaY_ = releaseMousePos_.y - pressedMousePos_.y;

        float dragThreshold_ = 6.0f;

        bool isSameObject_ = pressedObjectLock_ == object_;
        bool isShortPress_ = (ImGui::GetTime() - pressedTime_) < 0.25;
        bool isNotDragged_ = (deltaX_ * deltaX_ + deltaY_ * deltaY_) < (dragThreshold_ * dragThreshold_);

        if (isSameObject_ && isShortPress_ && isNotDragged_)
        {
            selectedObject = object_;
        }

        pressedObject_.reset();
    }

    if (ImGui::BeginPopupContextItem())
    {
        selectedObject = object_;

        if (ImGui::MenuItem(IM_U8("ゲームオブジェクトを削除##GameObjectManager_DeleteGameObjectMenuItem")))
        {
            Destroy(object_->GetName());
        }

        ImGui::EndPopup();
    }

    if (opened_)
    {
        for (auto& childTransform_ : object_->transform->children)
        {
            if (auto childObject_ = childTransform_->gameObject.lock())
            {
                if (!childObject_->saveFlg) continue;
                DrawHierarchyNode(childObject_, objectToDelete_);
            }
        }

        ImGui::TreePop();
    }
}

void GameObjectManager::RenderAll(SpriteRenderSystem& spriteSystem_, ModelRenderSystem& modelSystem_)
{
    auto deviceContext_ = GraphicsDeviceManager::Instance().GetContext();
    auto& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();

    // --- 追加: ライト情報の収集 ---
    LightManager lightManager_;
    // デフォルトの平行光源設定（必要に応じて）
    DirectionalLight directionalLight_;
    directionalLight_.direction = {1, 1, 1 };
    lightManager_.SetDirectionalLight(directionalLight_);

    DecalManager decalManager_; // デカール用マネージャー

    for (auto& object_ : objects)
    {
        auto lightComponent_ = object_->GetComponent<LightComponent>();
        if (lightComponent_ && lightComponent_->IsEnabled())
        {
            // A. 平行光源
            if (lightComponent_->type == LightComponent::Type::Directional)
            {
                DirectionalLight directionalLight_;
                directionalLight_.direction = lightComponent_->finalWorldDirection;
                directionalLight_.color = lightComponent_->color;
                directionalLight_.intensity = lightComponent_->intensity;
                lightManager_.AddDirectionalLight(directionalLight_);
            }
            // B. 点光源
            else if (lightComponent_->type == LightComponent::Type::Point)
            {
                PointLight pointLight_;
                pointLight_.position = lightComponent_->finalWorldPosition;
                pointLight_.color = lightComponent_->color;
                pointLight_.intensity = lightComponent_->intensity;
                pointLight_.range = lightComponent_->range;
                lightManager_.AddPointLight(pointLight_);
            }
            // C. スポットライト
            else if (lightComponent_->type == LightComponent::Type::Spot)
            {
                SpotLight spotLight_;
                spotLight_.position = lightComponent_->finalWorldPosition;
                spotLight_.direction = lightComponent_->finalWorldDirection;
                spotLight_.color = lightComponent_->color;
                spotLight_.intensity = lightComponent_->intensity;
                spotLight_.range = lightComponent_->range;
                spotLight_.innerAngle = lightComponent_->innerAngle;
                spotLight_.outerAngle = lightComponent_->outerAngle;
                lightManager_.AddSpotLight(spotLight_);
            }
        }

        auto decalComponent_ = object_->GetComponent<DecalComponent>();

        // デカールの収集
        if (decalComponent_ && decalComponent_->IsEnabled())
        {
            decalComponent_->SubmitDecal(decalManager_);
        }


    }


    for (auto& object_ : objects)
    {
        auto modelComponent_ = object_->GetComponent<GltfModelCommponennt>();
        if (modelComponent_)
            modelSystem_.PushSubmit(modelComponent_->GetShaderID(), modelComponent_);
    }
    RenderContext renderContext_;

    renderContext_.device = GraphicsDeviceManager::Instance().GetDevice();
    renderContext_.deviceContext = deviceContext_;
    renderContext_.camera = cameraManager->GetActiveCamera().get();
    renderContext_.renderState = GraphicsDeviceManager::Instance().GetRenderState();
    renderContext_.lights = &lightManager_;
    renderContext_.decal = &decalManager_;
    renderContext_.baseColorSRV = GraphicsDeviceManager::Instance().GetSceneSRV();
    renderContext_.depthSRV = GraphicsDeviceManager::Instance().GetMainDepthSRV();
    renderContext_.shapeSystem = &GraphicsDeviceManager::Instance().GetShapeRendererSystem();
    renderContext_.cascadeShadowMap = &GraphicsDeviceManager::Instance().GetCascadeShadowMap();

    // ここでレンダー状態をリセット
    deviceContext_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    deviceContext_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    deviceContext_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
    ID3D11SamplerState* samplers_[] = { renderContext_.renderState->GetSamplerState(SamplerState::LinearWrap) };
    deviceContext_->PSSetSamplers(0, 1, samplers_);

    // 1. シーンバッファ（HDR）へ描画開始
    graphicsDeviceManager_.SetSceneRenderTarget();

    modelSystem_.RenderAll(renderContext_);


    if(shapeRenderEnabled)
    {
        GraphicsDeviceManager::Instance().GetShapeRendererSystem().Render(deviceContext_, renderContext_.camera->GetView(), renderContext_.camera->GetProjection());
        deviceContext_->OMSetDepthStencilState(
            renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    }

    graphicsDeviceManager_.SetParticleRenderTarget();

    for (auto& obj : objects)
    {
        if (!obj) continue;
        for (const auto& meshVFX_ : obj->GetComponents<MeshVFXComponent>())
        {
            if (meshVFX_ && meshVFX_->IsEnabled())
            {
                meshVFXRenderSystem->PushSubmit(meshVFX_.get());
            }
        }
    }
    meshVFXRenderSystem->RenderAll(renderContext_);

    // MeshVFX can switch between temporary and distortion render targets.
    // Rebind the scene color target so particles are included in the post-process input.
    graphicsDeviceManager_.SetParticleRenderTarget();

    for (auto& obj : objects)
    {
        if (!obj) continue;

        auto particle = obj->GetComponent<ParticleComponent>();
        if (particle && particle->IsEnabled())
        {
            particleRenderSystem->PushSubmit(particle.get());
        }
    }

    particleRenderSystem->RenderAll(renderContext_);

    auto* finalRTV = graphicsDeviceManager_.GetRenderTargetView();

    // 2. ポストレンダーシステムを実行
    graphicsDeviceManager_.GetPostRenderSystem().Execute(
        renderContext_,
        graphicsDeviceManager_.GetSceneSRV(), // 描画結果のSRV
        finalRTV           // 最終出力先
    );


    // SpriteRenderSubmit
    for (auto& object_ : objects)
    {
        auto sr = object_->GetComponent<SpriteRenderer>();
        if (sr && sr->enabled)
            spriteSystem_.Submit(sr, sr->GetSpriteShaderID());
    }

    deviceContext_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

    spriteSystem_.RenderAll(deviceContext_);
}



void GameObjectManager::Destroy(std::shared_ptr<GameObject> object_)
{
    if (!object_) return;
    // 即座に消さず、予約リストに入れる
    pendingDestroys.push_back(object_);
}

void GameObjectManager::Destroy(std::string objectName_)
{
    for (auto& object_ : objects)
    {
        if (object_ && object_->GetName() == objectName_)
        {
            Destroy(object_); // 上のDestroyを呼ぶ
            return; // 1つ見つけたら終了（全部消すならreturnしない）
        }
    }
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

void GameObjectManager::SaveScene(const std::string& sceneName_)
{
    std::string basePath = "Data/Scenes/" + sceneName_ + "/";
    std::string objectsPath = basePath + "Objects/";
    std::filesystem::create_directories(objectsPath);

    nlohmann::json listJson;
    listJson["SceneName"] = sceneName_;
    listJson["ObjectList"] = nlohmann::json::array(); // 配列を初期化

    // --- グラフィックス設定（そのまま） ---
    auto& params = GraphicsDeviceManager::Instance().GetBloomParams();
    listJson["GraphicsSettings"]["Bloom"] = {
        {"enabled", params.enabled},
        {"threshold", params.threshold},
        {"intensity", params.intensity},
        {"kawaseScale", params.kawaseScale},
        {"blurPasses", params.blurPasses},
        {"toneMappingEnabled", params.toneMappingEnabled},
        {"exposure", params.exposure}
    };

    if (modelRenderSystem)
    {
        modelRenderSystem->SerializeGraphicsSettings(listJson["GraphicsSettings"]);
    }

    // 1. 各オブジェクトの個別ファイルを保存
    // (ここは順不同でOKなので、まずは全ファイルを書き出す)
    for (auto& obj : objects) {
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

    printf("Scene '%s' saved with ordered SceneList.json\n", sceneName_.c_str());
}

void GameObjectManager::LoadScene(const std::string& sceneName_)
{

    Clear(); // 現在のシーンをクリア
    std::string basePath = "Data/Scenes/" + sceneName_ + "/";
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
    if (listJson.contains("GraphicsSettings")) {
        auto& graphicsJson = listJson["GraphicsSettings"];

        if (graphicsJson.contains("Bloom")) {
            auto& bloomJson = graphicsJson["Bloom"];
            auto& params = GraphicsDeviceManager::Instance().GetBloomParams();

            params.enabled = bloomJson.value("enabled", true);
            params.threshold = bloomJson.value("threshold", 1.0f);
            params.intensity = bloomJson.value("intensity", 1.0f);
            params.kawaseScale = bloomJson.value("kawaseScale", 1.0f);
            params.blurPasses = bloomJson.value("blurPasses", 4);
            params.toneMappingEnabled = bloomJson.value("toneMappingEnabled", true);
            params.exposure = bloomJson.value("exposure", 1.0f);
        }

        if (modelRenderSystem)
        {
            modelRenderSystem->DeserializeGraphicsSettings(graphicsJson);
        }
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

    printf("Scene '%s' loaded from SceneList.json\n", sceneName_.c_str());
}

void GameObjectManager::ResolveHierarchy()
{
    for (auto& obj : objects)
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
    for (auto& obj : objects)
    {
        if (obj) obj->UpdateTransform();
    }
}

std::vector<std::string> GameObjectManager::GetSceneDirectories()
{
    sceneState.sceneFiles.clear();
    std::string rootPath = "Data/Scenes";

    if (std::filesystem::exists(rootPath)) {
        for (const auto& entry : std::filesystem::directory_iterator(rootPath)) {
            if (entry.is_directory()) {
                std::string sceneName = entry.path().filename().string();

                // シーンフォルダの中に「SceneList.json」が存在するかを確認する
                std::filesystem::path jsonPath = entry.path() / "SceneList.json";

                if (std::filesystem::exists(jsonPath)) {
                    sceneState.sceneFiles.push_back(sceneName);
                }
            }
        }
    }
    return sceneState.sceneFiles;
}


// フォルダ内のJSONファイルをスキャンする
void GameObjectManager::RefreshPrefabList()
{
    prefabFiles.clear();
    if (!std::filesystem::exists(prefabDirectory)) {
        std::filesystem::create_directories(prefabDirectory);
    }

    for (const auto& entry : std::filesystem::directory_iterator(prefabDirectory)) {
        if (entry.path().extension() == ".json") {
            // ファイル名（拡張子なし）をリストに追加
            prefabFiles.push_back(entry.path().stem().string());
        }
    }
}

void GameObjectManager::ApplyDestroys(std::shared_ptr<GameObject> object_)
{
    if (!object_) return;
    for (const std::shared_ptr<ColliderComponent>& collider_ :
        object_->GetComponents<ColliderComponent>())
    {
        collisionStepSystem->UnregisterCollider(collider_.get());
    }

    // 1. 子供を先に全て削除する（再帰）
    auto childrenCopy = object_->transform->children;
    for (auto& childTrans : childrenCopy)
    {
        if (auto childGO = childTrans->gameObject.lock())
        {
            ApplyDestroys(Find(childTrans->gameObject.lock()->GetName()));
        }
    }

    // 2. 特殊な参照を外す
    if (selectedObject == object_) selectedObject = nullptr;

    // 3. マネージャーのメインリストから自分を削除
    auto it = std::remove(objects.begin(), objects.end(), object_);
    if (it != objects.end()) {
        objects.erase(it, objects.end());
    }
}

// プレハブ保存
void GameObjectManager::SaveAsPrefab(std::shared_ptr<GameObject> object_, const std::string& prefabName_)
{
    if (!object_) return;

    std::filesystem::create_directories(prefabDirectory);
    std::string filePath = prefabDirectory + prefabName_ + ".json";

    nlohmann::json j = object_->Save();
    std::ofstream file(filePath);
    if (file.is_open()) {
        file << j.dump(4);
        RefreshPrefabList(); // リストを更新
    }
}


void GameObjectManager::InitializeAllComponents()
{
    //// ★ 現在存在しているオブジェクトをスナップショット
    //auto objectsSnapshot = objects;

    for (auto& obj : objects)
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
    for (auto& obj : objects)
    {
        if (!obj) continue;

        if (auto seq = obj->GetComponent<ActionSequence>()) {
            obj->CallAllComponentStart();
        }
    }
}

CameraManager* GameObjectManager::GetCameraManager(){ return cameraManager.get(); }

// プレハブから生成
std::shared_ptr<GameObject> GameObjectManager::Instantiate(const std::string& prefabName_)
{
    std::string filePath = prefabDirectory + prefabName_ + ".json";
    std::ifstream file(filePath);
    if (!file.is_open()) {
        printf("Error: Prefab '%s' not found!\n", prefabName_.c_str());
        return nullptr;
    }

    nlohmann::json prefabJson;
    file >> prefabJson;

    // 1. 重複しないユニークな名前を生成する
    std::string uniqueName = MakeUniqueName(prefabName_);

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

void GameObjectManager::RegisterAttackCallbacks()
{
    attackSystem->AddHitCallback(
        [this](const AttackHitEvent& event_)
        {
            if (timeManager && event_.hitStopDuration > 0.0f && event_.hitStopLayerMask != 0u)
            {
                timeManager->TriggerHitStop(
                    event_.hitStopLayerMask,
                    event_.hitStopDuration,
                    event_.hitStopScale);
            }

            if (effectManager && event_.hasHitContact)
            {
                effectManager->SpawnForHit(event_);
            }

            if (!event_.hitSoundPath.empty())
            {
                AudioManager& audioManager_ = AudioManager::Instance();
                audioManager_.AddAudioFromFile(event_.hitSoundPath, false);
                audioManager_.SetVolumeByPath(event_.hitSoundPath, event_.hitSoundVolume);
                audioManager_.SetMuffledByPath(
                    event_.hitSoundPath,
                    event_.hitSoundMuffled,
                    event_.hitSoundMuffledCutoff);
                audioManager_.PlaySEByPath(event_.hitSoundPath);
            }
        });
}



