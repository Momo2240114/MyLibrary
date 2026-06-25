// EditorWindow.h
#pragma once
#include <string>
#include <memory>
#include <imgui.h>
#include <imgui_internal.h>
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

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

    virtual void Draw() = 0; // 各ウィンドウ固有の描画
    virtual void Update(float deltaTime_) {}
    virtual void DrawMenuBar() {}
    virtual void OnVisibilityChanged(bool visible) {}
    virtual bool IsEditorWorkspace() const { return false; }
    virtual bool ShouldPauseScene() const { return false; }

    // この窓が表示されているかを確認し、設定する
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
    // 共通のスタイル編集UIを描画する関数
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
        if (const ImGuiViewport* mainViewport = ImGui::GetMainViewport())
        {
            ImGui::SetNextWindowViewport(mainViewport->ID);
        }

        if (!hasLayoutRect && !hasDockId)
        {
            ImGui::SetNextWindowSize(
                defaultWindowSize,
                ImGuiCond_FirstUseEver);
        }


        if (hasLayoutRect)
        {
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
    GameObjectManager* gameObjectManager; // データを扱うための窓口
    bool isVisible; // 顕現のフラグ
    bool keepOnTop = true;
    bool lockToMainViewport = true;
    bool hasLayoutRect = false;
    bool layoutLocked = true;
    ImVec2 layoutPos = ImVec2(0.0f, 0.0f);
    ImVec2 layoutSize = ImVec2(0.0f, 0.0f);
    bool hasDockId = false;
    ImGuiID dockId = 0;
    inline static bool menuInteractionActive = false;
};