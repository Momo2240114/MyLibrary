#include "PrecompiledHeader.h"

#include "AnimNodeEditorWindow.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include <algorithm>

namespace
{
    void DrawVerticalSplitter(
        const char* id_,
        float height_,
        float& size_,
        float deltaScale_)
    {
        constexpr float SPLITTER_SIZE = 6.0f;
        ImGui::InvisibleButton(id_, ImVec2(SPLITTER_SIZE, height_));
        const bool hovered_ = ImGui::IsItemHovered();
        const bool active_ = ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(
                hovered_ || active_
                ? ImGuiCol_SeparatorHovered
                : ImGuiCol_Separator));
        if (hovered_ || active_)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (active_)
        {
            size_ += ImGui::GetIO().MouseDelta.x * deltaScale_;
        }
    }

    void DrawHorizontalSplitter(
        const char* id_,
        float width_,
        float& size_,
        float deltaScale_)
    {
        constexpr float SPLITTER_SIZE = 6.0f;
        ImGui::InvisibleButton(id_, ImVec2(width_, SPLITTER_SIZE));
        const bool hovered_ = ImGui::IsItemHovered();
        const bool active_ = ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(
                hovered_ || active_
                ? ImGuiCol_SeparatorHovered
                : ImGuiCol_Separator));
        if (hovered_ || active_)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (active_)
        {
            size_ += ImGui::GetIO().MouseDelta.y * deltaScale_;
        }
    }
}

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
    constexpr float SPLITTER_SIZE = 6.0f;
    const float verticalSpacing_ = ImGui::GetStyle().ItemSpacing.y * 2.0f;
    const float minimumLeftWidth_ = 420.0f;
    const float minimumRightWidth_ = 360.0f;
    const float minimumSceneHeight_ = 180.0f;
    const float minimumLeftBottomHeight_ = 180.0f;
    const float minimumGraphHeight_ = 240.0f;
    const float minimumAssetHeight_ = 180.0f;
    const float minimumHierarchyWidth_ = 180.0f;
    const float minimumInspectorWidth_ = 220.0f;

    if (!layoutSizeInitialized)
    {
        rightPanelWidth = available_.x * 0.42f;
        scenePreviewHeight = available_.y * 0.64f;
        hierarchyWidth = available_.x * 0.24f;
        assetBrowserHeight = available_.y * 0.30f;
        layoutSizeInitialized = true;
    }

    rightPanelWidth = std::clamp(
        rightPanelWidth,
        minimumRightWidth_,
        (std::max)(minimumRightWidth_, available_.x - minimumLeftWidth_ - SPLITTER_SIZE));
    const float leftWidth_ = (std::max)(
        minimumLeftWidth_,
        available_.x - rightPanelWidth - SPLITTER_SIZE);

    scenePreviewHeight = std::clamp(
        scenePreviewHeight,
        minimumSceneHeight_,
        (std::max)(
            minimumSceneHeight_,
            available_.y - minimumLeftBottomHeight_ - SPLITTER_SIZE - verticalSpacing_));
    assetBrowserHeight = std::clamp(
        assetBrowserHeight,
        minimumAssetHeight_,
        (std::max)(
            minimumAssetHeight_,
            available_.y - minimumGraphHeight_ - SPLITTER_SIZE - verticalSpacing_));
    hierarchyWidth = std::clamp(
        hierarchyWidth,
        minimumHierarchyWidth_,
        (std::max)(minimumHierarchyWidth_, leftWidth_ - minimumInspectorWidth_ - SPLITTER_SIZE));

    const float graphHeight_ = (std::max)(
        minimumGraphHeight_,
        available_.y - assetBrowserHeight - SPLITTER_SIZE - verticalSpacing_);

    ImGui::BeginGroup();
    ImGui::BeginChild("AnimNodeScenePreviewPanel", ImVec2(leftWidth_, scenePreviewHeight), true);
    DrawScenePreviewPanel();
    ImGui::EndChild();

    DrawHorizontalSplitter(
        "##AnimNodeLeftHorizontalSplitter",
        leftWidth_,
        scenePreviewHeight,
        1.0f);

    ImGui::BeginChild("AnimNodeLeftBottomPanel", ImVec2(leftWidth_, 0.0f), false);
    ImGui::BeginChild("AnimNodeSceneHierarchyPanel", ImVec2(hierarchyWidth, 0.0f), true);
    DrawSceneHierarchyPanel();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter(
        "##AnimNodeHierarchySplitter",
        ImGui::GetContentRegionAvail().y,
        hierarchyWidth,
        1.0f);

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("AnimNodeSceneInspectorPanel", ImVec2(0.0f, 0.0f), true);
    DrawSceneInspectorPanel();
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter(
        "##AnimNodeMainVerticalSplitter",
        available_.y,
        rightPanelWidth,
        -1.0f);

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginGroup();
    ImGui::BeginChild("AnimNodeGraphPanel", ImVec2(rightPanelWidth, graphHeight_), true);
    DrawGraphPanel(animController);
    ImGui::EndChild();

    DrawHorizontalSplitter(
        "##AnimNodeRightHorizontalSplitter",
        rightPanelWidth,
        assetBrowserHeight,
        -1.0f);

    ImGui::BeginChild("AnimNodeAssetBrowserPanel", ImVec2(rightPanelWidth, 0.0f), true);
    DrawAssetBrowserPanel();
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
    ImGui::SameLine();

    const char* connectionModeItems_[] = { "Legacy Two Pin", "Unity Style" };
    int connectionModeIndex_ = static_cast<int>(graphConnectionMode);
    ImGui::TextUnformatted("Connection Mode:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo("##AnimGraphConnectionMode", &connectionModeIndex_, connectionModeItems_, IM_ARRAYSIZE(connectionModeItems_)))
    {
        graphConnectionMode = static_cast<ANIM_GRAPH_CONNECTION_MODE>(connectionModeIndex_);
    }

    ImGui::Separator();

    if (!animController_)
    {
        ImGui::TextWrapped("Select a Scene GameObject that has GltfModel and AnimationControllerComponent.");
        return;
    }

    constexpr float SPLITTER_SIZE = 6.0f;
    constexpr float MINIMUM_TRANSITION_INSPECTOR_WIDTH = 260.0f;
    constexpr float MINIMUM_GRAPH_CANVAS_WIDTH = 360.0f;

    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    transitionInspectorWidth = std::clamp(
        transitionInspectorWidth,
        MINIMUM_TRANSITION_INSPECTOR_WIDTH,
        (std::max)(MINIMUM_TRANSITION_INSPECTOR_WIDTH, available_.x - MINIMUM_GRAPH_CANVAS_WIDTH - SPLITTER_SIZE));

    const float graphCanvasWidth_ = (std::max)(
        MINIMUM_GRAPH_CANVAS_WIDTH,
        available_.x - transitionInspectorWidth - SPLITTER_SIZE);

    ImGui::BeginChild("AnimNodeGraphCanvas", ImVec2(graphCanvasWidth_, 0.0f), false);
    animController_->DrawNodeEditorContent(graphConnectionMode);
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter(
        "##AnimNodeGraphTransitionSplitter",
        available_.y,
        transitionInspectorWidth,
        -1.0f);

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("AnimNodeTransitionInspector", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    animController_->DrawTransitionInspectorContent();
    ImGui::EndChild();
}

void AnimNodeEditorWindow::DrawAssetBrowserPanel()
{
    ImGui::TextUnformatted("Asset Browser");
    ImGui::Separator();
    assetBrowser.DrawContent();
}
