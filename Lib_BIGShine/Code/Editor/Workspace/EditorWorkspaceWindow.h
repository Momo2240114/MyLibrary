#pragma once

#include "Editor/Window/EditorWindow.h"

#include <algorithm>

class EditorWorkspaceWindow : public EditorWindow
{
public:
    EditorWorkspaceWindow(const std::string& name_, GameObjectManager* manager_)
        : EditorWindow(name_, manager_)
    {
    }

    bool IsEditorWorkspace() const override { return true; }
    // Workspaceは上部Selectorから切り替えるため、通常のWindowメニューには出さない。
    bool ShouldShowInWindowMenu() const override { return false; }

protected:
    bool BeginWorkspaceWindow(ImGuiWindowFlags flags_ = 0)
    {
        // MainViewportからWorkspace Selector分を引いた領域いっぱいに作業画面を固定する。
        const ImGuiViewport* viewport_ = ImGui::GetMainViewport();
        if (viewport_)
        {
            const float workspaceSelectorHeight_ = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
            ImVec2 pos_ = viewport_->WorkPos;
            ImVec2 size_ = viewport_->WorkSize;
            pos_.y += workspaceSelectorHeight_;
            size_.y = (std::max)(1.0f, size_.y - workspaceSelectorHeight_);
            SetLayoutRect(pos_, size_, true);
        }

        flags_ |= ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

        // 通常EditorWindowの描画開始処理を使いつつ、Workspace用の固定レイアウトだけ上書きする。
        return BeginEditorWindow(nullptr, flags_);
    }
};
