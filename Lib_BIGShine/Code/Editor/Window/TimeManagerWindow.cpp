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

        struct LayerScaleView
        {
            const char* name;
            TarGetLayerType layer;
        };

        static const LayerScaleView layerScaleViews_[] =
        {
            { "Default", TarGetLayerType::Default },
            { "Player", TarGetLayerType::Player },
            { "Enemy", TarGetLayerType::Enemy },
            { "PlayerBullet", TarGetLayerType::PlayerBullet },
            { "EnemyBullet", TarGetLayerType::EnemyBullet },
            { "UI", TarGetLayerType::UI },
            { "Environment", TarGetLayerType::Environment },
            { "SneakObject", TarGetLayerType::SneakObject },
            { "Terrain", TarGetLayerType::Terrain },
            { "Wall", TarGetLayerType::Wall },
            { "Item", TarGetLayerType::Item },
        };

        if (ImGui::BeginTable("CurrentTimeScales", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Layer");
            ImGui::TableSetupColumn("Scale");
            ImGui::TableHeadersRow();

            for (const LayerScaleView& layerScaleView_ : layerScaleViews_)
            {
                const float currentScale_ = timeManager_->GetCachedScale(
                    static_cast<uint32_t>(layerScaleView_.layer));

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(layerScaleView_.name);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", currentScale_);
            }

            ImGui::EndTable();
        }

        ImGui::SliderFloat(
            "Target Scale",
            &targetScale,
            0.0f,
            2.0f,
            "%.2fx");
        ImGui::DragFloat(
            "Transition Speed",
            &transitionSpeed,
            0.1f,
            0.01f,
            100.0f,
            "%.2f");

        if (ImGui::Button("Apply Persistent Scale"))
        {
            ApplyTimeScale();
        }

        ImGui::SeparatorText("Timed Scale");
        ImGui::SliderFloat("Default Scale", &defaultScale, 0.0f, 2.0f, "%.2fx");
        ImGui::DragFloat("Time To Target", &transitionDuration, 0.01f, 0.0f, 60.0f, "%.2f sec");
        ImGui::DragFloat("Target Hold Time", &holdDuration, 0.01f, 0.0f, 60.0f, "%.2f sec");
        ImGui::DragFloat("Time To Default", &returnDuration, 0.01f, 0.0f, 60.0f, "%.2f sec");
        if (ImGui::Button("Play Timed Scale"))
        {
            timeManager_->PlayTimedScale(
                static_cast<uint32_t>(TarGetLayerType::All),
                targetScale,
                transitionDuration,
                holdDuration,
                returnDuration,
                defaultScale);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset To Default"))
        {
            timeManager_->ResetToDefaults();
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
