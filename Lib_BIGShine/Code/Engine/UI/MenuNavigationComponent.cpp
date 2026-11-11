#include "PrecompiledHeader.h"
#include "MenuNavigationComponent.h"
#include "ScreenFadeComponent.h"
#include "Engine/UI/ScreenTransitionManager.h"
#include "MenuPresentation.h"
#include "UIWidgetComponent.h"
#include "UIVisualEffectComponent.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Core/GameRuntimeSettings.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"
#include "Engine/GamePlay/Input/KeyInput.h"
#include "Engine/GamePlay/Input/MouseInput.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>
#include "Engine/GamePlay/Input/KeyConfig.h"

const std::string& MenuNavigationComponent::GetName() const
{
    static const std::string NAME = "MenuNavigationComponent";
    return NAME;
}

void MenuNavigationComponent::Start()
{
    BS_PROFILE_SCOPE("MenuNavigationComponent::Start");
    MenuPresentation::ReloadCurves();
    elapsedTime = repeatTime = 0.0f;
    selectionTime = 0.0f;
    selectedIndex = -1;
    heldDirection = 0;
    confirmHeld = true;
    transitioning = false;
    for (auto& item_ : items) item_.selection = 0.0f;
    UpdateVisuals();
}

bool MenuNavigationComponent::IsSelectable(int index_) const
{
    if (index_ < 0 || index_ >= static_cast<int>(items.size())) return false;
    const auto& item_ = items[index_];
    // Sceneから削除したUIを、入力候補だけが残った状態で選択させない。
    const auto owner_ = gameObject.lock();
    const auto object_ = owner_ && owner_->manager ? owner_->GetManager().Find(item_.labelObject) : nullptr;
    const auto widget_ = object_ ? object_->GetComponent<UIWidgetComponent>() : nullptr;
    const auto sprite_ = object_ ? object_->GetComponent<SpriteRenderer>() : nullptr;
    return item_.visible && item_.enabled && object_ && !object_->IsPendingDestroy() &&
        widget_ && widget_->IsEnabled() && widget_->IsVisible() && sprite_ && sprite_->IsEnabled() &&
        ActionRegistry::Instance().HasAction(item_.action);
}

void MenuNavigationComponent::PlaySound(const std::string& path_) const
{
    std::error_code error_;
    if (!path_.empty() && std::filesystem::is_regular_file(std::filesystem::path(std::u8string(path_.begin(), path_.end())), error_))
        AudioManager::Instance().PlaySEByPath(path_);
}

void MenuNavigationComponent::SelectItem(int index_)
{
    if (!IsSelectable(index_) || selectedIndex == index_) return;
    selectedIndex = index_;
    selectionTime = 0.0f;
    PlaySound(moveSound);
}

void MenuNavigationComponent::MoveSelection(int direction_)
{
    const int count_ = static_cast<int>(items.size());
    for (int step_ = 1; step_ <= count_; ++step_)
    {
        const int index_ = (selectedIndex + direction_ * step_ + count_ * 2) % count_;
        if (IsSelectable(index_)) { SelectItem(index_); return; }
    }
}

void MenuNavigationComponent::ConfirmSelection()
{
    if (transitioning || elapsedTime < inputDelay || !IsSelectable(selectedIndex)) return;
    const auto object_ = GetManager().Find(fadeObject);
    const auto fade_ = object_ ? object_->GetComponent<ScreenFadeComponent>() : nullptr;
    // 設定した暗転Objectが欠けた場合は、無操作状態へ移らず再度選択できるようにする。
    if (!fade_ || !fade_->IsEnabled() || !fade_->BeginFadeOut(items[selectedIndex].action)) return;
    transitioning = true;
    PlaySound(confirmSound);
}

void MenuNavigationComponent::UpdateVisuals()
{
    BS_PROFILE_SCOPE("MenuNavigationComponent::UpdateVisuals");
    const auto object_ = GetManager().Find(fadeObject);
    const auto fade_ = object_ ? object_->GetComponent<ScreenFadeComponent>() : nullptr;
    const float fadeAlpha_ = fade_ && fade_->IsFadingOut() ? 1.0f - std::clamp(fade_->GetFadeOutTime() / 0.2f, 0.0f, 1.0f) : 1.0f;
    const auto apply_ = [&](const std::string& name_, float alpha_, float brightness_ = 1.0f)
    {
        const auto element_ = GetManager().Find(name_);
        const auto sprite_ = element_ ? element_->GetComponent<SpriteRenderer>() : nullptr;
        if (!sprite_) return;
        auto color_ = sprite_->GetConfiguredColor();
        color_.x *= brightness_; color_.y *= brightness_; color_.z *= brightness_;
        color_.w *= std::clamp(alpha_, 0.0f, 1.0f);
        sprite_->SetRuntimeColor(color_);
        sprite_->SetRuntimeVisible(color_.w > 0.0f);
    };
    for (std::size_t index_ = 0; index_ < items.size(); ++index_)
    {
        const auto& item_ = items[index_];
        const float reveal_ = MenuPresentation::Reveal((elapsedTime - 1.0f - static_cast<float>(index_) * 0.08f) / 0.5f);
        const float alpha_ = item_.visible ? reveal_ * fadeAlpha_ : 0.0f;
        const bool selected_ = static_cast<int>(index_) == selectedIndex;
        const float kick_ = selected_ ? MenuPresentation::Focus(selectionTime / 0.24f) - 1.0f : 0.0f;
        const float pulse_ = 0.88f + 0.12f * std::sin(elapsedTime * 3.6f);
        // 文字・枠・光を同時に動かし、設定された位置やサイズは保存時にも維持する。
        const auto transform_ = [&](const std::string& name_, float offset_, float scale_)
        {
            const auto element_ = GetManager().Find(name_);
            if (const auto widget_ = element_ ? element_->GetComponent<UIWidgetComponent>() : nullptr)
                widget_->SetRuntimeVisualTransform({ (1.0f - reveal_) * -38.0f + offset_, 0.0f }, scale_);
        };
        const float shift_ = 12.0f * item_.selection + 5.0f * kick_;
        const float scale_ = 1.0f + 0.045f * item_.selection + 0.035f * kick_;
        transform_(item_.labelObject, shift_, scale_);
        transform_(item_.highlightObject, shift_, scale_);
        transform_(item_.markerObject, shift_ + std::sin(elapsedTime * 4.0f) * 3.0f, scale_);
        transform_(item_.glowObject, shift_, scale_ + 0.018f * pulse_);
        // 非選択の文字は控えめに保ち、選択時だけ共通UIEffectの光沢を強める。
        const auto labelObject_ = GetManager().Find(item_.labelObject);
        if (const auto effect_ = labelObject_ ? labelObject_->GetComponent<UIVisualEffectComponent>() : nullptr)
            effect_->SetRuntimeStrength(0.25f + item_.selection * 0.75f);
        apply_(item_.labelObject, alpha_, IsSelectable(static_cast<int>(index_)) ? 1.0f + item_.selection * 0.16f : 0.48f);
        apply_(item_.highlightObject, alpha_ * item_.selection);
        apply_(item_.markerObject, alpha_ * item_.selection);
        apply_(item_.glowObject, alpha_ * item_.selection * pulse_, transitioning && selected_ ? 2.0f : 1.0f);
        if (transitioning && selected_ && fade_)
        {
            const auto label_ = GetManager().Find(item_.labelObject);
            if (const auto widget_ = label_ ? label_->GetComponent<UIWidgetComponent>() : nullptr)
            {
                const auto& rect_ = widget_->GetScreenRect();
                const auto position_ = ImGui::GetMainViewport()->Pos;
                MenuPresentation::DrawFeedback(ImVec2(rect_.min.x + position_.x, rect_.min.y + position_.y),
                    ImVec2(rect_.max.x + position_.x, rect_.max.y + position_.y), fade_->GetFadeOutTime() / 0.24f, fadeAlpha_, false);
            }
        }
    }
    const float introAlpha_ = std::clamp((elapsedTime - 1.0f) / 0.5f, 0.0f, 1.0f) * fadeAlpha_;
    apply_(panelObject, introAlpha_);
    const bool playStation_ = ControllerInput::Instance().IsPlayStation();
    apply_(keyboardGuideObject, playStation_ ? 0.0f : introAlpha_);
    apply_(playStationGuideObject, playStation_ ? introAlpha_ : 0.0f);
    apply_(developerGuideObject, GameRuntimeSettings::IsGameOnly() ? 0.0f : introAlpha_);
}

void MenuNavigationComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("MenuNavigationComponent::Update");
    if (GetManager().GetSceneRuntimeState() != SCENE_RUNTIME_STATE::RUNNING) return;
    const float step_ = (std::max)(0.0f, deltaTime_);
    elapsedTime += step_;
    selectionTime += step_;
    if (!IsSelectable(selectedIndex))
    {
        selectedIndex = -1;
        for (int index_ = 0; index_ < static_cast<int>(items.size()); ++index_)
            if (IsSelectable(index_)) { selectedIndex = index_; break; }
    }
    for (int index_ = 0; index_ < static_cast<int>(items.size()); ++index_)
        items[index_].selection += ((index_ == selectedIndex ? 1.0f : 0.0f) - items[index_].selection) * (1.0f - std::exp(-12.0f * step_));
    const auto& keys_ = KeyConfig::Instance();
    const bool confirm_ = keys_.ActionPressed(confirmInputAction);
    const auto* viewport_ = ImGui::GetMainViewport();
    const bool focused_ = viewport_ && GetForegroundWindow() == static_cast<HWND>(viewport_->PlatformHandle);
    if (focused_ && !ScreenTransitionManager::Instance().IsInputBlocked() && !inputSuppressed && !transitioning && elapsedTime >= inputDelay)
    {
        const bool up_ = keys_.ActionPressed(previousInputAction);
        const bool down_ = keys_.ActionPressed(nextInputAction);
        const int direction_ = up_ == down_ ? 0 : up_ ? -1 : 1;
        if (!direction_) { heldDirection = 0; repeatTime = 0.0f; }
        else if (direction_ != heldDirection) { MoveSelection(direction_); heldDirection = direction_; repeatTime = repeatDelay; }
        else if ((repeatTime -= step_) <= 0.0f) { MoveSelection(direction_); repeatTime = repeatInterval; }
        const auto& io_ = ImGui::GetIO();
        const auto& mouse_ = MouseInput::Instance();
        if (!io_.WantCaptureMouse && (std::abs(io_.MouseDelta.x) + std::abs(io_.MouseDelta.y) > 0.5f || mouse_.LeftPressed()))
            for (int index_ = 0; index_ < static_cast<int>(items.size()); ++index_)
            {
                const auto object_ = GetManager().Find(items[index_].labelObject);
                const auto widget_ = object_ ? object_->GetComponent<UIWidgetComponent>() : nullptr;
                if (!widget_ || !IsSelectable(index_)) continue;
                const auto& rect_ = widget_->GetScreenRect();
                const ImVec2 min_(rect_.min.x + viewport_->Pos.x, rect_.min.y + viewport_->Pos.y);
                const ImVec2 max_(rect_.max.x + viewport_->Pos.x, rect_.max.y + viewport_->Pos.y);
                if (ImGui::IsMouseHoveringRect(min_, max_, false))
                {
                    SelectItem(index_);
                    if (mouse_.LeftPressed()) ConfirmSelection();
                    break;
                }
            }
        if (!GameRuntimeSettings::IsGameOnly() && !developerAction.empty() && keys_.ActionPressedIn(developerInputAction))
        {
            const auto object_ = GetManager().Find(fadeObject);
            const auto fade_ = object_ ? object_->GetComponent<ScreenFadeComponent>() : nullptr;
            if (fade_ && fade_->IsEnabled()) transitioning = fade_->BeginFadeOut(developerAction);
        }
        else if (confirm_ && !confirmHeld && !KeyInput::KeyPressed(VK_MENU)) ConfirmSelection();
    }
    else { heldDirection = 0; repeatTime = 0.0f; }
    confirmHeld = confirm_;
    UpdateVisuals();
}

void MenuNavigationComponent::Gui()
{
    ImGui::DragFloat("Input Delay", &inputDelay, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat("Repeat Delay", &repeatDelay, 0.01f, 0.01f, 2.0f);
    ImGui::DragFloat("Repeat Interval", &repeatInterval, 0.01f, 0.01f, 2.0f);
    for (std::size_t index_ = 0; index_ < items.size(); ++index_)
    {
        ImGui::PushID(static_cast<int>(index_));
        ImGui::TextUnformatted(items[index_].labelObject.c_str());
        ImGui::Checkbox("Visible", &items[index_].visible);
        ImGui::SameLine();
        ImGui::Checkbox("Enabled", &items[index_].enabled);
        ImGui::PopID();
    }
}

void MenuNavigationComponent::Serialize(nlohmann::json& json_) const
{
    json_ = {{"FadeObject", fadeObject}, {"PanelObject", panelObject}, {"KeyboardGuideObject", keyboardGuideObject},
        {"PlayStationGuideObject", playStationGuideObject}, {"DeveloperGuideObject", developerGuideObject},
        {"DeveloperAction", developerAction}, {"MoveSound", moveSound}, {"ConfirmSound", confirmSound},
        {"InputDelay", inputDelay}, {"RepeatDelay", repeatDelay}, {"RepeatInterval", repeatInterval},
        {"PreviousInputAction", previousInputAction}, {"NextInputAction", nextInputAction},
        {"ConfirmInputAction", confirmInputAction}, {"DeveloperInputAction", developerInputAction}, {"Items", nlohmann::json::array()}};
    for (const auto& item_ : items)
        json_["Items"].push_back({{"Action", item_.action}, {"LabelObject", item_.labelObject},
            {"HighlightObject", item_.highlightObject}, {"MarkerObject", item_.markerObject}, {"GlowObject", item_.glowObject},
            {"Visible", item_.visible}, {"Enabled", item_.enabled}});
}

void MenuNavigationComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("MenuNavigationComponent::Deserialize");
    fadeObject = json_.value("FadeObject", ""); panelObject = json_.value("PanelObject", "");
    keyboardGuideObject = json_.value("KeyboardGuideObject", ""); playStationGuideObject = json_.value("PlayStationGuideObject", "");
    developerGuideObject = json_.value("DeveloperGuideObject", ""); developerAction = json_.value("DeveloperAction", "");
    moveSound = json_.value("MoveSound", ""); confirmSound = json_.value("ConfirmSound", "");
    previousInputAction = json_.value("PreviousInputAction", "UI.MenuPrevious"); nextInputAction = json_.value("NextInputAction", "UI.MenuNext");
    confirmInputAction = json_.value("ConfirmInputAction", "UI.MenuConfirm"); developerInputAction = json_.value("DeveloperInputAction", "UI.OpenEditor");
    const auto duration_ = [&](const char* key_, float fallback_)
    {
        const float value_ = json_.value(key_, fallback_);
        return std::isfinite(value_) ? (std::max)(0.01f, value_) : fallback_;
    };
    inputDelay = duration_("InputDelay", 1.5f); repeatDelay = duration_("RepeatDelay", 0.35f); repeatInterval = duration_("RepeatInterval", 0.14f);
    items.clear();
    if (json_.contains("Items"))
        for (const auto& item_ : json_.at("Items"))
        {
            MenuItem entry_;
            entry_.action = item_.value("Action", ""); entry_.labelObject = item_.value("LabelObject", "");
            entry_.highlightObject = item_.value("HighlightObject", ""); entry_.markerObject = item_.value("MarkerObject", "");
            entry_.glowObject = item_.value("GlowObject", ""); entry_.visible = item_.value("Visible", true); entry_.enabled = item_.value("Enabled", false);
            if (!entry_.labelObject.empty()) items.push_back(std::move(entry_));
        }
}
