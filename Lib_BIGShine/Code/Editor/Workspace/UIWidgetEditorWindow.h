#pragma once

#include "EditorWorkspaceWindow.h"

#include <memory>
#include <vector>

class GameObject;
class SpriteRenderer;
class UIAnimationPlayerComponent;
class UIWidgetComponent;
class UIVertexModifierComponent;
class WidgetBoardComponent;

class UIWidgetEditorWindow : public EditorWorkspaceWindow
{
public:
    explicit UIWidgetEditorWindow(GameObjectManager* manager_);

    void Draw() override;
    bool ShouldRenderSceneWhenPaused() const override { return true; }

private:
    struct WidgetEntry
    {
        std::shared_ptr<GameObject> object;
        std::shared_ptr<UIWidgetComponent> widget;
        std::shared_ptr<SpriteRenderer> sprite;
        std::shared_ptr<WidgetBoardComponent> board;
        std::shared_ptr<UIVertexModifierComponent> vertexModifier;
        std::shared_ptr<UIAnimationPlayerComponent> animationPlayer;
    };

    void RefreshCache();
    void DrawToolbar();
    void DrawHierarchyPanel();
    void DrawPreviewPanel(const ImVec2& previewSize_);
    void DrawInspectorPanel();
    void DrawAnimationPanel();
    void DrawSelectedObjectTools();
    void SelectObject(const std::shared_ptr<GameObject>& object_);
    void EnsureWidgetObject();
    std::shared_ptr<WidgetBoardComponent> ResolveBoard(const std::shared_ptr<GameObject>& object_) const;
    UIWidgetComponent* ResolveParentWidget(UIWidgetComponent* widget_) const;

    std::vector<WidgetEntry> widgets;
    std::vector<std::shared_ptr<GameObject>> boardObjects;
    std::weak_ptr<GameObject> selectedObject;
    float hierarchyWidth = 280.0f;
    float inspectorWidth = 360.0f;
    float animationHeight = 180.0f;
    float previewZoom = 1.0f;
    ImVec2 previewPan = ImVec2(0.0f, 0.0f);
    bool previewAutoFit = true;
    bool autoRefresh = true;
};
