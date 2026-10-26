#include "PrecompiledHeader.h"

#include "PrefabWorkspaceWindow.h"

#include "Editor/AssetBrowserPayload.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/LightManager.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Decal/DecalManager.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <algorithm>
#include <filesystem>
#include <imgui_internal.h>
#include <vector>

PrefabWorkspaceWindow::PrefabWorkspaceWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("Prefab Workspace", manager_),
    previewGameObjectManager(std::make_unique<GameObjectManager>()),
    assetBrowser(previewGameObjectManager.get())
{
    isVisible = false;
    previewGameObjectManager->Initialize();
    gameObjectManager = previewGameObjectManager.get();
    assetBrowser.SetPrefabOpenHandler(
        [this](const std::filesystem::path& prefabPath_)
        {
            LoadPrefabForEdit(prefabPath_.stem().string());
        });
}

void PrefabWorkspaceWindow::Draw()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::Draw");

    EnsurePreviewScene();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    DrawToolbar();

    const float splitterSize_ = 5.0f;
    const float minimumPanelWidth_ = 220.0f;
    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    const float mainHeight_ = showAssetBrowser
        ? (std::max)(120.0f, available_.y - assetBrowserHeight - splitterSize_)
        : available_.y;
    const float maximumHierarchyWidth_ =
        (std::max)(minimumPanelWidth_, available_.x - minimumPanelWidth_ * 2.0f);
    hierarchyPanelWidth = std::clamp(hierarchyPanelWidth, minimumPanelWidth_, maximumHierarchyWidth_);
    const float maximumInspectorWidth_ =
        (std::max)(minimumPanelWidth_, available_.x - hierarchyPanelWidth - splitterSize_ * 2.0f - minimumPanelWidth_);
    inspectorPanelWidth = std::clamp(inspectorPanelWidth, minimumPanelWidth_, maximumInspectorWidth_);
    const float previewWidth_ =
        (std::max)(minimumPanelWidth_, available_.x - hierarchyPanelWidth - inspectorPanelWidth - splitterSize_ * 2.0f);

    ImGui::BeginChild("PrefabWorkspaceHierarchyPanel", ImVec2(hierarchyPanelWidth, mainHeight_), true);
    DrawHierarchyPanel();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##PrefabWorkspaceHierarchySplitter", ImVec2(splitterSize_, mainHeight_));
    if (ImGui::IsItemActive()) hierarchyPanelWidth += ImGui::GetIO().MouseDelta.x;
    ImGui::SameLine();

    ImGui::BeginChild("PrefabWorkspacePreviewPanel", ImVec2(previewWidth_, mainHeight_), true);
    DrawPreviewPanel();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##PrefabWorkspaceInspectorSplitter", ImVec2(splitterSize_, mainHeight_));
    if (ImGui::IsItemActive()) inspectorPanelWidth -= ImGui::GetIO().MouseDelta.x;
    ImGui::SameLine();

    ImGui::BeginChild("PrefabWorkspaceInspectorPanel", ImVec2(0.0f, mainHeight_), true);
    DrawInspectorPanel();
    ImGui::EndChild();

    if (showAssetBrowser)
    {
        ImGui::InvisibleButton("##PrefabWorkspaceAssetSplitter", ImVec2(-1.0f, splitterSize_));
        if (ImGui::IsItemActive()) assetBrowserHeight -= ImGui::GetIO().MouseDelta.y;
        assetBrowserHeight = std::clamp(assetBrowserHeight, 160.0f, (std::max)(160.0f, available_.y - 120.0f));

        ImGui::BeginChild("PrefabWorkspaceAssetBrowserPanel", ImVec2(0.0f, 0.0f), true);
        DrawAssetBrowserPanel();
        ImGui::EndChild();
    }

    EndEditorWindow();
}

void PrefabWorkspaceWindow::DrawToolbar()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::DrawToolbar");

    if (!ImGui::BeginMenuBar()) return;

    const std::vector<std::string>& prefabFiles_ = gameObjectManager->GetPrefabFiles();
    if (prefabFiles_.empty())
    {
        gameObjectManager->RefreshPrefabList();
    }

    const std::vector<std::string>& refreshedPrefabFiles_ = gameObjectManager->GetPrefabFiles();
    ImGui::TextUnformatted("Prefab");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220.0f);
    if (!refreshedPrefabFiles_.empty())
    {
        int selectedIndex_ = gameObjectManager->GetSelectedPrefabIndex();
        selectedIndex_ = std::clamp(selectedIndex_, 0, static_cast<int>(refreshedPrefabFiles_.size()) - 1);
        const char* currentPrefabName_ = refreshedPrefabFiles_[selectedIndex_].c_str();
        if (ImGui::BeginCombo("##PrefabWorkspacePrefabSelect", currentPrefabName_))
        {
            for (int index_ = 0; index_ < static_cast<int>(refreshedPrefabFiles_.size()); ++index_)
            {
                if (ImGui::Selectable(refreshedPrefabFiles_[index_].c_str(), selectedIndex_ == index_))
                {
                    gameObjectManager->SetSelectedPrefabIndex(index_);
                }
            }
            ImGui::EndCombo();
        }
    }
    else
    {
        ImGui::TextDisabled("No Prefab");
    }

    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        LoadSelectedPrefab();
    }
    ImGui::SameLine();
    const bool canSave_ = !editingPrefabName.empty() && !editingRootObject.expired();
    if (!canSave_) ImGui::BeginDisabled();
    if (ImGui::Button("Save"))
    {
        SaveCurrentPrefab();
    }
    if (!canSave_) ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("##PrefabWorkspaceSaveAs", saveAsPrefabName, IM_ARRAYSIZE(saveAsPrefabName));
    ImGui::SameLine();
    if (ImGui::Button("Save As"))
    {
        std::shared_ptr<GameObject> selectedObject_ = gameObjectManager->GetSelectedObject();
        if (selectedObject_ && saveAsPrefabName[0] != '\0')
        {
            editingRootObject = selectedObject_;
            editingPrefabName = saveAsPrefabName;
            SaveCurrentPrefab();
            saveAsPrefabName[0] = '\0';
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(showAssetBrowser ? "Close Assets" : "Open Assets"))
    {
        showAssetBrowser = !showAssetBrowser;
    }

    ImGui::SameLine();
    ImGui::MenuItem("Grid", nullptr, &showGrid);
    ImGui::SameLine();
    ImGui::MenuItem("Free Camera", nullptr, &editorCamera.enablePreviewFreeCamera);
    ImGui::SameLine();
    if (ImGui::MenuItem("Reset Camera"))
    {
        editorCamera.ResetPreviewCamera(previewCamera);
    }

    ImGui::EndMenuBar();
}

void PrefabWorkspaceWindow::DrawHierarchyPanel()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::DrawHierarchyPanel");

    ImGui::TextUnformatted("Hierarchy");
    ImGui::Separator();
    if (!previewGameObjectManager)
    {
        ImGui::TextDisabled("Preview manager is not ready.");
        return;
    }

    // 編集中Prefab専用のManagerだけを表示し、現在のSceneのObjectを混ぜない。
    previewGameObjectManager->DrawHierarchyGui();
}

void PrefabWorkspaceWindow::DrawPreviewPanel()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::DrawPreviewPanel");

    ImGui::TextUnformatted("Prefab Scene View");
    ImGui::Separator();

    ImVec2 previewSize_ = ImGui::GetContentRegionAvail();
    previewSize_.x = (std::max)(1.0f, previewSize_.x);
    previewSize_.y = (std::max)(1.0f, previewSize_.y);

    const bool previewHovered_ =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    editorCamera.UpdatePreviewFreeCamera(
        previewCamera,
        ImGui::GetIO().DeltaTime,
        previewHovered_);

    RenderPreview(previewSize_);

    if (previewTarget.GetShaderResourceView())
    {
        ImGui::Image(reinterpret_cast<void*>(previewTarget.GetShaderResourceView()), previewSize_);
        AcceptPrefabDrop();
    }
    else
    {
        ImGui::Button("Drop Prefab Here", previewSize_);
        AcceptPrefabDrop();
    }

    if (previewHovered_)
    {
        ImGui::SetTooltip("RMB + Mouse: Look / WASD: Move / Q,E: Up Down / Wheel: Speed");
    }
}

void PrefabWorkspaceWindow::DrawInspectorPanel()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::DrawInspectorPanel");

    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();
    if (!previewGameObjectManager)
    {
        ImGui::TextDisabled("Preview manager is not ready.");
        return;
    }

    if (ImGui::BeginTabBar("PrefabWorkspaceInspectorTabs"))
    {
        if (ImGui::BeginTabItem("Object"))
        {
            previewGameObjectManager->DrawInspectorGui();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Camera"))
        {
            editorCamera.Gui();
            if (ImGui::Button("Reset Camera"))
            {
                editorCamera.ResetPreviewCamera(previewCamera);
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void PrefabWorkspaceWindow::DrawAssetBrowserPanel()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::DrawAssetBrowserPanel");

    ImGui::TextUnformatted("Assets");
    ImGui::Separator();
    ImGui::Button("Drop Prefab Here", ImVec2(-1.0f, 0.0f));
    AcceptPrefabDrop();
    assetBrowser.DrawContent();
}

void PrefabWorkspaceWindow::RenderPreview(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::RenderPreview");

    if (!previewGameObjectManager) return;

    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_) return;

    const int width_ = (std::max)(1, static_cast<int>(previewSize_.x));
    const int height_ = (std::max)(1, static_cast<int>(previewSize_.y));
    if (!previewTarget.Resize(device_, width_, height_)) return;

    previewCamera.SetPerspectiveFov(45.0f, static_cast<float>(width_) / static_cast<float>(height_), 0.01f, 1000.0f);

    const float clearColor_[4] = { 0.035f, 0.038f, 0.042f, 1.0f };
    previewTarget.Begin(context_, clearColor_);

    LightManager lightManager_;
    DirectionalLight directionalLight_;
    directionalLight_.direction = { 0.35f, -0.8f, 0.45f };
    directionalLight_.color = { 1.0f, 0.97f, 0.92f };
    directionalLight_.intensity = 5.0f;
    lightManager_.SetDirectionalLight(directionalLight_);

    DecalManager decalManager_;

    RenderContext renderContext_{};
    renderContext_.device = device_;
    renderContext_.deviceContext = context_;
    renderContext_.renderState = graphics_.GetRenderState();
    renderContext_.camera = &previewCamera;
    renderContext_.lights = &lightManager_;
    renderContext_.decal = &decalManager_;
    renderContext_.depthSRV = previewTarget.GetDepthShaderResourceView();
    renderContext_.baseColorSRV = previewTarget.GetShaderResourceView();
    renderContext_.shapeSystem = &graphics_.GetShapeRendererSystem();
    renderContext_.cascadeShadowMap = nullptr;
    renderContext_.screenWidth = static_cast<float>(width_);
    renderContext_.screenHeight = static_cast<float>(height_);

    context_->OMSetBlendState(renderContext_.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    context_->OMSetDepthStencilState(renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    context_->RSSetState(renderContext_.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

    if (showGrid)
    {
        // Prefabの足元とスケール感を確認しやすいよう、Preview専用RTへ先にグリッドを描画する。
        editorPrimitiveRenderer.DrawGrid(
            20,
            1.0f,
            { 0.25f, 0.25f, 0.25f, 1.0f },
            true);
        editorPrimitiveRenderer.Render(
            context_,
            previewCamera.GetView(),
            previewCamera.GetProjection());
        previewTarget.ResetRenderTarget(context_);
    }

    for (auto& object_ : previewGameObjectManager->GetObjects())
    {
        if (!object_) continue;

        // Inspectorで変更された親子Transformを、Prefab専用Previewへ反映する。
        object_->UpdateTransform();

        if (auto model_ = object_->GetComponent<GltfModelCommponennt>())
        {
            previewModelRenderSystem.PushSubmit(model_->GetShaderID(), model_);
        }

        if (auto particle_ = object_->GetComponent<ParticleComponent>())
        {
            previewParticleRenderSystem.PushSubmit(particle_.get());
        }

        for (const std::shared_ptr<MeshVFXComponent>& meshVFX_ :
            object_->GetComponents<MeshVFXComponent>())
        {
            if (meshVFX_ && meshVFX_->IsEnabled())
            {
                previewMeshVFXRenderSystem.PushSubmit(meshVFX_.get());
            }
        }
    }

    previewModelRenderSystem.RenderAll(renderContext_);
    previewTarget.ResetRenderTarget(context_);
    previewMeshVFXRenderSystem.RenderAll(
        renderContext_,
        true,
        previewTarget.GetRenderTargetView(),
        previewTarget.GetDepthStencilView());
    previewTarget.ResetRenderTarget(context_);
    context_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    graphics_.GetShapeRendererSystem().Render(
        context_,
        renderContext_.camera->GetView(),
        renderContext_.camera->GetProjection());
    context_->OMSetDepthStencilState(
        renderContext_.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
    previewParticleRenderSystem.RenderAll(renderContext_);

    previewTarget.End(context_);
    graphics_.SetBackBufferRenderTarget();
}

void PrefabWorkspaceWindow::EnsurePreviewScene()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::EnsurePreviewScene");

    if (isPreviewSceneInitialized) return;

    if (!previewGameObjectManager)
    {
        previewGameObjectManager = std::make_unique<GameObjectManager>();
        previewGameObjectManager->Initialize();
        gameObjectManager = previewGameObjectManager.get();
    }

    auto& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    if (!device_) return;

    previewGameObjectManager->SetModelRenderSystem(&previewModelRenderSystem);
    editorPrimitiveRenderer.Initialize(device_);
    previewModelRenderSystem.Initialize(device_);
    previewParticleRenderSystem.Initialize(device_);
    previewMeshVFXRenderSystem.Initialize(device_);

    // Prefab編集ではScene本体のCameraを使わず、Workspace専用Cameraでプレビューする。
    editorCamera.ResetPreviewCamera(previewCamera);
    previewCamera.SetPerspectiveFov(45.0f, 16.0f / 9.0f, 0.01f, 1000.0f);

    isPreviewSceneInitialized = true;
}

void PrefabWorkspaceWindow::LoadSelectedPrefab()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::LoadSelectedPrefab");

    const std::vector<std::string>& prefabFiles_ = gameObjectManager->GetPrefabFiles();
    if (prefabFiles_.empty()) return;

    const int selectedIndex_ = std::clamp(
        gameObjectManager->GetSelectedPrefabIndex(),
        0,
        static_cast<int>(prefabFiles_.size()) - 1);
    LoadPrefabForEdit(prefabFiles_[selectedIndex_]);
}

void PrefabWorkspaceWindow::LoadPrefabForEdit(const std::string& prefabName_)
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::LoadPrefabForEdit");

    if (!previewGameObjectManager || prefabName_.empty()) return;

    editingPrefabName = prefabName_;
    // 読み込み先をPreview専用Managerに限定し、現在のSceneへPrefabを増やさない。
    previewGameObjectManager->Clear();
    // 編集時は内部Object名を保持し、保存先のPrefab名で生成先や装着先の参照名を変えない。
    editingRootObject = previewGameObjectManager->Instantiate(editingPrefabName, true);
    if (const std::shared_ptr<GameObject> rootObject_ = editingRootObject.lock())
    {
        previewGameObjectManager->SetSelectedObject(rootObject_);
    }
    previewGameObjectManager->Update(0.0f);
}

void PrefabWorkspaceWindow::SaveCurrentPrefab()
{
    BS_PROFILE_SCOPE("PrefabWorkspaceWindow::SaveCurrentPrefab");

    if (!previewGameObjectManager) return;

    std::shared_ptr<GameObject> rootObject_ = editingRootObject.lock();
    if (!rootObject_ || editingPrefabName.empty()) return;

    previewGameObjectManager->SaveAsPrefab(rootObject_, editingPrefabName);
}

void PrefabWorkspaceWindow::AcceptPrefabDrop()
{
    if (!ImGui::BeginDragDropTarget()) return;

    const ImGuiPayload* payload_ = ImGui::AcceptDragDropPayload(AssetBrowserPayload::PREFAB);
    if (payload_ && payload_->Data && previewGameObjectManager)
    {
        // Asset Browserから落とされたPrefabだけを、編集用SceneViewへ読み込む。
        const char* path_ = static_cast<const char*>(payload_->Data);
        LoadPrefabForEdit(std::filesystem::path(path_).stem().string());
    }
    ImGui::EndDragDropTarget();
}
