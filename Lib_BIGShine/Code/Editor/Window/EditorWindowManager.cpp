#include "PrecompiledHeader.h"

#include "EditorWindowManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Editor/GamePackageExporter.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Platform/WindowsDialogHelper.h"

#include <algorithm>
#include <string>
#include <vector>

void EditorWindowManager::BuildDefaultDockLayout(ImGuiID dockspaceId_, const ImVec2& dockspaceSize_)
{
    BS_PROFILE_SCOPE("EditorWindowManager::BuildDefaultDockLayout");

    // Dock情報が無い時の標準配置を作り、Sceneを中央、Hierarchy/Inspector/Toolを周囲へ分ける。
    ImGui::DockBuilderRemoveNode(dockspaceId_);
    ImGui::DockBuilderAddNode(dockspaceId_, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId_, dockspaceSize_);

    ImGuiID centerDockId_ = dockspaceId_;
    leftDockId = ImGui::DockBuilderSplitNode(centerDockId_, ImGuiDir_Left, 0.20f, nullptr, &centerDockId_);
    rightDockId = ImGui::DockBuilderSplitNode(centerDockId_, ImGuiDir_Right, 0.24f, nullptr, &centerDockId_);
    bottomDockId = ImGui::DockBuilderSplitNode(centerDockId_, ImGuiDir_Down, 0.32f, nullptr, &sceneDockId);

    ImGui::DockBuilderDockWindow("Scene", sceneDockId);
    for (const auto& window_ : windows)
    {
        if (window_->IsEditorWorkspace())
        {
            continue;
        }

        const std::string& name_ = window_->GetName();
        if (name_ == "Hierarchy")
        {
            ImGui::DockBuilderDockWindow(name_.c_str(), leftDockId);
        }
        else if (name_ == "InspectorWindow")
        {
            ImGui::DockBuilderDockWindow(name_.c_str(), rightDockId);
        }
        else
        {
            ImGui::DockBuilderDockWindow(name_.c_str(), bottomDockId);
        }
    }

    ImGui::DockBuilderFinish(dockspaceId_);
}

void EditorWindowManager::DrawSceneEditorDockSpace()
{
    BS_PROFILE_SCOPE("EditorWindowManager::DrawSceneEditorDockSpace");

    const ImGuiViewport* viewport_ = ImGui::GetMainViewport();
    if (!viewport_)
    {
        return;
    }

    const float workspaceSelectorHeight_ = GetWorkspaceSelectorHeight();
    ImVec2 dockspacePos_ = viewport_->WorkPos;
    ImVec2 dockspaceSize_ = viewport_->WorkSize;
    // 上部のWorkspace Selector分だけDockSpaceを下げ、Scene Editorの領域と重ならないようにする。
    dockspacePos_.y += workspaceSelectorHeight_;
    dockspaceSize_.y = (std::max)(1.0f, dockspaceSize_.y - workspaceSelectorHeight_);

    ImGuiWindowFlags windowFlags_ = ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoBackground;

    ImGui::SetNextWindowPos(dockspacePos_, ImGuiCond_Always);
    ImGui::SetNextWindowSize(dockspaceSize_, ImGuiCond_Always);
    ImGui::SetNextWindowViewport(viewport_->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_DockingEmptyBg, ImVec4(0.1f, 0.1f, 0.1f, 0.18f));
    ImGui::SetNextWindowBgAlpha(0.0f);

    if (ImGui::Begin("Scene Editor DockSpace", nullptr, windowFlags_))
    {
        ImGuiID dockspaceId_ = ImGui::GetID("SceneEditorDockSpace");
        if (!ImGui::DockBuilderGetNode(dockspaceId_))
        {
            // Dock情報がまだない場合だけ標準レイアウトを構築する。既存のユーザー配置は維持する。
            BuildDefaultDockLayout(dockspaceId_, dockspaceSize_);
        }

        ImGui::DockSpace(dockspaceId_, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

void EditorWindowManager::DrawSceneView()
{
    if (sceneDockId != 0)
    {
        ImGui::SetNextWindowDockID(sceneDockId, ImGuiCond_FirstUseEver);
    }

    ImGuiWindowFlags flags_ = ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("Scene", nullptr, flags_))
    {
        DrawPlayModeToolbar();
        auto& graphicsDeviceManager_ = GraphicsDeviceManager::Instance();
        ID3D11ShaderResourceView* sceneSRV_ = graphicsDeviceManager_.GetSceneViewSRV();
        const ImVec2 available_ = ImGui::GetContentRegionAvail();
        if (sceneSRV_ && available_.x > 1.0f && available_.y > 1.0f)
        {
            // RenderTargetのアスペクト比を保ったまま、Dock領域の中央へScene画像を表示する。
            const float renderWidth_ = static_cast<float>(graphicsDeviceManager_.GetRenderWidth());
            const float renderHeight_ = static_cast<float>(graphicsDeviceManager_.GetRenderHeight());
            ImVec2 imageSize_ = available_;
            if (renderWidth_ > 0.0f && renderHeight_ > 0.0f)
            {
                const float renderAspect_ = renderWidth_ / renderHeight_;
                const float availableAspect_ = available_.x / available_.y;
                if (availableAspect_ > renderAspect_)
                {
                    imageSize_.x = available_.y * renderAspect_;
                }
                else
                {
                    imageSize_.y = available_.x / renderAspect_;
                }
            }

            const ImVec2 cursorPos_ = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(
                cursorPos_.x + (available_.x - imageSize_.x) * 0.5f,
                cursorPos_.y + (available_.y - imageSize_.y) * 0.5f));
            ImGui::Image(reinterpret_cast<void*>(sceneSRV_), imageSize_);
            if (gameObjectManager && gameObjectManager->SupportsEditorPlayMode() &&
                gameObjectManager->GetEditorPlayMode() == EditorPlayMode::PLAY &&
                ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                gameObjectManager->SetPlayInputCaptured(true);
            }
            if (gameObjectManager && gameObjectManager->SupportsEditorPlayMode())
            {
                if (gameObjectManager->GetEditorPlayMode() == EditorPlayMode::EDITOR)
                {
                    if (const auto camera_ = gameObjectManager->GetCameraManager()->GetEditorCamera())
                    {
                        if (!sceneEditorCameraCaptured || (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
                        {
                            sceneEditorCameraData.CaptureFromCamera(*camera_);
                            sceneEditorCameraCaptured = true;
                        }
                        sceneEditorCameraData.UpdatePreviewFreeCamera(*camera_, ImGui::GetIO().DeltaTime, ImGui::IsItemHovered());
                    }
                }
                else
                {
                    sceneEditorCameraCaptured = false;
                }
            }
        }
    }
    ImGui::End();
}

void EditorWindowManager::DrawPlayModeToolbar()
{
    BS_PROFILE_SCOPE("EditorPlayMode::Toolbar");
    if (!gameObjectManager)
    {
        return;
    }
    const bool supported_ = gameObjectManager->SupportsEditorPlayMode();
    const auto mode_ = gameObjectManager->GetEditorPlayMode();
    constexpr float BUTTON_WIDTH = 72.0f;
    const float toolbarWidth_ = BUTTON_WIDTH * 3.0f + ImGui::GetStyle().ItemSpacing.x * 2.0f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (std::max)(0.0f, (ImGui::GetContentRegionAvail().x - toolbarWidth_) * 0.5f));
    ImGui::BeginDisabled(!supported_ || !gameObjectManager->CanStartEditorPlay());
    if (ImGui::Button("Play", ImVec2(BUTTON_WIDTH, 0)))
    {
        gameObjectManager->RequestEditorPlayCommand(EditorPlayCommand::PLAY);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!supported_ || mode_ == EditorPlayMode::EDITOR);
    if (ImGui::Button(mode_ == EditorPlayMode::PAUSED ? "Resume" : "Pause", ImVec2(BUTTON_WIDTH, 0)))
    {
        gameObjectManager->RequestEditorPlayCommand(EditorPlayCommand::PAUSE);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(BUTTON_WIDTH, 0)))
    {
        gameObjectManager->RequestEditorPlayCommand(EditorPlayCommand::STOP);
    }
    ImGui::EndDisabled();
    const char* status_ = !supported_ ? "Runtime" : mode_ == EditorPlayMode::EDITOR ? "Editor" :
        mode_ == EditorPlayMode::PAUSED ? "Paused - mouse released" :
        gameObjectManager->IsPlayInputCaptured() ? "Play - mouse captured / F2 or Esc to release" :
        "Play - click Scene or press F2 to capture mouse";
    ImGui::TextDisabled("%s", status_);
    ImGui::Separator();
}

const char* EditorWindowManager::GetWindowMenuCategory(
    const EditorWindow& window_) const
{
    const std::string& name_ = window_.GetName();

    // Windowメニューを用途別に整理し、増えたEditor Windowを探しやすくする。
    if (window_.IsEditorWorkspace())
    {
        return "Workspace";
    }
    if (name_ == "Hierarchy" || name_ == "InspectorWindow")
    {
        return "Core";
    }
    if (name_ == "Shader" ||
		name_ == "GPU Render Investigation" ||
        name_ == "Editor Scene View" ||
		name_ == "Shadow" ||
		name_ == "Post Effect" ||
        name_ == "IBL Settings" ||
		name_ == "Debug Texture Viewer")
	{
		return "Rendering";
	}
    if (name_ == "Input Editor" || name_ == "Input Monitor")
    {
        return "Input";
    }
    if (name_ == "Asset Browser" ||
        name_ == "Curve Asset Editor" ||
        name_ == "Easing Preset Editor" ||
        name_ == "Effect Asset Keys" ||
        name_ == "Effect Asset List" ||
        name_ == "Material Asset Keys" ||
        name_ == "VFX Material Editor" ||
        name_ == "VFX Material Preview" ||
        name_ == "Startup Model Settings")
    {
        return "Assets";
    }
    if (name_ == "Animation Editor" ||
        name_ == "Attack Template Editor" ||
        name_ == "Animation Node Graph" ||
        name_ == "Action Sequence Editor")
    {
        return "Animation";
    }
    if (name_ == "Debug Log" ||
        name_ == "Node Graph Monitor" ||
        name_ == "Component Update Sequencer")
    {
        return "Debug";
    }
    return "Tools";
}

// 書き出しは明示操作だけで実行し、処理結果と保存先を同じWindowに残す。
void EditorWindowManager::DrawGameExport()
{
    if (!showGameExport)
    {
        return;
    }
    BS_PROFILE_SCOPE("EditorWindowManager::DrawGameExport");
    ImGui::SetNextWindowSize(ImVec2(620.0f, 380.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Export Game", &showGameExport))
    {
        ImGui::TextWrapped("Export the scene used when Play started, with Data, runtime DLLs and ReadMe.txt.");
        ImGui::TextWrapped("Build the updated project as Shipping | x64 first, then select that EXE. This tool does not build or run it.");
        ImGui::TextWrapped("Output: Exports/ReviewGame_<date and time>/Game.exe. Copying all Data may take time.");
        ImGui::Separator();
        ImGui::TextUnformatted("Game EXE Icon (.ico)");
        ImGui::TextWrapped("Saved icon: Data/Icons/ANATHEMA/ANATHEMA.ico");
        ImGui::TextWrapped("After saving, build Game_Debug, Game_Release or Shipping to embed the icon. Engine keeps its own icon. Existing EXEs are unchanged until rebuilt.");
        // ファイル自体をプロジェクト内へ保存するため、再起動後も同じICOがビルドに使われる。
        if (ImGui::Button("Select ICO and save for Game"))
        {
            const auto icon_ = WindowsDialogHelper::OpenFileDialog(
                L"Select Game EXE icon", {{L"Windows icon", L"*.ico"}}, "Data/Icons");
            if (!icon_.empty())
            {
                std::string error_;
                const bool success_ = GamePackageExporter::SaveGameIcon(
                    WindowsDialogHelper::Utf8ToPath(icon_), error_);
                gameExportStatus = success_ ? "Game icon saved. Build a Game configuration or Shipping, then export the rebuilt EXE."
                    : "Game icon save failed: " + error_;
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Select built EXE and export"))
        {
            const auto executable_ = WindowsDialogHelper::OpenFileDialog(
                L"Select a built Shipping / Release EXE", {{L"Game executable", L"*.exe"}}, "x64/Shipping");
            if (!executable_.empty())
            {
                std::string output_;
                std::string error_;
                const bool success_ = GamePackageExporter::Export(*gameObjectManager,
                    WindowsDialogHelper::Utf8ToPath(executable_), output_, error_);
                gameExportStatus = success_ ? "Export complete: " + output_ : "Export failed: " + error_;
            }
        }
        ImGui::Separator();
        ImGui::TextWrapped("%s", gameExportStatus.c_str());
    }
    ImGui::End();
}

void EditorWindowManager::DrawMenuBar()
{
    BS_PROFILE_SCOPE("EditorWindowManager::DrawMenuBar");
    DrawGameExport();
    bool requestTitle_ = false;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Scene")) {
            // 状態を直接書き換える
            if (ImGui::MenuItem("Save Scene"))
            {
                gameObjectManager->GetSceneState().showSavePopup = true;
            }
            if (ImGui::MenuItem("Load Scene"))
            {
                gameObjectManager->GetSceneState().showLoadPopup = true;
            }
            if (ImGui::MenuItem("Export Game..."))
            {
                showGameExport = true;
            }
            ImGui::Separator();
            // Play中の一時状態を編集Sceneへ保存しないよう、Stop後だけ離脱を受け付ける。
            const bool canReturn_ = gameObjectManager && gameObjectManager->CanStartEditorPlay() &&
                !gameObjectManager->IsCinematicSequencePreview() && !SceneManager::IsTransitionActive() && !SceneManager::IsEditorPlaySessionActive();
            if (ImGui::MenuItem("Return to Title...", nullptr, false, canReturn_))
            {
                requestTitle_ = true;
            }
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
            ImGui::MenuItem("Use Docked Scene View", nullptr, &useDockedSceneView);
            ImGui::Separator();

            // 各窓の状態を切り替えるメニューを生成
            const char* categories_[] =
            {
                "Core",
                "Workspace",
                "Input",
                "Animation",
                "Assets",
                "Rendering",
                "Debug",
                "Tools"
            };

            for (const char* category_ : categories_) {
                bool hasCategoryWindow_ = false;
                for (const auto& window_ : windows) {
                    if (!window_->ShouldShowInWindowMenu())
                    {
                        continue;
                    }
                    if (std::string(GetWindowMenuCategory(*window_)) == category_) {
                        hasCategoryWindow_ = true;
                        break;
                    }
                }
                if (!hasCategoryWindow_)
                {
                    continue;
                }

                if (ImGui::BeginMenu(category_)) {
                    for (auto& window_ : windows) {
                        if (!window_->ShouldShowInWindowMenu())
                        {
                            continue;
                        }
                        if (std::string(GetWindowMenuCategory(*window_)) != category_)
                        {
                            continue;
                        }

                        bool visible_ = window_->IsVisible();
                        if (ImGui::MenuItem(window_->GetName().c_str(), nullptr, &visible_)) {
                            window_->SetVisible(visible_);
                        }
                    }
                    ImGui::EndMenu();
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    if (requestTitle_)
    {
        ImGui::OpenPopup("Return to Title##Editor");
    }
    if (ImGui::BeginPopupModal("Return to Title##Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Unsaved scene changes will be lost.");
        ImGui::TextUnformatted("Use Save Scene before returning if you want to keep your edits.");
        if (ImGui::Button("Save Scene..."))
        {
            gameObjectManager->GetSceneState().showSavePopup = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Return Without Saving"))
        {
            // Sceneの具象型へ依存せず、既存Actionにタイトルへの通常遷移を委ねる。
            ActionRegistry::Instance().Call("GoSceneTitle");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

float EditorWindowManager::GetWorkspaceSelectorHeight() const
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
}

void EditorWindowManager::DrawWorkspaceSelectorBar()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport)
    {
        return;
    }

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
        // Scene Editorと専用Workspaceを切り替え、重い専用Editorを必要な時だけ表示する。
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
                if (!window_->IsEditorWorkspace())
                {
                    continue;
                }

                const bool selected_ = activeWorkspaceIndex == index_;
                if (ImGui::Selectable(window_->GetName().c_str(), selected_))
                {
                    SetActiveWorkspace(index_);
                }
                if (selected_)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }
    ImGui::End();
}

void EditorWindowManager::SetActiveWorkspace(int workspaceIndex_)
{
    activeWorkspaceIndex = workspaceIndex_;
    // Workspace型Windowは同時表示せず、選択中のものだけVisibleにする。
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
    // 他Workspaceが対象を取得する前に試写Sceneを戻し、生成物の参照を引き継がせない。
    if (gameObjectManager && gameObjectManager->HasCinematicSequenceStopRequest())
    {
        gameObjectManager->Update(0.0f);
    }
    // ComponentのGUI走査中にWorkspaceを切り替えず、次の更新開始時に対象を渡す。
    std::string ownerName_;
    int sequenceIndex_ = -1;
    if (gameObjectManager && gameObjectManager->ConsumeCinematicEditorRequest(ownerName_, sequenceIndex_))
    {
        for (int index_ = 0; index_ < static_cast<int>(windows.size()); ++index_)
        {
            if (windows[index_]->OpenCinematicSequence(ownerName_, sequenceIndex_))
            {
                SetActiveWorkspace(index_);
                break;
            }
        }
    }
    if (activeWorkspaceIndex < 0 || activeWorkspaceIndex >= static_cast<int>(windows.size()))
    {
        return;
    }
    if (!windows[activeWorkspaceIndex]->IsEditorWorkspace())
    {
        return;
    }

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

    // 通常Scene編集時はDockSpaceとSceneViewを表示し、専用Workspace時はそのWindowだけを前面に出す。
    if (!hasActiveWorkspace)
    {
        DrawSceneEditorDockSpace();
        if (useDockedSceneView)
        {
            DrawSceneView();
        }
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
            // Window単位でTracy計測を切り、EditorのどのPanelが重いか追えるようにする。
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

