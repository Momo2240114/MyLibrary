#include "PrecompiledHeader.h"
#include "LoadingPresentation.h"
#include "TransitionSequence.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Debug/TracyProfiler.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <fstream>

void LoadingPresentation::Initialize(bool engine_)
{
    BS_PROFILE_SCOPE("LoadingPresentation::Initialize");
    Finalize();
    engine = engine_;
    elapsedTime = displayProgress = actualProgress = hintTime = 0;
    completeTime = -1;
    hintIndex = -1;
    settings = nlohmann::json::object();
    std::ifstream file_(engine ? "Data/UI/BigShineLoading.json" : "Data/UI/AnathemaLoading.json");
    if (file_)
    {
        auto document_ = nlohmann::json::parse(file_, nullptr, false);
        if (document_.is_object())
        {
            settings = std::move(document_);
        }
    }
    if (settings.contains("Hints") && settings["Hints"].is_array())
    {
        for (const auto& hint_ : settings["Hints"])
        {
            if (hint_.is_string() && !hint_.get<std::string>().empty())
            {
                hints.push_back(hint_.get<std::string>());
            }
        }
    }
    SelectHint();
    const auto path_ = settings.value("LogoPath", std::string());
    // ロゴ欠損時はブランド名で代替し、ロードScene自体の進行を妨げない。
    if (!path_.empty() && std::filesystem::is_regular_file(path_))
    {
        D3D11_TEXTURE2D_DESC texture_{};
        const auto widePath_ = std::filesystem::path(path_).wstring();
        if (SUCCEEDED(CPUResourceLoader::LoadTextureFromFile(GraphicsDeviceManager::Instance().GetDevice(), widePath_.c_str(), logo.GetAddressOf(), &texture_)) && texture_.Height)
        {
            logoAspect = static_cast<float>(texture_.Width) / texture_.Height;
        }
    }
}

void LoadingPresentation::Finalize()
{
    BS_PROFILE_SCOPE("LoadingPresentation::Finalize");
    logo.Reset();
    hints.clear();
}

void LoadingPresentation::SelectHint()
{
    if (hints.empty())
    {
        return;
    }
    const int previous_ = hintIndex;
    randomState = randomState * 1664525u + 1013904223u;
    if (hints.size() == 1)
    {
        hintIndex = 0;
    }
    else
    {
        hintIndex = static_cast<int>(randomState % (previous_ < 0 ? hints.size() : hints.size() - 1));
        if (hintIndex >= previous_ && previous_ >= 0)
        {
            ++hintIndex;
        }
    }
    hintTime = 0;
}

void LoadingPresentation::Update(float deltaTime_, float progress_, bool completed_)
{
    BS_PROFILE_SCOPE("LoadingPresentation::Update");
    const float step_ = std::isfinite(deltaTime_) ? (std::max)(0.0f, deltaTime_) : 0;
    elapsedTime += step_;
    hintTime += step_;
    actualProgress = std::isfinite(progress_) ? std::clamp(progress_, 0.0f, 1.0f) : actualProgress;
    displayProgress = (std::min)(actualProgress, displayProgress + (actualProgress - displayProgress) * (1.0f - std::exp(-8.0f * step_)));
    if (completed_)
    {
        displayProgress = 1;
        if (completeTime < 0)
        {
            completeTime = 0;
        }
        else
        {
            completeTime += step_;
        }
    }
    else
    {
        completeTime = -1;
    }
    if (hintTime >= (std::max)(2.0f, settings.value("HintDuration", 7.0f)))
    {
        SelectHint();
    }
}

void LoadingPresentation::Draw(bool failed_, bool standalone_)
{
    BS_PROFILE_SCOPE("LoadingPresentation::Draw");
    const auto viewport_ = ImGui::GetMainViewport();
    if (!viewport_)
    {
        return;
    }
    const auto draw_ = ImGui::GetBackgroundDrawList(viewport_);
    const auto origin_ = viewport_->Pos;
    const float width_ = viewport_->Size.x, height_ = viewport_->Size.y;
    const float scale_ = (std::min)(width_ / 1280.0f, height_ / 720.0f);
    const float intro_ = std::clamp(TransitionSequence::Evaluate(elapsedTime / 0.8f, TransitionEasing::EASE_OUT_CUBIC), 0.0f, 1.0f);
    const auto accent_ = settings.value("AccentColor", std::vector<float>{0.65f,0.08f,0.1f});
    const auto edge_ = settings.value("LightColor", std::vector<float>{0.72f,1.0f,0.85f});
    const auto tint_ = [&](float alpha_, bool light_ = false)
    {
        const auto& source_ = light_ ? edge_ : accent_;
        return ImGui::ColorConvertFloat4ToU32(ImVec4(source_.size() >= 3 ? source_[0] : 0.7f,
            source_.size() >= 3 ? source_[1] : 0.8f, source_.size() >= 3 ? source_[2] : 1.0f, std::clamp(alpha_ * intro_, 0.0f, 1.0f)));
    };
    const auto position_ = [&](float x_, float y_)
    {
        return ImVec2(origin_.x + width_ * x_, origin_.y + height_ * y_);
    };
    const auto text_ = [&](const std::string& value_, float x_, float y_, float size_, ImU32 color_, float wrap_ = 0.0f)
    {
        draw_->AddText(ImGui::GetFont(), size_ * scale_, position_(x_,y_), color_, value_.c_str(), nullptr, wrap_ * width_);
    };
    const float motion_ = settings.value("BackgroundSpeed", 0.06f);
    const float intensity_ = std::clamp(settings.value("ParticleIntensity", 0.7f), 0.0f, 2.0f);
    if (standalone_)
    {
        draw_->AddRectFilled(position_(0,0), position_(1,1), IM_COL32(4,5,9,255));
    }
    if (engine)
    {
        // 背景と装飾の時間差で、起動時の情報を段階的に見せる。
        draw_->AddRectFilled(position_(0,0), position_(1,1), IM_COL32(5,10,19,255));
        for (int index_ = 0; index_ < 13; ++index_)
        {
            const float x_ = std::fmod(index_ / 12.0f + elapsedTime * motion_ * 0.05f, 1.1f);
            draw_->AddLine(position_(x_,0), position_(x_ - 0.18f,1), tint_(0.045f), 1);
        }
        const float reveal_ = std::clamp(TransitionSequence::Evaluate((elapsedTime - 0.15f) / 0.65f, TransitionEasing::EASE_OUT_EXPO), 0.0f, 1.0f);
        draw_->AddLine(position_(0.5f - 0.22f * reveal_,0.58f), position_(0.5f + 0.22f * reveal_,0.58f), tint_(0.5f), scale_);
    }
    else
    {
        // 半透明の霧を画面端と足元へ限定し、既存モデルの輪郭を保つ。
        draw_->AddRectFilledMultiColor(position_(0,0.60f), position_(1,1), IM_COL32(0,0,0,0), IM_COL32(0,0,0,0), IM_COL32(0,0,0,220), IM_COL32(0,0,0,220));
        for (int layer_ = 0; layer_ < 3; ++layer_)
        {
            for (int index_ = 0; index_ < 18; ++index_)
            {
                const float seed_ = static_cast<float>(index_);
                const float x_ = std::fmod(seed_ * 0.061f + elapsedTime * motion_ * (0.2f + layer_ * 0.12f), 1.1f) - 0.05f;
                const float y_ = 0.76f + std::sin(seed_ * 1.7f + elapsedTime * 0.25f + layer_) * (0.012f + layer_ * 0.013f);
                const float radius_ = (32.0f + layer_ * 22.0f) * scale_;
                draw_->AddCircleFilled(position_(x_, y_), radius_, IM_COL32(3,1,4,9), 20);
            }
        }
        // 接地面の低い反射と、足元から浮く少数の粒子を同じDrawListへまとめる。
        draw_->AddRectFilledMultiColor(position_(0.05f,0.77f), position_(0.36f,0.80f), tint_(0.0f), tint_(0.0f), tint_(0.06f), tint_(0.04f));
        for (int index_ = 0; index_ < 18; ++index_)
        {
            const float phase_ = std::fmod(elapsedTime * (0.12f + index_ * 0.004f) + index_ * 0.137f, 1.0f);
            const float x_ = 0.12f + index_ * 0.009f + phase_ * 0.018f;
            draw_->AddCircleFilled(position_(x_,0.77f - phase_ * 0.13f), (index_ % 3 == 0 ? 1.4f : 0.8f) * scale_, tint_(std::sin(phase_ * 3.141593f) * 0.35f * intensity_, index_ % 5 == 0), 6);
        }
        // 奥行きの違う薄い弧をゆっくり流し、シルエットの領域には強い光を置かない。
        for (int layer_ = 0; layer_ < 3; ++layer_)
        {
            const ImVec2 center_ = position_(0.77f + std::sin(elapsedTime * motion_ + layer_) * 0.015f,0.43f);
            const float radius_ = (145 + layer_ * 70.0f) * scale_;
            draw_->PathArcTo(center_, radius_, 0.25f + layer_, 2.8f + layer_, 40);
            draw_->PathStroke(tint_(0.07f - layer_ * 0.015f), 0, scale_);
        }
    }
    const float centerX_ = settings.value("LogoCenterX", engine ? 0.5f : 0.72f);
    const float top_ = settings.value("LogoTop", engine ? 0.21f : 0.22f) + (1 - intro_) * 0.018f;
    const float logoWidth_ = width_ * settings.value("LogoWidth", engine ? 0.14f : 0.34f);
    if (logo)
    {
        draw_->AddImage((ImTextureID)(intptr_t)logo.Get(), position_(centerX_ - logoWidth_ / width_ * 0.5f,top_),
            ImVec2(origin_.x + width_ * centerX_ + logoWidth_ * 0.5f, origin_.y + height_ * top_ + logoWidth_ / (std::max)(0.01f,logoAspect)), ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,static_cast<int>(255 * intro_)));
    }
    else
    {
        text_(engine ? "BIGSHINE" : "ANATHEMA", centerX_ - 0.10f,top_,42,tint_(1,true));
    }
    if (engine)
    {
        text_("BIGSHINE  /  ENGINE",0.40f,0.50f,23,tint_(0.95f,true));
    }
    const float barX_ = settings.value("ProgressLeft", engine ? 0.28f : 0.40f);
    const float barY_ = settings.value("ProgressTop", engine ? 0.68f : 0.81f);
    const float barWidth_ = settings.value("ProgressWidth", engine ? 0.44f : 0.49f);
    const float barHeight_ = (engine ? 4.0f : 2.0f) * scale_;
    const ImVec2 start_ = position_(barX_, barY_), end_ = position_(barX_ + barWidth_,barY_);
    const float tip_ = start_.x + (end_.x - start_.x) * displayProgress;
    const float pulse_ = completeTime >= 0 ? std::exp(-completeTime * 7) : 0;
    const float uiAlpha_ = std::clamp((elapsedTime - 0.25f) / 0.45f,0.0f,1.0f);
    draw_->AddRectFilled(ImVec2(start_.x,start_.y - barHeight_), ImVec2(end_.x,start_.y + barHeight_), tint_(0.12f * uiAlpha_));
    draw_->AddRectFilled(start_, ImVec2(tip_,start_.y + barHeight_), tint_((0.8f + pulse_ * 0.2f) * uiAlpha_));
    if (displayProgress > 0)
    {
        for (int ring_ = 3; ring_ >= 1; --ring_)
        {
            draw_->AddCircleFilled(ImVec2(tip_,start_.y + barHeight_ * 0.5f), (3 + ring_ * 3.0f + pulse_ * 6.0f) * scale_, tint_(0.08f * uiAlpha_,true), 16);
        }
        draw_->AddCircleFilled(ImVec2(tip_,start_.y + barHeight_ * 0.5f), 2.2f * scale_, tint_(uiAlpha_,true), 12);
    }
    const std::string operation_ = failed_ ? settings.value("FailureText", std::string("Loading failed")) : completeTime >= 0 ? settings.value("ReadyText", std::string("READY")) : settings.value("LoadingText", std::string("PREPARING"));
    text_(operation_,barX_,barY_ - 0.045f,16,tint_(uiAlpha_,true));
    const std::string percent_ = std::to_string(static_cast<int>(displayProgress * 100)) + "%";
    text_(percent_,barX_ + barWidth_ - 0.035f,barY_ - 0.045f,16,tint_(uiAlpha_,true));
    // 一般的なスピナーではなく、順番に発光する斜線をブランド別の色で描く。
    for (int index_ = 0; index_ < 5; ++index_)
    {
        const float wave_ = 0.2f + 0.8f * (0.5f + 0.5f * std::sin(elapsedTime * 4 - index_ * 0.9f));
        const ImVec2 point_ = position_(barX_ + barWidth_ - 0.055f + index_ * 0.012f,barY_ + 0.05f);
        draw_->AddLine(point_, ImVec2(point_.x + 7 * scale_,point_.y - 13 * scale_), tint_(wave_ * 0.75f,true), 1.5f * scale_);
    }
    if (!engine && settings.value("ShowHints",true) && hintIndex >= 0)
    {
        const float duration_ = (std::max)(2.0f,settings.value("HintDuration",7.0f));
        const float fade_ = (std::max)(0.1f,settings.value("HintFadeDuration",0.6f));
        const float alpha_ = std::clamp((std::min)(hintTime,duration_ - hintTime) / fade_,0.0f,1.0f);
        text_(settings.value("HintLabel",std::string("COMBAT NOTES")),barX_,barY_ + 0.07f,12,tint_(alpha_ * 0.6f,true));
        text_(hints[hintIndex],barX_,barY_ + 0.105f,16,tint_(alpha_ * 0.9f,true),barWidth_);
    }
}
