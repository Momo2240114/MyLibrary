#include "TimeManagerWindow.h"

#include "Engine/GamePlay/System/TimeManager.h"

void TimeManagerWindow::Draw()
{
    if (!isVisible) return;

    if (BeginEditorWindow())
    {
        DrawStylePopup();

        const std::shared_ptr<TimeManager> timeManager_ =
            gameObjectManager ? gameObjectManager->GetTimeManager() : nullptr;
        if (!timeManager_)
        {
            ImGui::TextDisabled("TimeManager is not available.");
            EndEditorWindow();
            return;
        }

        const float currentScale_ = timeManager_->GetCachedScale(
            static_cast<uint32_t>(TarGetLayerType::Player));
        ImGui::Text("Current Scale: %.3f", currentScale_);

        bool changed_ = ImGui::SliderFloat(
            "Target Scale",
            &targetScale,
            0.0f,
            2.0f,
            "%.2fx");
        changed_ |= ImGui::DragFloat(
            "Transition Speed",
            &transitionSpeed,
            0.1f,
            0.01f,
            100.0f,
            "%.2f");

        if (changed_)
        {
            ApplyTimeScale();
        }

        if (ImGui::Button("Pause"))
        {
            targetScale = 0.0f;
            ApplyTimeScale();
        }
        ImGui::SameLine();
        if (ImGui::Button("0.25x"))
        {
            targetScale = 0.25f;
            ApplyTimeScale();
        }
        ImGui::SameLine();
        if (ImGui::Button("0.5x"))
        {
            targetScale = 0.5f;
            ApplyTimeScale();
        }
        ImGui::SameLine();
        if (ImGui::Button("1.0x"))
        {
            targetScale = 1.0f;
            ApplyTimeScale();
        }
    }
    EndEditorWindow();
}

void TimeManagerWindow::ApplyTimeScale()
{
    if (!gameObjectManager) return;

    const std::shared_ptr<TimeManager> timeManager_ = gameObjectManager->GetTimeManager();
    if (!timeManager_) return;

    timeManager_->SetTargetScale(
        static_cast<uint32_t>(TarGetLayerType::All),
        targetScale,
        transitionSpeed);
}
