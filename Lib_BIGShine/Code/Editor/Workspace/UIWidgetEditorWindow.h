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

    // UI専用Workspace全体を描画する。
    void Draw() override;

    // Pause中でもUI Previewを見たいのでScene描画を許可する。
    bool ShouldRenderSceneWhenPaused() const override
    {
        return true;
    }

private:
    struct WidgetEntry
    {
        // 1つのUI GameObjectから、Editorで扱う関連ComponentをまとめたCache。
        std::shared_ptr<GameObject> object;
        std::shared_ptr<UIWidgetComponent> widget;
        std::shared_ptr<SpriteRenderer> sprite;
        std::shared_ptr<WidgetBoardComponent> board;
        std::shared_ptr<UIVertexModifierComponent> vertexModifier;
        std::shared_ptr<UIAnimationPlayerComponent> animationPlayer;
    };

    // Scene内のWidget/Board情報を再収集する。
    void RefreshCache();
    // MenuBar上の操作を描画する。
    void DrawToolbar();
    // Board/Widgetの階層一覧を描画する。
    void DrawHierarchyPanel();
    // Canvas PreviewとWidget矩形を描画する。
    void DrawPreviewPanel(const ImVec2& previewSize_);
    // 選択ObjectのComponent Inspectorを描画する。
    void DrawInspectorPanel();
    // UIAnimationPlayerの編集領域を描画する。
    void DrawAnimationPanel();
    // Scene選択ObjectをUI編集対象へ変換する操作を描画する。
    void DrawSelectedObjectTools();
    // Workspace選択とScene選択を同期する。
    void SelectObject(const std::shared_ptr<GameObject>& object_);
    // 選択ObjectにUI編集で必要なComponentを追加する。
    void EnsureWidgetObject();
    // 指定Objectから親方向へBoardを探す。
    std::shared_ptr<WidgetBoardComponent> ResolveBoard(const std::shared_ptr<GameObject>& object_) const;
    // WidgetComponentが持つ親Widget参照を取得する。
    UIWidgetComponent* ResolveParentWidget(UIWidgetComponent* widget_) const;

    // Preview/Hierarchy用のCache。
    std::vector<WidgetEntry> widgets;
    std::vector<std::shared_ptr<GameObject>> boardObjects;
    std::weak_ptr<GameObject> selectedObject;
    // 3ペインLayoutの可変Size。
    float hierarchyWidth = 280.0f;
    float inspectorWidth = 360.0f;
    float animationHeight = 180.0f;
    float previewZoom = 1.0f;
    ImVec2 previewPan = ImVec2(0.0f, 0.0f);
    // trueならPreviewをCanvas全体が収まるZoom/Panに自動調整する。
    bool previewAutoFit = true;
    // trueなら毎Frame Scene状態をCacheへ反映する。
    bool autoRefresh = true;
};
