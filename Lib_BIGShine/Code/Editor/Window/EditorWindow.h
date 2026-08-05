// EditorWindow.h
#pragma once
#include <string>
#include <memory>
#include <imgui.h>
#include <imgui_internal.h>
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

// EditorWindowごとの簡易スタイル。Window背景とTitleBarを個別に調整できるようにする。
struct WindowStyle {
    ImVec4 backgroundColor = ImVec4(0.1f, 0.1f, 0.1f, 0.35f);
    ImVec4 titleBarColor = ImVec4(0.2f, 0.3f, 0.4f, 1.0f);
};

class EditorWindow {
public:
    EditorWindow(const std::string& name, GameObjectManager* manager)
        : windowName(name), gameObjectManager(manager), isVisible(true)
    {
    }
    virtual ~EditorWindow() = default;

    virtual void Draw() = 0; // 各Window固有の描画処理。
    virtual void Update(float deltaTime_) {}
    virtual void DrawMenuBar() {}
    virtual void OnVisibilityChanged(bool visible) {}
    // 専用Workspaceとして扱うWindowはScene EditorのDock表示とは別枠で表示する。
    virtual bool IsEditorWorkspace() const { return false; }
    virtual bool ShouldShowInWindowMenu() const { return true; }
    // Animation/Particleなど、編集中にScene更新を止めたいWindowがtrueを返す。
    virtual bool ShouldPauseScene() const { return false; }
    virtual bool ShouldRenderSceneWhenPaused() const { return false; }

    // Windowの表示状態を確認・設定する。変更時はOnVisibilityChangedで派生側へ通知する。
    bool IsVisible() const { return isVisible; }
    void SetVisible(bool visible) { if (isVisible != visible) { isVisible = visible; OnVisibilityChanged(visible); } }
    bool IsKeepOnTop() const { return keepOnTop; }
    void SetKeepOnTop(bool enabled) { keepOnTop = enabled; }
    bool IsLockToMainViewport() const { return lockToMainViewport; }
    void SetLockToMainViewport(bool enabled) { lockToMainViewport = enabled; }
    void SetLayoutRect(const ImVec2& pos, const ImVec2& size, bool locked = true) { layoutPos = pos; layoutSize = size; hasLayoutRect = true; layoutLocked = locked; }
    void ClearLayoutRect() { hasLayoutRect = false; }
    void SetDockId(ImGuiID id) { dockId = id; hasDockId = (id != 0); }
    void ClearDockId() { dockId = 0; hasDockId = false; }
    static void SetMenuInteractionActive(bool active) { menuInteractionActive = active; }

    const std::string& GetName() const { return windowName; }
    WindowStyle& GetStyle() { return style; }
protected:
    void ApplyStyleToGlobalImGui()
    {
        ImGuiStyle& imguiStyle = ImGui::GetStyle();
        imguiStyle.Colors[ImGuiCol_WindowBg] = style.backgroundColor;
        imguiStyle.Colors[ImGuiCol_ChildBg] = style.backgroundColor;
        imguiStyle.Colors[ImGuiCol_DockingEmptyBg] = style.backgroundColor;
        imguiStyle.Colors[ImGuiCol_TitleBg] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_TitleBgActive] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_TitleBgCollapsed] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_Tab] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_TabActive] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_TabHovered] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_TabUnfocused] = style.titleBarColor;
        imguiStyle.Colors[ImGuiCol_TabUnfocusedActive] = style.titleBarColor;
    }

    WindowStyle style;
    // 共通のスタイル編集UIを描画する関数。
    void DrawStylePopup() {
        if (ImGui::BeginPopupContextWindow("WindowSettings", ImGuiPopupFlags_MouseButtonRight)) {
            ImGui::Text("Style Settings");
            ImGui::Separator();
            ImGui::Checkbox("Keep On Top", &keepOnTop);
            ImGui::Checkbox("Use Main Viewport", &lockToMainViewport);
            ImGui::Separator();
            bool styleChanged = false;
            styleChanged |= ImGui::ColorEdit4("Background", (float*)&style.backgroundColor);
            styleChanged |= ImGui::ColorEdit4("Title Bar", (float*)&style.titleBarColor);
            if (styleChanged)
            {
                ApplyStyleToGlobalImGui();
            }

            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    bool BeginEditorWindow(const char* overrideName = nullptr, ImGuiWindowFlags flags = 0)
    {
        // 基本的にEditor WindowはMainViewportへ固定し、MultiViewport時も親Windowからはみ出しにくくする。
        if (const ImGuiViewport* mainViewport = ImGui::GetMainViewport())
        {
            ImGui::SetNextWindowViewport(mainViewport->ID);
        }

        if (!hasLayoutRect && !hasDockId)
        {
            // 明示レイアウトやDock指定がないWindowは、初回だけ標準サイズを指定する。
            ImGui::SetNextWindowSize(
                defaultWindowSize,
                ImGuiCond_FirstUseEver);
        }

        if (hasLayoutRect)
        {
            // Workspace内の固定PanelなどはLayoutRectで位置とサイズを直接指定する。
            ImGuiCond layoutCond = layoutLocked ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
            ImGui::SetNextWindowPos(layoutPos, layoutCond);
            ImGui::SetNextWindowSize(layoutSize, layoutCond);
            if (layoutLocked)
            {
                flags |= ImGuiWindowFlags_NoMove;
            }
        }
        else if (hasDockId)
        {
            ImGui::SetNextWindowDockID(dockId, ImGuiCond_Once);
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, style.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, style.backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, style.titleBarColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, style.titleBarColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, style.titleBarColor);
        ImGui::PushStyleColor(ImGuiCol_Tab, style.titleBarColor);
        ImGui::PushStyleColor(ImGuiCol_TabActive, style.titleBarColor);
        ImGui::PushStyleColor(ImGuiCol_TabHovered, style.titleBarColor);
        ImGui::SetNextWindowBgAlpha(style.backgroundColor.w);

        bool open = ImGui::Begin(overrideName ? overrideName : windowName.c_str(), &isVisible, flags);
        ImGuiContext* context = ImGui::GetCurrentContext();
        const bool hasOpenPopup = context && context->OpenPopupStack.Size > 0;
        const bool clickedWindow = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (keepOnTop && !hasOpenPopup && !menuInteractionActive && clickedWindow)
        {
            // Windowをクリックした時だけ前面へ出し、メニュー操作中やPopup操作中のZ順暴れを避ける。
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        }

        return open;
    }

    void EndEditorWindow()
    {
        ImGui::End();
        ImGui::PopStyleColor(8);
    }

    ImVec2 defaultWindowSize = ImVec2(400.0f, 600.0f);
    std::string windowName;
    GameObjectManager* gameObjectManager; // Editor操作対象のObject/Sceneへアクセスするための参照。
    bool isVisible; // Windowを表示するかどうか。
    bool keepOnTop = true; // クリックしたWindowを前面へ出すか。
    bool lockToMainViewport = true; // MainViewport利用設定。現在のWindow生成ではMainViewport指定を標準にしている。
    bool hasLayoutRect = false; // Workspaceなどから固定位置を指定されているか。
    bool layoutLocked = true; // 固定レイアウト時にユーザー移動を許可しないか。
    ImVec2 layoutPos = ImVec2(0.0f, 0.0f);
    ImVec2 layoutSize = ImVec2(0.0f, 0.0f);
    bool hasDockId = false;
    ImGuiID dockId = 0;
    inline static bool menuInteractionActive = false;
};
