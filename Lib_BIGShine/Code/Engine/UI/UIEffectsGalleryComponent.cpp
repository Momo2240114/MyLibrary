#include "PrecompiledHeader.h"
#include "UIEffectsGalleryComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/UI/UIMotionComponent.h"
#include "Engine/UI/UITextComponent.h"
#include "Engine/UI/UIVisualEffectComponent.h"
#include "Engine/UI/UIWidgetComponent.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float FOCUS_DURATION = 0.32f;
}

const std::string& UIEffectsGalleryComponent::GetName() const
{
    static const std::string NAME = "UIEffectsGalleryComponent";
    return NAME;
}

void UIEffectsGalleryComponent::Start()
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::Start");
    ResetFocus();
    currentPage = pages.empty() ? 0 : std::clamp(initialPage, 0, static_cast<int>(pages.size()) - 1);
    displayPending = true;
    replayPending = GetManager().GetEditorPlayMode() == EditorPlayMode::PLAY;
    ApplyPage();
}

void UIEffectsGalleryComponent::SelectPage(int index_)
{
    if (pages.empty()) return;
    ResetFocus();
    const int count_ = static_cast<int>(pages.size());
    currentPage = ((index_ % count_) + count_) % count_;
    displayPending = replayPending = true;
}

void UIEffectsGalleryComponent::ApplyPage()
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::ApplyPage");
    if (pages.empty()) return;
    bool ready_ = true;
    for (std::size_t pageIndex_ = 0; pageIndex_ < pages.size(); ++pageIndex_)
    {
        const bool visible_ = static_cast<int>(pageIndex_) == currentPage;
        for (const auto& name_ : pages[pageIndex_].objects)
        {
            const auto object_ = GetManager().Find(name_);
            if (!object_ || object_->IsPendingDestroy()) continue;
            const auto sprite_ = object_->GetComponent<SpriteRenderer>();
            if (!sprite_ || (sprite_->IsEnabled() && !sprite_->IsStarted())) ready_ = false;
            if (sprite_) sprite_->SetRuntimeVisible(visible_);

            // 非表示ページの残像を破棄し、再表示時に前の演出が混ざるのを防ぐ。
            if (const auto effect_ = object_->GetComponent<UIVisualEffectComponent>())
            {
                if (effect_->IsEnabled() && !effect_->IsStarted()) ready_ = false;
                else if (!visible_) effect_->Stop();
            }
            if (const auto motion_ = object_->GetComponent<UIMotionComponent>())
            {
                if (motion_->IsEnabled() && !motion_->IsStarted()) ready_ = false;
                else if (!visible_) motion_->Stop();
            }
        }
    }
    const auto labelObject_ = GetManager().Find(pageLabelObject);
    if (const auto text_ = labelObject_ ? labelObject_->GetComponent<UITextComponent>() : nullptr)
        text_->SetText(std::to_string(currentPage + 1) + " / " + std::to_string(pages.size()) + "   " + pages[currentPage].title);

    // UITextがStartでRendererを追加するため、全作例の初期化後に一度だけ再生する。
    if (ready_ && replayPending)
    {
        const bool preview_ = GetManager().GetEditorPlayMode() == EditorPlayMode::EDITOR;
        for (const auto& name_ : pages[currentPage].objects)
        {
            const auto object_ = GetManager().Find(name_);
            if (!object_ || object_->IsPendingDestroy()) continue;
            if (const auto effect_ = object_->GetComponent<UIVisualEffectComponent>(); effect_ && effect_->IsEnabled()) effect_->Play(preview_);
            if (const auto motion_ = object_->GetComponent<UIMotionComponent>(); motion_ && motion_->IsEnabled()) motion_->Play(preview_);
        }
        replayPending = false;
    }
    displayPending = !ready_;
}

bool UIEffectsGalleryComponent::IsControlClicked(const std::string& name_)
{
    if (ImGui::GetIO().WantCaptureMouse || !MouseInput::Instance().LeftPressed()) return false;
    const auto object_ = GetManager().Find(name_);
    const auto widget_ = object_ ? object_->GetComponent<UIWidgetComponent>() : nullptr;
    if (!widget_ || !widget_->IsEnabled() || !widget_->IsVisible()) return false;
    const auto& rect_ = widget_->GetScreenRect();
    const auto origin_ = ImGui::GetMainViewport()->Pos;
    return ImGui::IsMouseHoveringRect(
        ImVec2(rect_.min.x + origin_.x, rect_.min.y + origin_.y),
        ImVec2(rect_.max.x + origin_.x, rect_.max.y + origin_.y), false);
}

void UIEffectsGalleryComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::Update");
    (void)deltaTime_;
    // EditorではInspectorから操作し、シーン編集用のキー入力とは競合させない。
    const auto* viewport_ = ImGui::GetMainViewport();
    if (GetManager().GetEditorPlayMode() == EditorPlayMode::PLAY && viewport_ &&
        GetForegroundWindow() == static_cast<HWND>(viewport_->PlatformHandle))
    {
        const bool keyboard_ = !ImGui::GetIO().WantCaptureKeyboard;
        if (focusedWindow >= 0)
        {
            if ((keyboard_ && KeyInput::KeyPressedIn(KeyInput::K_Escape)) ||
                (!ImGui::GetIO().WantCaptureMouse && MouseInput::Instance().LeftPressed())) focusClosing = true;
        }
        else if ((keyboard_ && (KeyInput::KeyPressedIn(KeyInput::K_Left) || KeyInput::KeyPressedIn(KeyInput::K_PageUp))) || IsControlClicked(previousObject))
            SelectPage(currentPage - 1);
        else if ((keyboard_ && (KeyInput::KeyPressedIn(KeyInput::K_Right) || KeyInput::KeyPressedIn(KeyInput::K_PageDown))) || IsControlClicked(nextObject))
            SelectPage(currentPage + 1);
        else if ((keyboard_ && KeyInput::KeyPressedIn(KeyInput::K_R)) || IsControlClicked(replayObject))
            SelectPage(currentPage);
        else if (!pages.empty())
            for (int index_ = 0; index_ < static_cast<int>(pages[currentPage].windows.size()); ++index_)
                if (IsControlClicked(pages[currentPage].windows[index_].hitObject)) { OpenWindow(index_); break; }
    }
    if (displayPending || replayPending) ApplyPage();
}

void UIEffectsGalleryComponent::OpenWindow(int index_)
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::OpenWindow");
    if (pages.empty() || index_ < 0 || index_ >= static_cast<int>(pages[currentPage].windows.size())) return;
    ResetFocus();
    focusedWindow = index_;
    UpdateFocus(0.0f);
}

void UIEffectsGalleryComponent::UpdateUnscaled(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::UpdateUnscaled");
    if (focusedWindow >= 0) UpdateFocus(deltaTime_);
}

void UIEffectsGalleryComponent::UpdateFocus(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::UpdateFocus");
    if (pages.empty() || focusedWindow < 0 || focusedWindow >= static_cast<int>(pages[currentPage].windows.size())) return;
    const auto& window_ = pages[currentPage].windows[focusedWindow];
    const auto frameObject_ = GetManager().Find(window_.hitObject);
    const auto frame_ = frameObject_ ? frameObject_->GetComponent<UIWidgetComponent>() : nullptr;
    if (!frame_) { ResetFocus(); return; }
    const float step_ = std::isfinite(deltaTime_) ? (std::max)(0.0f, deltaTime_) / FOCUS_DURATION : 0.0f;
    focusProgress = std::clamp(focusProgress + (focusClosing ? -step_ : step_), 0.0f, 1.0f);
    const float blend_ = focusProgress * focusProgress * (3.0f - 2.0f * focusProgress);
    const auto center_ = frame_->GetRectTransform().anchoredPosition;
    const auto size_ = frame_->GetRectTransform().sizeDelta;
    const float targetScale_ = std::clamp((std::min)(880.0f / (std::max)(1.0f, size_.x), 440.0f / (std::max)(1.0f, size_.y)), 1.0f, 3.0f);
    const float scale_ = 1.0f + (targetScale_ - 1.0f) * blend_;
    for (const auto& name_ : window_.objects)
    {
        const auto object_ = GetManager().Find(name_);
        if (const auto widget_ = object_ ? object_->GetComponent<UIWidgetComponent>() : nullptr)
        {
            // 中央Anchorを基準に、部品間の相対位置も同じ倍率で拡大する。
            const auto position_ = widget_->GetRectTransform().anchoredPosition;
            widget_->SetRuntimeVisualTransform({ -center_.x * blend_ + (position_.x - center_.x) * (scale_ - 1.0f),
                -center_.y * blend_ + (position_.y - center_.y) * (scale_ - 1.0f) }, scale_);
            widget_->SetRuntimeZOrderOffset(100);
        }
    }
    const auto backdropObject_ = GetManager().Find(focusBackdropObject);
    if (const auto sprite_ = backdropObject_ ? backdropObject_->GetComponent<SpriteRenderer>() : nullptr)
    {
        auto color_ = sprite_->GetConfiguredColor();
        color_.w *= blend_;
        sprite_->SetRuntimeColor(color_);
        sprite_->SetRuntimeVisible(true);
    }
    const auto hintObject_ = GetManager().Find(focusHintObject);
    if (const auto sprite_ = hintObject_ ? hintObject_->GetComponent<SpriteRenderer>() : nullptr)
    {
        auto color_ = sprite_->GetConfiguredColor();
        color_.w *= blend_;
        sprite_->SetRuntimeColor(color_);
        sprite_->SetRuntimeVisible(true);
    }
    if (focusClosing && focusProgress <= 0.0f) ResetFocus();
}

void UIEffectsGalleryComponent::ResetFocus()
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::ResetFocus");
    if (!pages.empty() && focusedWindow >= 0 && focusedWindow < static_cast<int>(pages[currentPage].windows.size()))
        for (const auto& name_ : pages[currentPage].windows[focusedWindow].objects)
        {
            const auto object_ = GetManager().Find(name_);
            if (const auto widget_ = object_ ? object_->GetComponent<UIWidgetComponent>() : nullptr)
            {
                widget_->SetRuntimeVisualTransform({ 0.0f, 0.0f }, 1.0f);
                widget_->SetRuntimeZOrderOffset(0);
            }
        }
    for (const auto& name_ : { focusBackdropObject, focusHintObject })
    {
        const auto object_ = GetManager().Find(name_);
        if (const auto sprite_ = object_ ? object_->GetComponent<SpriteRenderer>() : nullptr) sprite_->SetRuntimeVisible(false);
    }
    focusedWindow = -1;
    focusProgress = 0.0f;
    focusClosing = false;
}

void UIEffectsGalleryComponent::OnDestroy()
{
    ResetFocus();
}

void UIEffectsGalleryComponent::Gui()
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::Gui");
    if (pages.empty()) { ImGui::TextUnformatted("No gallery pages."); return; }
    if (ImGui::Button("Previous Page")) SelectPage(currentPage - 1);
    ImGui::SameLine();
    if (ImGui::Button("Next Page")) SelectPage(currentPage + 1);
    ImGui::SameLine();
    if (ImGui::Button("Replay Page")) SelectPage(currentPage);
    for (int index_ = 0; index_ < static_cast<int>(pages.size()); ++index_)
        if (ImGui::Selectable(pages[index_].title.c_str(), currentPage == index_)) SelectPage(index_);
    if (ImGui::Button("Use Current Page On Start")) initialPage = currentPage;
    if (ImGui::Button("Return To Gallery")) focusClosing = true;
    for (int index_ = 0; index_ < static_cast<int>(pages[currentPage].windows.size()); ++index_)
        if (ImGui::Button(pages[currentPage].windows[index_].hitObject.c_str())) OpenWindow(index_);
}

void UIEffectsGalleryComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::Serialize");
    json_["InitialPage"] = initialPage;
    json_["PageLabelObject"] = pageLabelObject;
    json_["PreviousObject"] = previousObject;
    json_["NextObject"] = nextObject;
    json_["ReplayObject"] = replayObject;
    json_["FocusBackdropObject"] = focusBackdropObject;
    json_["FocusHintObject"] = focusHintObject;
    json_["Pages"] = nlohmann::json::array();
    for (const auto& page_ : pages)
    {
        nlohmann::json windows_ = nlohmann::json::array();
        for (const auto& window_ : page_.windows)
            windows_.push_back({ { "HitObject", window_.hitObject }, { "Objects", window_.objects } });
        json_["Pages"].push_back({ { "Title", page_.title }, { "Objects", page_.objects }, { "Windows", windows_ } });
    }
}

void UIEffectsGalleryComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UIEffectsGalleryComponent::Deserialize");
    initialPage = json_.value("InitialPage", 0);
    pageLabelObject = json_.value("PageLabelObject", std::string());
    previousObject = json_.value("PreviousObject", std::string());
    nextObject = json_.value("NextObject", std::string());
    replayObject = json_.value("ReplayObject", std::string());
    focusBackdropObject = json_.value("FocusBackdropObject", std::string());
    focusHintObject = json_.value("FocusHintObject", std::string());
    pages.clear();
    if (json_.contains("Pages") && json_["Pages"].is_array())
        for (const auto& page_ : json_["Pages"])
            if (page_.is_object())
            {
                GalleryPage restoredPage_;
                restoredPage_.title = page_.value("Title", std::string());
                restoredPage_.objects = page_.value("Objects", std::vector<std::string>());
                if (page_.contains("Windows") && page_["Windows"].is_array())
                    for (const auto& window_ : page_["Windows"])
                        if (window_.is_object()) restoredPage_.windows.push_back({ window_.value("HitObject", std::string()), window_.value("Objects", std::vector<std::string>()) });
                pages.push_back(restoredPage_);
            }
    currentPage = pages.empty() ? 0 : std::clamp(initialPage, 0, static_cast<int>(pages.size()) - 1);
    displayPending = true;
    replayPending = false;
    focusedWindow = -1;
    focusProgress = 0.0f;
    focusClosing = false;
}
