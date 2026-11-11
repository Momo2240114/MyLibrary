#include "PrecompiledHeader.h"
#include "BossFinishWindow.h"
#include "Game/GameMode/BossFinishDirector.h"
#include <imgui_stdlib.h>

BossFinishWindow::~BossFinishWindow() { OnVisibilityChanged(false); }

void BossFinishWindow::OnVisibilityChanged(bool visible_)
{
    const auto director_ = gameObjectManager ? gameObjectManager->GetBossFinishDirector() : nullptr;
    if (!visible_ && director_ && director_->IsPreview()) director_->Stop();
}

void BossFinishWindow::DrawSettings(nlohmann::json& value_, const std::string& label_)
{
    ImGui::PushID(label_.c_str());
    if (value_.is_object() || value_.is_array())
    {
        if (ImGui::TreeNode(label_.c_str()))
        {
            for (auto iterator_ = value_.begin(); iterator_ != value_.end(); ++iterator_)
                DrawSettings(iterator_.value(), value_.is_array() ? std::to_string(iterator_-value_.begin()) : iterator_.key());
            ImGui::TreePop();
        }
    }
    else if (value_.is_string())
    {
        auto text_ = value_.get<std::string>();
        if (ImGui::InputText(label_.c_str(), &text_)) value_ = text_;
    }
    else if (value_.is_boolean())
    {
        // 命中後だけ表示する設定も、他の演出パラメータと同じ画面で編集する。
        bool enabled_ = value_.get<bool>();
        if (ImGui::Checkbox(label_.c_str(), &enabled_)) value_ = enabled_;
    }
    else if (value_.is_number_integer())
    {
        int number_ = value_.get<int>();
        if (ImGui::DragInt(label_.c_str(), &number_)) value_ = number_;
    }
    else if (value_.is_number())
    {
        float number_ = value_.get<float>();
        if (ImGui::DragFloat(label_.c_str(), &number_, 0.01f)) value_ = number_;
    }
    ImGui::PopID();
}

void BossFinishWindow::Draw()
{
    BS_PROFILE_SCOPE("BossFinishWindow::Draw");
    if (!isVisible) { OnVisibilityChanged(false); return; }
    if (BeginEditorWindow())
    {
        const auto director_ = gameObjectManager ? gameObjectManager->GetBossFinishDirector() : nullptr;
        if (director_)
        {
            const bool production_ = director_->IsActive() && !director_->IsPreview();
            ImGui::TextWrapped("Preview uses the runtime director. Stop restores scene transforms and poses. Apply edits with Restart / Seek.");
            ImGui::BeginDisabled(production_);
            if (ImGui::Button("Play / Restart")) { director_->Stop(); director_->Begin(true); }
            ImGui::SameLine();
            if (ImGui::Button("Play Bad Ending")) { director_->Stop(); director_->Begin(true, BossFinishDirector::Ending::BAD_END); }
            ImGui::SameLine();
            if (ImGui::Button("Stop + Restore")) director_->Stop();
            ImGui::SameLine();
            const bool badEnding_ = director_->IsBadEnding();
            const float end_ = badEnding_ ? director_->GetPreviewEndTime(BossFinishDirector::Ending::BAD_END) : director_->GetCaptureTime() + 5.0f;
            if (badEnding_ && director_->IsPreview() && director_->GetTime() >= end_) director_->SetPaused(true);
            if (ImGui::Button(director_->IsPaused() ? "Resume" : "Pause"))
            {
                if (badEnding_ && director_->GetTime() >= end_) director_->Seek(0.0f);
                director_->SetPaused(!director_->IsPaused());
            }
            ImGui::BeginDisabled(badEnding_);
            if (ImGui::Button("Before Capture"))
            {
                if (!director_->IsActive()) director_->Begin(true);
                director_->Seek(director_->GetEndingBranchTime() - 1.0f/60.0f);
            }
            ImGui::SameLine();
            if (ImGui::Button("Step 1/60")) director_->Seek(director_->GetTime() + 1.0f/60.0f);
            float time_ = director_->GetTime();
            if (ImGui::SliderFloat("Timeline", &time_, 0, end_)) director_->Seek(time_);
            if (ImGui::Button("Recapture")) director_->RequestCapture();
            ImGui::SameLine();
            bool fixed_ = director_->IsShowingFixed();
            if (ImGui::Checkbox("Show fixed 2D", &fixed_)) director_->SetShowFixed(fixed_);
            if (auto* texture_ = director_->GetCapture())
            {
                const float width_ = (std::max)(ImGui::GetContentRegionAvail().x, 100.0f);
                const auto uv_ = director_->GetCaptureUv();
                ImGui::Image((ImTextureID)(intptr_t)texture_, {width_,width_/director_->GetAspect()}, {uv_.x,uv_.y}, {uv_.z,uv_.w});
            }
            if (ImGui::Button("Camera from Editor")) director_->CaptureEditorCamera();
            ImGui::EndDisabled();
            if (ImGui::Button("Save Preset")) director_->Save();
            ImGui::SameLine();
            if (ImGui::Button("Load Preset")) { director_->Stop(); director_->Load(); }
            ImGui::TextWrapped("%s", director_->GetStatus().c_str());
            DrawSettings(director_->GetSettings(), "Preset");
            ImGui::EndDisabled();
        }
    }
    EndEditorWindow();
}
