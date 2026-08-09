#pragma once

#include "Editor/EditorCameraData.h"
#include "Editor/Window/EditorWindow.h"
#include "Editor/Workspace/EditorRenderTarget.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include "Engine/Graphics/RenderSystem/SpriteRenderSystem.h"

// 現在のSceneをランタイムカメラとは別のEditor専用カメラで確認するWindow。
class EditorSceneViewWindow : public EditorWindow
{
public:
    EditorSceneViewWindow(
        GameObjectManager* manager_,
        SpriteRenderSystem* spriteRenderSystem_,
        ModelRenderSystem* modelRenderSystem_);

    // Editor専用カメラでSceneを再描画し、通常SceneViewと同時に別視点を表示する。
    void Draw() override;

private:
    std::shared_ptr<CameraComponent> ResolveEditorCamera();
    void DrawMenuBarContent(const std::shared_ptr<CameraComponent>& editorCamera_);
    void DrawCameraSettings(const std::shared_ptr<CameraComponent>& editorCamera_);
    void RenderEditorSceneView(const std::shared_ptr<CameraComponent>& editorCamera_);
    ImVec2 CalculateImageSize(const ImVec2& availableSize_) const;

private:
    SpriteRenderSystem* spriteRenderSystem = nullptr;
    ModelRenderSystem* modelRenderSystem = nullptr;
    EditorRenderTarget sceneViewTarget;
    EditorCameraData editorCameraData;
    bool initializedFromRuntimeCamera = false;
    bool renderEnabled = true;
    bool showCameraSettings = true;
};
