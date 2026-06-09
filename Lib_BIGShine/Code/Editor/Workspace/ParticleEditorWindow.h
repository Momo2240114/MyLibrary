#pragma once

#include "EditorWorkspaceWindow.h"
#include "EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/ParticleRenderSystem.h"
#include "Engine/Graphics/Resource/GlTFModel.h"

#include <memory>
#include <string>
#include <vector>

class ParticleEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit ParticleEditorWindow(GameObjectManager* manager_);
    void Draw() override;
    bool ShouldPauseScene() const override { return true; }

private:
    void EnsurePreviewObjects();
    void DrawToolbar();
    void DrawPreviewPanel();
    void DrawAssetBrowserPanel();
    void DrawInspectorPanel();
    void RenderPreview(const ImVec2& previewSize_);
    void RefreshParticleAssets();

private:
    EditorRenderTarget previewTarget;
    ModelRenderSystem modelRenderSystem;
    ParticleRenderSystem particleRenderSystem;
    CameraComponent previewCamera;
    std::shared_ptr<GameObject> previewObject;
    std::shared_ptr<GltfModelCommponennt> previewModel;
    std::shared_ptr<ParticleComponent> previewParticle;
    std::vector<std::string> particleAssetPaths;
    int selectedAssetIndex = -1;
    bool isInitialized = false;
    bool autoPlay = true;
    bool showGrid = true;
};
