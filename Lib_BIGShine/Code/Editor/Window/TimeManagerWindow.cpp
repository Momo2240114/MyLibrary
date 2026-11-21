#include "PrecompiledHeader.h"

#include "TimeManagerWindow.h"

#include "Engine/GamePlay/System/TimeManager.h"

void TimeManagerWindow::Draw()
{
    if (!isVisible)
    {
        return;
    }

    // TimeManagerのLayer別Scaleと、一時停止/フレーム送りを確認するデバッグWindow。
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
            // 表示名とTimeManager内部で使うLayerTypeを対応付ける。
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
            // 現在キャッシュされている各LayerのScaleを一覧表示する。
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

        ImGui::SeparatorText("Debug Playback");
        // Pause/Stepはデバッグ用の再生制御で、ゲーム全体を止めながら1フレームずつ確認できる。
        ImGui::Text("State: %s%s",
            timeManager_->IsPaused() ? "Paused" : "Running",
            timeManager_->IsFrameStepActive() ? " (Step)" : "");

        if (ImGui::Button(timeManager_->IsPaused() ? "Resume" : "Pause"))
        {
            timeManager_->TogglePaused();
        }
        ImGui::SameLine();
        if (ImGui::Button("Step 1 Frame"))
        {
            timeManager_->RequestFrameStep();
        }

        bool cameraUnscaledWhilePaused_ = timeManager_->IsCameraUnscaledWhilePaused();
        if (ImGui::Checkbox("Camera Moves While Paused", &cameraUnscaledWhilePaused_))
        {
            // 一時停止中でもEditor Cameraを動かしたい時だけ、Camera更新をUnscaled扱いにする。
            timeManager_->SetCameraUnscaledWhilePaused(cameraUnscaledWhilePaused_);
        }

        ImGui::SeparatorText("Scale");
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
            // Persistent Scaleは解除するまで全Layerへ目標Scaleを維持する。
            ApplyTimeScale();
        }

        ImGui::SeparatorText("Timed Scale");
        ImGui::SliderFloat("Default Scale", &defaultScale, 0.0f, 2.0f, "%.2fx");
        ImGui::DragFloat("Time To Target", &transitionDuration, 0.01f, 0.0f, 60.0f, "%.2f sec");
        ImGui::DragFloat("Target Hold Time", &holdDuration, 0.01f, 0.0f, 60.0f, "%.2f sec");
        ImGui::DragFloat("Time To Default", &returnDuration, 0.01f, 0.0f, 60.0f, "%.2f sec");
        if (ImGui::Button("Play Timed Scale"))
        {
            // Timed Scaleは遷移、保持、復帰の3区間で一時的なスロー/加速を再生する。
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
    // UIで設定した目標ScaleをAll Layerへ適用する。
    if (!gameObjectManager)
    {
        return;
    }

    const std::shared_ptr<TimeManager> timeManager_ = gameObjectManager->GetTimeManager();
    if (!timeManager_)
    {
        return;
    }

    timeManager_->SetTargetScale(
        static_cast<uint32_t>(TarGetLayerType::All),
        targetScale,
        transitionSpeed);
}
