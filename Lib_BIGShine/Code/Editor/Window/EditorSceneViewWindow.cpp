#include "PrecompiledHeader.h"

#include "EditorSceneViewWindow.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>

EditorSceneViewWindow::EditorSceneViewWindow(
    GameObjectManager* manager_,
    SpriteRenderSystem* spriteRenderSystem_,
    ModelRenderSystem* modelRenderSystem_)
    : EditorWindow("Editor Scene View", manager_),
    spriteRenderSystem(spriteRenderSystem_),
    modelRenderSystem(modelRenderSystem_)
{
    defaultWindowSize = ImVec2(640.0f, 420.0f);
}

void EditorSceneViewWindow::Draw()
{
    BS_PROFILE_SCOPE("EditorSceneViewWindow::Draw");

    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    const std::shared_ptr<CameraComponent> editorCamera_ = ResolveEditorCamera();
    DrawMenuBarContent(editorCamera_);

    if (!editorCamera_)
    {
        ImGui::TextDisabled("Editor camera is not available.");
        EndEditorWindow();
        return;
    }

    ImVec2 availableSize_ = ImGui::GetContentRegionAvail();
    availableSize_.x = (std::max)(1.0f, availableSize_.x);
    availableSize_.y = (std::max)(1.0f, availableSize_.y);

    const bool previewHovered_ =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    // カメラ設定の変更後も、操作開始時に現在の視点を引き継ぐ。
    if (previewHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        editorCameraData.CaptureFromCamera(*editorCamera_);
    editorCameraData.UpdatePreviewFreeCamera(
        *editorCamera_,
        ImGui::GetIO().DeltaTime,
        previewHovered_);

    if (renderEnabled)
    {
        RenderEditorSceneView(editorCamera_);
    }

    if (sceneViewTarget.GetShaderResourceView())
    {
        const ImVec2 imageSize_ = CalculateImageSize(availableSize_);
        const ImVec2 cursorPosition_ = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(
            cursorPosition_.x + (availableSize_.x - imageSize_.x) * 0.5f,
            cursorPosition_.y));
        ImGui::Image(
            reinterpret_cast<void*>(sceneViewTarget.GetShaderResourceView()),
            imageSize_);
    }
    else
    {
        ImGui::TextDisabled("Scene view render target is not ready.");
    }

    if (showCameraSettings)
    {
        ImGui::Separator();
        DrawCameraSettings(editorCamera_);
    }

    DrawStylePopup();
    EndEditorWindow();
}

std::shared_ptr<CameraComponent> EditorSceneViewWindow::ResolveEditorCamera()
{
    if (!gameObjectManager) return nullptr;

    CameraManager* cameraManager_ = gameObjectManager->GetCameraManager();
    if (!cameraManager_) return nullptr;

    std::shared_ptr<CameraComponent> editorCamera_ = cameraManager_->GetEditorCamera();
    if (!editorCamera_) return nullptr;

    if (!initializedFromRuntimeCamera)
    {
        // Editorでは中央ビューの視点を維持し、実行中だけゲームカメラを初期値にする。
        if (!gameObjectManager->SupportsEditorPlayMode() || gameObjectManager->GetEditorPlayMode() != EditorPlayMode::EDITOR)
            cameraManager_->CopyActiveCameraToEditorCamera();
        editorCameraData.CaptureFromCamera(*editorCamera_);
        initializedFromRuntimeCamera = true;
    }

    return editorCamera_;
}

void EditorSceneViewWindow::DrawMenuBarContent(const std::shared_ptr<CameraComponent>& editorCamera_)
{
    if (!ImGui::BeginMenuBar()) return;

    if (ImGui::BeginMenu("Camera"))
    {
        ImGui::MenuItem("Render", nullptr, &renderEnabled);
        ImGui::MenuItem("Free Camera", nullptr, &editorCameraData.enablePreviewFreeCamera);
        ImGui::MenuItem("Show Settings", nullptr, &showCameraSettings);
        ImGui::Separator();

        if (ImGui::MenuItem("Sync From Runtime Camera") && gameObjectManager && editorCamera_)
        {
            if (CameraManager* cameraManager_ = gameObjectManager->GetCameraManager())
            {
                cameraManager_->CopyActiveCameraToEditorCamera();
                editorCameraData.CaptureFromCamera(*editorCamera_);
            }
        }

        if (ImGui::MenuItem("Reset Editor Camera") && editorCamera_)
        {
            editorCameraData.ResetPreviewCamera(*editorCamera_);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

void EditorSceneViewWindow::DrawCameraSettings(const std::shared_ptr<CameraComponent>& editorCamera_)
{
    if (!editorCamera_) return;

    if (ImGui::CollapsingHeader("Editor Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        editorCameraData.Gui();

        float fovY_ = editorCamera_->GetFovY();
        float nearZ_ = editorCamera_->GetNear();
        float farZ_ = editorCamera_->GetFar();
        bool projectionChanged_ = false;

        projectionChanged_ |= ImGui::DragFloat("FOV", &fovY_, 0.1f, 10.0f, 120.0f, "%.1f");
        projectionChanged_ |= ImGui::DragFloat("Near", &nearZ_, 0.001f, 0.001f, farZ_ - 0.001f, "%.3f");
        projectionChanged_ |= ImGui::DragFloat("Far", &farZ_, 1.0f, nearZ_ + 0.001f, 10000.0f, "%.1f");

        if (projectionChanged_)
        {
            const float aspect_ = (std::max)(0.01f, editorCamera_->GetAspect());
            editorCamera_->SetPerspectiveFov(fovY_, aspect_, nearZ_, farZ_);
        }
    }
}

void EditorSceneViewWindow::RenderEditorSceneView(const std::shared_ptr<CameraComponent>& editorCamera_)
{
    BS_PROFILE_SCOPE("EditorSceneViewWindow::RenderEditorSceneView");

    if (!gameObjectManager || !spriteRenderSystem || !modelRenderSystem || !editorCamera_) return;

    GraphicsDeviceManager& graphics_ = GraphicsDeviceManager::Instance();
    ID3D11Device* device_ = graphics_.GetDevice();
    ID3D11DeviceContext* context_ = graphics_.GetContext();
    if (!device_ || !context_) return;

    const int renderWidth_ = (std::max)(1, graphics_.GetRenderWidth());
    const int renderHeight_ = (std::max)(1, graphics_.GetRenderHeight());
    if (!sceneViewTarget.Resize(device_, renderWidth_, renderHeight_)) return;

    editorCamera_->SetPerspectiveFov(
        editorCamera_->GetFovY(),
        static_cast<float>(renderWidth_) / static_cast<float>(renderHeight_),
        editorCamera_->GetNear(),
        editorCamera_->GetFar());

    const float clearColor_[4] = { 0.03f, 0.035f, 0.04f, 1.0f };
    sceneViewTarget.Begin(context_, clearColor_);
    gameObjectManager->RenderAll(
        *spriteRenderSystem,
        *modelRenderSystem,
        sceneViewTarget.GetRenderTargetView(),
        editorCamera_.get(),
        true);
    sceneViewTarget.End(context_);
    graphics_.SetBackBufferRenderTarget();
}

ImVec2 EditorSceneViewWindow::CalculateImageSize(const ImVec2& availableSize_) const
{
    const float renderWidth_ = static_cast<float>((std::max)(1, sceneViewTarget.GetWidth()));
    const float renderHeight_ = static_cast<float>((std::max)(1, sceneViewTarget.GetHeight()));
    ImVec2 imageSize_ = availableSize_;

    const float renderAspect_ = renderWidth_ / renderHeight_;
    const float availableAspect_ = availableSize_.x / (std::max)(1.0f, availableSize_.y);
    if (availableAspect_ > renderAspect_)
    {
        imageSize_.x = availableSize_.y * renderAspect_;
    }
    else
    {
        imageSize_.y = availableSize_.x / renderAspect_;
    }

    imageSize_.x = (std::max)(1.0f, imageSize_.x);
    imageSize_.y = (std::max)(1.0f, imageSize_.y);
    return imageSize_;
}
