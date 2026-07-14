#include "EditorWindowManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <string>
#include <vector>

void EditorWindowManager::ApplyEngineLayout(ImVec2& scenePos, ImVec2& sceneSize)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;

    const float workspaceSelectorHeight = GetWorkspaceSelectorHeight();
    const ImVec2 workPos = viewport->WorkPos;
    const ImVec2 workSize = viewport->WorkSize;

    const float top = workPos.y + workspaceSelectorHeight;
    const float usableHeight = (std::max)(120.0f, workSize.y - workspaceSelectorHeight);
    leftPanelWidth = (std::max)(180.0f, (std::min)(leftPanelWidth, workSize.x * 0.45f));
    rightPanelWidth = (std::max)(220.0f, (std::min)(rightPanelWidth, workSize.x * 0.45f));
    bottomPanelHeight = (std::max)(120.0f, (std::min)(bottomPanelHeight, usableHeight * 0.65f));
    const float leftWidth = leftPanelWidth;
    const float rightWidth = rightPanelWidth;
    const float bottomHeight = bottomPanelHeight;
    const float centerWidth = (std::max)(120.0f, workSize.x - leftWidth - rightWidth);
    const float sceneHeight = (std::max)(90.0f, usableHeight - bottomHeight);

    const ImVec2 hierarchyPos(workPos.x, top);
    const ImVec2 hierarchySize(leftWidth, usableHeight);
    const ImVec2 inspectorPos(workPos.x + workSize.x - rightWidth, top);
    const ImVec2 inspectorSize(rightWidth, usableHeight);
    bottomDockPos = ImVec2(workPos.x + leftWidth, top + sceneHeight);
    bottomDockSize = ImVec2(centerWidth, bottomHeight);

    scenePos = ImVec2(workPos.x + leftWidth, top);
    sceneSize = ImVec2(centerWidth, sceneHeight);

    const float tabHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f + 4.0f;
    for (auto& window : windows)
    {
        window->ClearDockId();
        window->ClearLayoutRect();

        const std::string& name = window->GetName();
        if (name == "Hierarchy")
        {
            window->SetLayoutRect(hierarchyPos, hierarchySize);
        }
        else if (name == "InspectorWindow")
        {
            window->SetLayoutRect(inspectorPos, inspectorSize);
        }
    }
}


void EditorWindowManager::DrawLayoutSplitters()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;

    const float workspaceSelectorHeight = GetWorkspaceSelectorHeight();
    const ImVec2 workPos = viewport->WorkPos;
    const ImVec2 workSize = viewport->WorkSize;
    const float top = workPos.y + workspaceSelectorHeight;
    const float usableHeight = (std::max)(120.0f, workSize.y - workspaceSelectorHeight);
    const float splitterSize = 6.0f;

    auto drawVerticalSplitter = [&](const char* name, float x, bool leftSide)
        {
            ImGui::SetNextWindowPos(ImVec2(x - splitterSize * 0.5f, top), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(splitterSize, usableHeight), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoBackground;

            if (ImGui::Begin(name, nullptr, flags))
            {
                ImGui::InvisibleButton("##splitter", ImVec2(splitterSize, usableHeight));
                if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
                if (ImGui::IsItemActive())
                {
                    const float delta = ImGui::GetIO().MouseDelta.x;
                    if (leftSide)
                    {
                        leftPanelWidth += delta;
                    }
                    else
                    {
                        rightPanelWidth -= delta;
                    }
                }
            }
            ImGui::End();
        };

    auto drawHorizontalSplitter = [&]()
        {
            ImGui::SetNextWindowPos(ImVec2(bottomDockPos.x, bottomDockPos.y - splitterSize * 0.5f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(bottomDockSize.x, splitterSize), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoBackground;

            if (ImGui::Begin("Bottom Splitter", nullptr, flags))
            {
                ImGui::InvisibleButton("##splitter", ImVec2(bottomDockSize.x, splitterSize));
                if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                }
                if (ImGui::IsItemActive())
                {
                    bottomPanelHeight -= ImGui::GetIO().MouseDelta.y;
                }
            }
            ImGui::End();
        };

    drawVerticalSplitter("Hierarchy Splitter", workPos.x + leftPanelWidth, true);
    drawVerticalSplitter("Inspector Splitter", workPos.x + workSize.x - rightPanelWidth, false);
    drawHorizontalSplitter();
}

void EditorWindowManager::DrawSceneView(const ImVec2& pos, const ImVec2& size)
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.07f, 0.08f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.0f);

    if (ImGui::Begin("Scene", nullptr, flags))
    {
        ID3D11ShaderResourceView* sceneSRV = GraphicsDeviceManager::Instance().GetSceneSRV();
        const ImVec2 available = ImGui::GetContentRegionAvail();
        if (sceneSRV && available.x > 1.0f && available.y > 1.0f)
        {
            ImGui::Image((void*)sceneSRV, available);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void EditorWindowManager::DrawMenuBar()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Scene")) {
            // 状態を直接書き換える
            if (ImGui::MenuItem("Save Scene")) { gameObjectManager->GetSceneState().showSavePopup = true; }
            if (ImGui::MenuItem("Load Scene")) { gameObjectManager->GetSceneState().showLoadPopup = true; }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            bool keepAllOnTop = true;
            for (auto& window : windows) {
                keepAllOnTop = keepAllOnTop && window->IsKeepOnTop();
            }
            if (ImGui::MenuItem("Keep Editor Windows On Top", nullptr, &keepAllOnTop)) {
                for (auto& window : windows) {
                    window->SetKeepOnTop(keepAllOnTop);
                }
            }

            bool useMainViewport = true;
            for (auto& window : windows) {
                useMainViewport = useMainViewport && window->IsLockToMainViewport();
            }
            if (ImGui::MenuItem("Use Main Viewport", nullptr, &useMainViewport)) {
                for (auto& window : windows) {
                    window->SetLockToMainViewport(useMainViewport);
                }
            }
            ImGui::Separator();

            // 各窓の状態を切り替えるメニューを生成
            for (auto& window : windows) {
                bool visible = window->IsVisible();
                if (ImGui::MenuItem(window->GetName().c_str(), nullptr, &visible)) {
                    window->SetVisible(visible);

                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

float EditorWindowManager::GetWorkspaceSelectorHeight() const
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
}

void EditorWindowManager::DrawWorkspaceSelectorBar()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) return;

    const float selectorHeight = GetWorkspaceSelectorHeight();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, selectorHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.95f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("Workspace Selector", nullptr, flags))
    {
        ImGui::TextUnformatted("Workspace");
        ImGui::SameLine();
        ImGui::SetNextItemWidth((std::min)(360.0f, viewport->WorkSize.x - 120.0f));

        const std::string activeName_ = GetActiveWorkspaceName();
        if (ImGui::BeginCombo("##WorkspaceSelect", activeName_.c_str()))
        {
            if (ImGui::Selectable("Scene Editor", activeWorkspaceIndex < 0))
            {
                SetActiveWorkspace(-1);
            }

            for (int index_ = 0; index_ < static_cast<int>(windows.size()); ++index_)
            {
                const auto& window_ = windows[index_];
                if (!window_->IsEditorWorkspace()) continue;

                const bool selected_ = activeWorkspaceIndex == index_;
                if (ImGui::Selectable(window_->GetName().c_str(), selected_))
                {
                    SetActiveWorkspace(index_);
                }
                if (selected_) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}

void EditorWindowManager::SetActiveWorkspace(int workspaceIndex_)
{
    activeWorkspaceIndex = workspaceIndex_;
    for (int index_ = 0; index_ < static_cast<int>(windows.size()); ++index_)
    {
        if (windows[index_]->IsEditorWorkspace())
        {
            windows[index_]->SetVisible(index_ == activeWorkspaceIndex);
        }
    }
}

std::string EditorWindowManager::GetActiveWorkspaceName() const
{
    if (activeWorkspaceIndex < 0 ||
        activeWorkspaceIndex >= static_cast<int>(windows.size()) ||
        !windows[activeWorkspaceIndex]->IsEditorWorkspace())
    {
        return "Scene Editor";
    }

    return windows[activeWorkspaceIndex]->GetName();
}
void EditorWindowManager::Update(float deltaTime_)
{
    if (activeWorkspaceIndex < 0 || activeWorkspaceIndex >= static_cast<int>(windows.size())) return;
    if (!windows[activeWorkspaceIndex]->IsEditorWorkspace()) return;

    windows[activeWorkspaceIndex]->Update(deltaTime_);
}

void EditorWindowManager::DrawAll()
{
    if (activeWorkspaceIndex >= static_cast<int>(windows.size()) ||
        (activeWorkspaceIndex >= 0 && !windows[activeWorkspaceIndex]->IsEditorWorkspace()))
    {
        activeWorkspaceIndex = -1;
    }

    const bool hasActiveWorkspace = activeWorkspaceIndex >= 0;
    DrawWorkspaceSelectorBar();

    if (!hasActiveWorkspace)
    {
        ImVec2 scenePos(0.0f, 0.0f);
        ImVec2 sceneSize(0.0f, 0.0f);
        ApplyEngineLayout(scenePos, sceneSize);
        //DrawSceneView(scenePos, sceneSize);

        DrawLayoutSplitters();
    }

    for (int index_ = 0; index_ < static_cast<int>(windows.size()); ++index_) {
        auto& window = windows[index_];

        if (!window->IsVisible()) {
            continue;
        }

        if (hasActiveWorkspace && index_ != activeWorkspaceIndex) {
            continue;
        }

        if (!hasActiveWorkspace && window->IsEditorWorkspace()) {
            continue;
        }

        {
            const std::string profileName_ = "EditorWindow::" + window->GetName();
            BS_PROFILE_SCOPE_DYNAMIC(profileName_.c_str());
            window->Draw();
        }
    }

    if (!hasActiveWorkspace)
    {
        gameObjectManager->DrawSceneModals();
    }

    EditorWindow::SetMenuInteractionActive(true);
    DrawMenuBar();
    EditorWindow::SetMenuInteractionActive(false);
}

