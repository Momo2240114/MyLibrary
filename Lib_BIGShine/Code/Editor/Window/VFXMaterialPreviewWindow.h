#pragma once

#include "EditorWindow.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Graphics/VFX/MeshVFXRenderSystem.h"

#include <memory>
#include <string>

class GameObject;

class VFXMaterialPreviewWindow : public EditorWindow
{
public:
    explicit VFXMaterialPreviewWindow(GameObjectManager* manager_);
    void Draw() override;
    bool ShouldPauseScene() const override { return false; }

private:
    void EnsurePreviewObject();
    void DrawToolbar();
    void DrawPreview(const ImVec2& previewSize_);
    void RenderPreview(const ImVec2& previewSize_);
    void ApplyPreviewMaterial();

private:
    EditorRenderTarget previewTarget;
    MeshVFXRenderSystem meshVFXRenderSystem;
    CameraComponent previewCamera;
    std::shared_ptr<GameObject> previewObject;
    std::shared_ptr<MeshVFXComponent> previewMeshVFX;
    std::string materialPath;
    float previewRotation = 0.0f;
    bool isInitialized = false;
};
