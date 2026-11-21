#pragma once

#include "EditorWorkspaceWindow.h"
#include "Editor/EditorCameraData.h"
#include "Editor/Utility/EditorPrimitiveRenderer.h"
#include "Editor/Window/AssetBrowserWindow.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

#include <memory>
#include <string>

class GameObject;

// 保存済みPrefabを専用SceneViewで読み込み、階層とInspectorを編集して保存するWorkspace。
class PrefabWorkspaceWindow : public EditorWorkspaceWindow
{
public:
    explicit PrefabWorkspaceWindow(GameObjectManager* manager_);

    bool ShouldPauseScene() const override
    {
        return true;
    }

    bool ShouldRenderSceneWhenPaused() const override
    {
        return false;
    }

    void Draw() override;

private:
    void DrawToolbar();
    void DrawHierarchyPanel();
    void DrawPreviewPanel();
    void DrawInspectorPanel();
    void DrawAssetBrowserPanel();
    void RenderPreview(const ImVec2& previewSize_);
    void EnsurePreviewScene();
    void LoadPrefabForEdit(const std::string& prefabName_);
    void LoadSelectedPrefab();
    void SaveCurrentPrefab();
    void AcceptPrefabDrop();

private:
    std::unique_ptr<GameObjectManager> previewGameObjectManager;
    AssetBrowserWindow assetBrowser;
    EditorPrimitiveRenderer editorPrimitiveRenderer;
    EditorRenderTarget previewTarget;
    ModelRenderSystem previewModelRenderSystem;
    ParticleRenderSystem previewParticleRenderSystem;
    MeshVFXRenderSystem previewMeshVFXRenderSystem;
    CameraComponent previewCamera;
    EditorCameraData editorCamera;
    std::weak_ptr<GameObject> editingRootObject;
    std::string editingPrefabName;
    char saveAsPrefabName[64]{};
    bool showAssetBrowser = false;
    bool isPreviewSceneInitialized = false;
    bool showGrid = true;
    float hierarchyPanelWidth = 300.0f;
    float inspectorPanelWidth = 360.0f;
    float assetBrowserHeight = 260.0f;
};
