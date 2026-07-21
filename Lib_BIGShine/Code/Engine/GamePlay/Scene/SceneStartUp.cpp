#include "PrecompiledHeader.h"

#include "SceneStartUp.h"
#include "SceneTitle.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Resource/ModelResourceManager.h"
#include "Engine/Common/DebugLog.h"

#include <algorithm>
#include <imgui.h>
#include <memory>

void SceneStartUp::Initialize()
{
    elapsedSeconds = 0.0f;
    titleSceneRequested = false;
    startupLoadFailed = false;

    ModelResourceManager::Instance().LoadStartupResourceSettings();
    criticalPreloadHandle = ModelResourceManager::Instance().RequestStartupCriticalModels();
    ModelResourceManager::Instance().RequestStartupHighPriorityModels();
    ModelResourceManager::Instance().StartAsyncLoader();
}

void SceneStartUp::Finalize()
{
}

void SceneStartUp::Update(float elapsedTime_)
{
    elapsedSeconds += elapsedTime_;

    ModelResourceManager::Instance().UpdateAsyncLoads(
        GraphicsDeviceManager::Instance().GetDevice(),
        2.0f);

    if (criticalPreloadHandle.IsValid() && criticalPreloadHandle.HasFailed())
    {
        startupLoadFailed = true;
        BS_LOG_ERROR(LOG_CATEGORY::GAME, "[StartUpScene] Critical startup model failed to load.");
        return;
    }

    const bool criticalReady_ =
        !criticalPreloadHandle.IsValid() ||
        criticalPreloadHandle.IsReady();

    if (criticalReady_ && !titleSceneRequested)
    {
        titleSceneRequested = true;
        SceneManager::ChangeScene(std::make_unique<SceneTitle>());
    }
}

void SceneStartUp::Render(float elapsedTime_)
{
    (void)elapsedTime_;
}

void SceneStartUp::DrawGUI()
{
    const ImGuiViewport* viewport_ = ImGui::GetMainViewport();
    if (!viewport_) return;

    ImGui::SetNextWindowPos(viewport_->WorkPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(viewport_->WorkSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);

    const ImGuiWindowFlags windowFlags_ =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav;

    if (!ImGui::Begin("StartUpScene", nullptr, windowFlags_))
    {
        ImGui::End();
        return;
    }

    const ImVec2 contentSize_ = ImGui::GetContentRegionAvail();
    const float panelWidth_ = (std::min)(520.0f, (std::max)(240.0f, contentSize_.x - 40.0f));
    const float startX_ = (contentSize_.x - panelWidth_) * 0.5f;
    const float startY_ = contentSize_.y * 0.42f;

    ImGui::SetCursorPos(ImVec2(startX_, startY_));
    ImGui::BeginGroup();

    ImGui::SetWindowFontScale(1.8f);
    ImGui::TextUnformatted("Big Shine");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Spacing();

    const float criticalProgress_ = criticalPreloadHandle.IsValid()
        ? criticalPreloadHandle.GetProgress()
        : 1.0f;
    const float totalProgress_ = ModelResourceManager::Instance().GetLoadProgress();
    const float displayProgress_ = std::clamp(
        criticalPreloadHandle.IsValid() ? criticalProgress_ : totalProgress_,
        0.0f,
        1.0f);

    if (startupLoadFailed)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "Startup load failed.");
    }
    else
    {
        ImGui::Text("Loading startup resources... %.0f%%", displayProgress_ * 100.0f);
    }

    ImGui::ProgressBar(displayProgress_, ImVec2(panelWidth_, 18.0f));

    ImGui::Spacing();
    ImGui::TextDisabled("Preparing models before TitleScene");

    const int dotCount_ = static_cast<int>(elapsedSeconds * 2.0f) % 4;
    std::string dots_(static_cast<size_t>(dotCount_), '.');
    ImGui::TextDisabled("Now Loading%s", dots_.c_str());

    ImGui::EndGroup();
    ImGui::End();
}
