#include "AnimNodeEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include <algorithm>

bool AnimNodeEditorWindow::ResolveTarget(GltfModelCommponennt*& modelComp, AnimationControllerComponent*& animController) const
{
    modelComp = nullptr;
    animController = nullptr;

    if (!gameObjectManager) return false;

    auto selectedObject = gameObjectManager->GetSelectedObject();
    if (!selectedObject) return false;

    auto model = selectedObject->GetComponent<GltfModelCommponennt>();
    auto animation = selectedObject->GetComponent<AnimationControllerComponent>();
    if (!model || !animation) return false;

    modelComp = model.get();
    animController = animation.get();
    return true;
}

void AnimNodeEditorWindow::Draw()
{
    GltfModelCommponennt* modelComp = nullptr;
    AnimationControllerComponent* animController = nullptr;
    ResolveTarget(modelComp, animController);

    if (!BeginWorkspaceWindow())
    {
        EndEditorWindow();
        return;
    }

    DrawStylePopup();

    ImVec2 available_ = ImGui::GetContentRegionAvail();
    const float rightWidth_ = (std::max)(360.0f, available_.x * 0.40f);
    const float leftWidth_ = (std::max)(320.0f, available_.x - rightWidth_ - ImGui::GetStyle().ItemSpacing.x);
    const float sceneHeight_ = (std::max)(220.0f, available_.y * 0.66f);
    const float graphHeight_ = (std::max)(260.0f, available_.y * 0.70f);

    ImGui::BeginGroup();
    ImGui::BeginChild("AnimNodeScenePreviewPanel", ImVec2(leftWidth_, sceneHeight_), true);
    DrawScenePreviewPanel();
    ImGui::EndChild();

    ImGui::BeginChild("AnimNodeLeftBottomPanel", ImVec2(leftWidth_, 0.0f), false);
    const float hierarchyWidth_ = (std::max)(180.0f, leftWidth_ * 0.50f - ImGui::GetStyle().ItemSpacing.x * 0.5f);
    ImGui::BeginChild("AnimNodeSceneHierarchyPanel", ImVec2(hierarchyWidth_, 0.0f), true);
    DrawSceneHierarchyPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("AnimNodeSceneInspectorPanel", ImVec2(0.0f, 0.0f), true);
    DrawSceneInspectorPanel();
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::BeginChild("AnimNodeGraphPanel", ImVec2(0.0f, graphHeight_), true);
    DrawGraphPanel(animController);
    ImGui::EndChild();

    ImGui::BeginChild("AnimNodeStatePanel", ImVec2(0.0f, 0.0f), true);
    DrawStatePanel(animController);
    ImGui::EndChild();
    ImGui::EndGroup();

    EndEditorWindow();
}

void AnimNodeEditorWindow::DrawScenePreviewPanel()
{
    ImGui::TextUnformatted("Scene Preview");
    ImGui::Separator();

    ID3D11ShaderResourceView* sceneSRV_ = GraphicsDeviceManager::Instance().GetSceneSRV();
    ImVec2 previewSize_ = ImGui::GetContentRegionAvail();
    previewSize_.x = (std::max)(1.0f, previewSize_.x);
    previewSize_.y = (std::max)(1.0f, previewSize_.y);

    if (sceneSRV_)
    {
        ImGui::Image(reinterpret_cast<void*>(sceneSRV_), previewSize_);
    }
    else
    {
        ImGui::Dummy(previewSize_);
    }
}

void AnimNodeEditorWindow::DrawSceneHierarchyPanel()
{
    ImGui::TextUnformatted("Scene Hierarchy");
    ImGui::Separator();

    if (!gameObjectManager)
    {
        ImGui::TextDisabled("Scene manager is not ready.");
        return;
    }

    gameObjectManager->DrawHierarchyGui();
}

void AnimNodeEditorWindow::DrawSceneInspectorPanel()
{
    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    if (!gameObjectManager || !gameObjectManager->GetSelectedObject())
    {
        ImGui::TextDisabled("Select a Scene object.");
        return;
    }

    gameObjectManager->DrawInspectorGui();
}

void AnimNodeEditorWindow::DrawGraphPanel(AnimationControllerComponent* animController_)
{
    ImGui::TextUnformatted("Graph Node");
    ImGui::Separator();

    if (!animController_)
    {
        ImGui::TextWrapped("Select a Scene GameObject that has GltfModel and AnimationControllerComponent.");
        return;
    }

    animController_->DrawNodeEditorContent();
}

void AnimNodeEditorWindow::DrawStatePanel(AnimationControllerComponent* animController_)
{
    ImGui::TextUnformatted("Animation State");
    ImGui::Separator();

    if (!animController_)
    {
        ImGui::TextDisabled("AnimationControllerComponent is required.");
        return;
    }

    animController_->Gui();
}
