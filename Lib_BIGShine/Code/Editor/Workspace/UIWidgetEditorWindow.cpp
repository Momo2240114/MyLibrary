#include "PrecompiledHeader.h"

#include "UIWidgetEditorWindow.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/UI/UIAnimationPlayerComponent.h"
#include "Engine/UI/UIVertexModifierComponent.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "Engine/UI/WidgetBoardComponent.h"

#include <algorithm>

namespace
{
    constexpr float SPLITTER_SIZE = 6.0f;

    ImU32 ToImColor(const DirectX::XMFLOAT4& color_)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(color_.x, color_.y, color_.z, color_.w));
    }

    void DrawVerticalSplitter(const char* id_, float height_, float& width_, float direction_)
    {
        ImGui::InvisibleButton(id_, ImVec2(SPLITTER_SIZE, height_));
        const bool hovered_ = ImGui::IsItemHovered();
        const bool active_ = ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(hovered_ || active_ ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        if (hovered_ || active_) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (active_) width_ += ImGui::GetIO().MouseDelta.x * direction_;
    }

    void DrawHorizontalSplitter(const char* id_, float width_, float& height_, float direction_)
    {
        ImGui::InvisibleButton(id_, ImVec2(width_, SPLITTER_SIZE));
        const bool hovered_ = ImGui::IsItemHovered();
        const bool active_ = ImGui::IsItemActive();
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetItemRectMin(),
            ImGui::GetItemRectMax(),
            ImGui::GetColorU32(hovered_ || active_ ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator));
        if (hovered_ || active_) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (active_) height_ += ImGui::GetIO().MouseDelta.y * direction_;
    }

    int CalculateTransformDepth(const std::shared_ptr<GameObject>& object_)
    {
        int depth_ = 0;
        if (!object_ || !object_->transform) return depth_;

        auto parentTransform_ = object_->transform->parent.lock();
        while (parentTransform_)
        {
            ++depth_;
            parentTransform_ = parentTransform_->parent.lock();
        }

        return depth_;
    }
}

UIWidgetEditorWindow::UIWidgetEditorWindow(GameObjectManager* manager_)
    : EditorWorkspaceWindow("UI Widget Workspace", manager_)
{
    isVisible = false;
    style.backgroundColor = ImVec4(0.055f, 0.06f, 0.065f, 1.0f);
    style.titleBarColor = ImVec4(0.13f, 0.22f, 0.25f, 1.0f);
}

void UIWidgetEditorWindow::Draw()
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::Draw");

    if (autoRefresh) RefreshCache();

    if (!BeginWorkspaceWindow(ImGuiWindowFlags_MenuBar))
    {
        EndEditorWindow();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        DrawToolbar();
        ImGui::EndMenuBar();
    }

    const ImVec2 available_ = ImGui::GetContentRegionAvail();
    hierarchyWidth = std::clamp(hierarchyWidth, 220.0f, (std::max)(220.0f, available_.x * 0.38f));
    inspectorWidth = std::clamp(inspectorWidth, 280.0f, (std::max)(280.0f, available_.x * 0.45f));
    animationHeight = std::clamp(animationHeight, 120.0f, (std::max)(120.0f, available_.y * 0.45f));

    const float centerWidth_ = (std::max)(260.0f, available_.x - hierarchyWidth - inspectorWidth - SPLITTER_SIZE * 2.0f);
    const float previewHeight_ = (std::max)(160.0f, available_.y - animationHeight - SPLITTER_SIZE);

    ImGui::BeginChild("UIWidgetHierarchy", ImVec2(hierarchyWidth, available_.y), true);
    DrawHierarchyPanel();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##UIWidgetHierarchySplitter", available_.y, hierarchyWidth, 1.0f);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("UIWidgetCenter", ImVec2(centerWidth_, available_.y), false);
    ImGui::BeginChild("UIWidgetPreview", ImVec2(centerWidth_, previewHeight_), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawPreviewPanel(ImGui::GetContentRegionAvail());
    ImGui::EndChild();

    DrawHorizontalSplitter("##UIWidgetAnimationSplitter", centerWidth_, animationHeight, -1.0f);

    ImGui::BeginChild("UIWidgetAnimation", ImVec2(centerWidth_, 0.0f), true);
    DrawAnimationPanel();
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine(0.0f, 0.0f);
    DrawVerticalSplitter("##UIWidgetInspectorSplitter", available_.y, inspectorWidth, -1.0f);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::BeginChild("UIWidgetInspector", ImVec2(inspectorWidth, available_.y), true);
    DrawInspectorPanel();
    ImGui::EndChild();

    EndEditorWindow();
}

void UIWidgetEditorWindow::RefreshCache()
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::RefreshCache");

    widgets.clear();
    boardObjects.clear();
    if (!gameObjectManager) return;

    for (const std::shared_ptr<GameObject>& object_ : gameObjectManager->GetObjects())
    {
        if (!object_) continue;

        std::shared_ptr<WidgetBoardComponent> board_ = object_->GetComponent<WidgetBoardComponent>();
        if (board_) boardObjects.push_back(object_);

        std::shared_ptr<UIWidgetComponent> widget_ = object_->GetComponent<UIWidgetComponent>();
        if (!widget_) continue;

        WidgetEntry entry_;
        entry_.object = object_;
        entry_.widget = widget_;
        entry_.sprite = object_->GetComponent<SpriteRenderer>();
        entry_.board = board_ ? board_ : ResolveBoard(object_);
        entry_.vertexModifier = object_->GetComponent<UIVertexModifierComponent>();
        entry_.animationPlayer = object_->GetComponent<UIAnimationPlayerComponent>();
        widgets.push_back(entry_);
    }

    std::stable_sort(
        widgets.begin(),
        widgets.end(),
        [](const WidgetEntry& left_, const WidgetEntry& right_)
        {
            return CalculateTransformDepth(left_.object) < CalculateTransformDepth(right_.object);
        });
}

void UIWidgetEditorWindow::DrawToolbar()
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::DrawToolbar");

    if (ImGui::MenuItem("Refresh"))
    {
        RefreshCache();
    }
    ImGui::MenuItem("Auto Refresh", nullptr, &autoRefresh);

    if (ImGui::MenuItem("Add Board To Selected"))
    {
        if (gameObjectManager && gameObjectManager->GetSelectedObject())
        {
            gameObjectManager->GetSelectedObject()->GetOrAddComponent<WidgetBoardComponent>();
            RefreshCache();
        }
    }

    if (ImGui::MenuItem("Make Selected Widget"))
    {
        EnsureWidgetObject();
        RefreshCache();
    }
}

void UIWidgetEditorWindow::DrawHierarchyPanel()
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::DrawHierarchyPanel");

    DrawSelectedObjectTools();
    ImGui::Separator();

    ImGui::Text("Boards");
    for (const std::shared_ptr<GameObject>& boardObject_ : boardObjects)
    {
        if (!boardObject_) continue;
        const bool selected_ = selectedObject.lock() == boardObject_;
        if (ImGui::Selectable(boardObject_->GetName().c_str(), selected_))
        {
            SelectObject(boardObject_);
        }
    }

    ImGui::Separator();
    ImGui::Text("Widgets");
    for (WidgetEntry& entry_ : widgets)
    {
        if (!entry_.object || !entry_.widget) continue;

        const int depth_ = CalculateTransformDepth(entry_.object);
        ImGui::Indent(depth_ * 16.0f);
        const bool selected_ = selectedObject.lock() == entry_.object;
        const std::string label_ = entry_.widget->GetWidgetId().empty()
            ? entry_.object->GetName()
            : entry_.widget->GetWidgetId();
        if (ImGui::Selectable(label_.c_str(), selected_))
        {
            SelectObject(entry_.object);
        }
        ImGui::Unindent(depth_ * 16.0f);
    }
}

void UIWidgetEditorWindow::DrawPreviewPanel(const ImVec2& previewSize_)
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::DrawPreviewPanel");

    DirectX::XMFLOAT2 designSize_ = { 1920.0f, 1080.0f };
    if (std::shared_ptr<GameObject> selectedObject_ = selectedObject.lock())
    {
        if (std::shared_ptr<WidgetBoardComponent> board_ = ResolveBoard(selectedObject_))
        {
            designSize_ = board_->GetReferenceResolution();
        }
    }
    else if (!boardObjects.empty() && boardObjects.front())
    {
        if (std::shared_ptr<WidgetBoardComponent> board_ = boardObjects.front()->GetComponent<WidgetBoardComponent>())
        {
            designSize_ = board_->GetReferenceResolution();
        }
    }
    designSize_.x = (std::max)(1.0f, designSize_.x);
    designSize_.y = (std::max)(1.0f, designSize_.y);

    ImGui::Text("Canvas %.0fx%.0f", designSize_.x, designSize_.y);
    ImGui::SameLine();
    if (ImGui::SmallButton("Fit")) previewAutoFit = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("100%"))
    {
        previewZoom = 1.0f;
        previewPan = ImVec2(0.0f, 0.0f);
        previewAutoFit = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Center"))
    {
        previewPan = ImVec2(0.0f, 0.0f);
        previewAutoFit = false;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("Zoom", &previewZoom, 0.05f, 4.0f, "%.2fx"))
    {
        previewAutoFit = false;
    }

    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    const ImVec2 origin_ = ImGui::GetCursorScreenPos();
    const ImVec2 remainingSize_ = ImGui::GetContentRegionAvail();
    const ImVec2 canvasSize_ = ImVec2(
        (std::max)(1.0f, previewSize_.x),
        (std::max)(1.0f, remainingSize_.y));
    const ImVec2 canvasEnd_ = ImVec2(origin_.x + canvasSize_.x, origin_.y + canvasSize_.y);

    ImGui::InvisibleButton("##UIWidgetPreviewCanvas", canvasSize_);
    const bool canvasHovered_ = ImGui::IsItemHovered();
    const ImVec2 afterCanvasCursor_ = ImGui::GetCursorScreenPos();
    drawList_->AddRectFilled(origin_, canvasEnd_, IM_COL32(18, 21, 23, 255));
    drawList_->AddRect(origin_, canvasEnd_, IM_COL32(80, 92, 96, 255));

    previewZoom = std::clamp(previewZoom, 0.05f, 4.0f);
    const float fitZoom_ = (std::min)(
        canvasSize_.x / designSize_.x,
        canvasSize_.y / designSize_.y) * 0.95f;
    if (previewAutoFit)
    {
        previewZoom = std::clamp(fitZoom_, 0.05f, 4.0f);
        previewPan = ImVec2(
            (canvasSize_.x - designSize_.x * previewZoom) * 0.5f,
            (canvasSize_.y - designSize_.y * previewZoom) * 0.5f);
    }

    ImGuiIO& io_ = ImGui::GetIO();
    if (canvasHovered_ && io_.MouseWheel != 0.0f)
    {
        const ImVec2 mouseLocal_ = ImVec2(io_.MousePos.x - origin_.x, io_.MousePos.y - origin_.y);
        const float oldZoom_ = previewZoom;
        const float zoomScale_ = io_.MouseWheel > 0.0f ? 1.1f : 1.0f / 1.1f;
        previewZoom = std::clamp(previewZoom * zoomScale_, 0.05f, 4.0f);
        if (oldZoom_ > 0.0001f)
        {
            previewPan.x = mouseLocal_.x - (mouseLocal_.x - previewPan.x) * (previewZoom / oldZoom_);
            previewPan.y = mouseLocal_.y - (mouseLocal_.y - previewPan.y) * (previewZoom / oldZoom_);
        }
        previewAutoFit = false;
    }
    if (canvasHovered_ &&
        (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
    {
        previewPan.x += io_.MouseDelta.x;
        previewPan.y += io_.MouseDelta.y;
        previewAutoFit = false;
    }

    const ImVec2 designOrigin_ = ImVec2(origin_.x + previewPan.x, origin_.y + previewPan.y);
    const ImVec2 designEnd_ = ImVec2(
        designOrigin_.x + designSize_.x * previewZoom,
        designOrigin_.y + designSize_.y * previewZoom);
    drawList_->PushClipRect(origin_, canvasEnd_, true);
    drawList_->AddRectFilled(designOrigin_, designEnd_, IM_COL32(28, 32, 35, 255));
    drawList_->AddRect(designOrigin_, designEnd_, IM_COL32(120, 135, 140, 255));

    for (WidgetEntry& entry_ : widgets)
    {
        if (!entry_.widget || !entry_.object) continue;

        UIWidgetComponent* parentWidget_ = ResolveParentWidget(entry_.widget.get());
        entry_.widget->CalculateLayout(entry_.board.get(), parentWidget_, designSize_);
        const UIScreenRect& rect_ = entry_.widget->GetScreenRect();

        ImVec2 min_ = ImVec2(
            designOrigin_.x + rect_.min.x * previewZoom,
            designOrigin_.y + rect_.min.y * previewZoom);
        ImVec2 max_ = ImVec2(
            designOrigin_.x + rect_.max.x * previewZoom,
            designOrigin_.y + rect_.max.y * previewZoom);
        const bool selected_ = selectedObject.lock() == entry_.object;
        const ImU32 color_ = selected_ ? IM_COL32(70, 180, 210, 230) : IM_COL32(130, 150, 155, 170);
        if (entry_.sprite && entry_.sprite->IsVisible())
        {
            const DirectX::XMFLOAT4 spriteColor_ = entry_.sprite->GetColor();
            ID3D11ShaderResourceView* texture_ = entry_.sprite->GetTexture().Get();
            if (texture_)
            {
                DirectX::XMFLOAT2 textureSize_ = entry_.sprite->GetTextureSize();
                textureSize_.x = (std::max)(1.0f, textureSize_.x);
                textureSize_.y = (std::max)(1.0f, textureSize_.y);

                const DirectX::XMFLOAT2 catPos_ = entry_.sprite->GetCatPos();
                const DirectX::XMFLOAT2 catSize_ = entry_.sprite->GetCatSize();
                const ImVec2 uvMin_ = ImVec2(catPos_.x / textureSize_.x, catPos_.y / textureSize_.y);
                const ImVec2 uvMax_ = ImVec2(
                    (catPos_.x + catSize_.x) / textureSize_.x,
                    (catPos_.y + catSize_.y) / textureSize_.y);

                drawList_->AddImage(
                    reinterpret_cast<ImTextureID>(texture_),
                    min_,
                    max_,
                    uvMin_,
                    uvMax_,
                    ToImColor(spriteColor_));
            }
            else if (spriteColor_.w > 0.0f)
            {
                drawList_->AddRectFilled(min_, max_, ToImColor(spriteColor_));
            }
        }
        drawList_->AddRect(min_, max_, color_, 0.0f, 0, selected_ ? 3.0f : 1.5f);
        drawList_->AddText(ImVec2(min_.x + 4.0f, min_.y + 4.0f), color_, entry_.widget->GetWidgetId().c_str());

        const ImVec2 center_ = ImVec2((min_.x + max_.x) * 0.5f, (min_.y + max_.y) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(center_.x - 8.0f, center_.y - 8.0f));
        ImGui::PushID(entry_.object.get());
        ImGui::InvisibleButton("WidgetDragHandle", ImVec2(16.0f, 16.0f));
        if (ImGui::IsItemClicked())
        {
            SelectObject(entry_.object);
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const ImVec2 delta_ = ImGui::GetIO().MouseDelta;
            UIRectTransform& rectTransform_ = entry_.widget->GetRectTransform();
            const DirectX::XMFLOAT2 scale_ = entry_.board
                ? entry_.board->CalculateBoardScale(designSize_)
                : DirectX::XMFLOAT2{ 1.0f, 1.0f };
            rectTransform_.anchoredPosition.x += delta_.x / (std::max)(0.0001f, scale_.x * previewZoom);
            rectTransform_.anchoredPosition.y += delta_.y / (std::max)(0.0001f, scale_.y * previewZoom);
            entry_.widget->MarkLayoutDirty();
        }
        ImGui::PopID();
    }
    drawList_->PopClipRect();

    ImGui::SetCursorScreenPos(afterCanvasCursor_);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

void UIWidgetEditorWindow::DrawInspectorPanel()
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::DrawInspectorPanel");

    std::shared_ptr<GameObject> selectedObject_ = selectedObject.lock();
    if (!selectedObject_)
    {
        ImGui::TextUnformatted("No UI object selected");
        return;
    }

    ImGui::Text("Object: %s", selectedObject_->GetName().c_str());
    if (ImGui::Button("Select In Scene"))
    {
        gameObjectManager->SetSelectedObject(selectedObject_);
    }
    ImGui::Separator();

    if (std::shared_ptr<WidgetBoardComponent> board_ = selectedObject_->GetComponent<WidgetBoardComponent>())
    {
        if (ImGui::CollapsingHeader("Widget Board", ImGuiTreeNodeFlags_DefaultOpen))
        {
            board_->DrawGuiProfiled();
        }
    }

    if (std::shared_ptr<UIWidgetComponent> widget_ = selectedObject_->GetComponent<UIWidgetComponent>())
    {
        if (ImGui::CollapsingHeader("UI Widget", ImGuiTreeNodeFlags_DefaultOpen))
        {
            widget_->DrawGuiProfiled();
        }
    }

    if (std::shared_ptr<SpriteRenderer> sprite_ = selectedObject_->GetComponent<SpriteRenderer>())
    {
        if (ImGui::CollapsingHeader("Sprite Renderer"))
        {
            sprite_->DrawGuiProfiled();
        }
    }

    if (std::shared_ptr<UIVertexModifierComponent> vertexModifier_ = selectedObject_->GetComponent<UIVertexModifierComponent>())
    {
        if (ImGui::CollapsingHeader("Vertex Modifier", ImGuiTreeNodeFlags_DefaultOpen))
        {
            vertexModifier_->DrawGuiProfiled();
        }
    }

    if (std::shared_ptr<UIAnimationPlayerComponent> animationPlayer_ = selectedObject_->GetComponent<UIAnimationPlayerComponent>())
    {
        if (ImGui::CollapsingHeader("Animation Player"))
        {
            animationPlayer_->DrawGuiProfiled();
        }
    }
}

void UIWidgetEditorWindow::DrawAnimationPanel()
{
    BS_PROFILE_SCOPE("UIWidgetEditorWindow::DrawAnimationPanel");

    std::shared_ptr<GameObject> selectedObject_ = selectedObject.lock();
    if (!selectedObject_)
    {
        ImGui::TextUnformatted("Select a widget to edit animation playback.");
        return;
    }

    std::shared_ptr<UIAnimationPlayerComponent> animationPlayer_ =
        selectedObject_->GetComponent<UIAnimationPlayerComponent>();
    if (!animationPlayer_)
    {
        if (ImGui::Button("Add Animation Player"))
        {
            selectedObject_->AddComponent<UIAnimationPlayerComponent>();
            RefreshCache();
        }
        return;
    }

    animationPlayer_->DrawGuiProfiled();
}

void UIWidgetEditorWindow::DrawSelectedObjectTools()
{
    if (!gameObjectManager) return;

    std::shared_ptr<GameObject> sceneSelected_ = gameObjectManager->GetSelectedObject();
    if (!sceneSelected_)
    {
        ImGui::TextUnformatted("Scene selection: none");
        return;
    }

    ImGui::Text("Scene selection: %s", sceneSelected_->GetName().c_str());
    if (ImGui::Button("Use Selection"))
    {
        SelectObject(sceneSelected_);
    }
    if (ImGui::Button("Add Missing UI Components"))
    {
        EnsureWidgetObject();
        RefreshCache();
    }
}

void UIWidgetEditorWindow::SelectObject(const std::shared_ptr<GameObject>& object_)
{
    selectedObject = object_;
    if (gameObjectManager && object_)
    {
        gameObjectManager->SetSelectedObject(object_);
    }
}

void UIWidgetEditorWindow::EnsureWidgetObject()
{
    if (!gameObjectManager) return;
    std::shared_ptr<GameObject> object_ = gameObjectManager->GetSelectedObject();
    if (!object_) return;

    object_->GetOrAddComponent<SpriteRenderer>();
    object_->GetOrAddComponent<UIWidgetComponent>();
    object_->GetOrAddComponent<UIVertexModifierComponent>();
    SelectObject(object_);
}

std::shared_ptr<WidgetBoardComponent> UIWidgetEditorWindow::ResolveBoard(
    const std::shared_ptr<GameObject>& object_) const
{
    if (!object_ || !object_->transform) return nullptr;

    std::shared_ptr<WidgetBoardComponent> board_ = object_->GetComponent<WidgetBoardComponent>();
    if (board_) return board_;

    auto parentTransform_ = object_->transform->parent.lock();
    while (parentTransform_)
    {
        auto parentObject_ = parentTransform_->gameObject.lock();
        if (parentObject_)
        {
            board_ = parentObject_->GetComponent<WidgetBoardComponent>();
            if (board_) return board_;
        }
        parentTransform_ = parentTransform_->parent.lock();
    }

    return nullptr;
}

UIWidgetComponent* UIWidgetEditorWindow::ResolveParentWidget(UIWidgetComponent* widget_) const
{
    if (!widget_) return nullptr;
    return widget_->GetParentWidget();
}
