#include "PrecompiledHeader.h"

#include "UIProgressBarComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include "Engine/UI/ImGuiColorEditor.h"
#include "Engine/UI/UIWidgetComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "imgui.h"

#include <algorithm>
#include <cstring>
#include <cmath>

namespace
{
    const char* UI_PROGRESS_SOURCE_NAMES[] =
    {
        "MANUAL",
        "PLAYER_HP_RATE",
        "BOSS_HP_RATE"
    };

    constexpr int UI_PROGRESS_SOURCE_COUNT =
        static_cast<int>(sizeof(UI_PROGRESS_SOURCE_NAMES) / sizeof(UI_PROGRESS_SOURCE_NAMES[0]));

    float ClampRate(float rate_)
    {
        return std::clamp(rate_, 0.0f, 1.0f);
    }

    const char* ToString(UI_PROGRESS_SOURCE source_)
    {
        const int index_ = static_cast<int>(source_);
        if (index_ < 0 || index_ >= UI_PROGRESS_SOURCE_COUNT)
        {
            return UI_PROGRESS_SOURCE_NAMES[0];
        }
        return UI_PROGRESS_SOURCE_NAMES[index_];
    }

    UI_PROGRESS_SOURCE ProgressSourceFromString(const std::string& name_)
    {
        for (int index_ = 0; index_ < UI_PROGRESS_SOURCE_COUNT; ++index_)
        {
            if (name_ == UI_PROGRESS_SOURCE_NAMES[index_])
            {
                return static_cast<UI_PROGRESS_SOURCE>(index_);
            }
        }
        return UI_PROGRESS_SOURCE::MANUAL;
    }

    void DrawWidgetIdInput(const char* label_, std::string& value_)
    {
        char buffer_[128]{};
        strncpy_s(buffer_, sizeof(buffer_), value_.c_str(), _TRUNCATE);
        if (ImGui::InputText(label_, buffer_, sizeof(buffer_)))
        {
            value_ = buffer_;
        }
    }

    void SerializeFloat4(
        nlohmann::json& json_,
        const char* key_,
        const DirectX::XMFLOAT4& value_)
    {
        json_[key_] = { value_.x, value_.y, value_.z, value_.w };
    }

    void DeserializeFloat4(
        const nlohmann::json& json_,
        const char* key_,
        DirectX::XMFLOAT4& value_)
    {
        if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 4) return;

        value_.x = json_[key_][0].get<float>();
        value_.y = json_[key_][1].get<float>();
        value_.z = json_[key_][2].get<float>();
        value_.w = json_[key_][3].get<float>();
    }

    DirectX::XMFLOAT4 WithAlpha(const DirectX::XMFLOAT4& color_, float alpha_)
    {
        return
        {
            color_.x,
            color_.y,
            color_.z,
            std::clamp(alpha_, 0.0f, 1.0f)
        };
    }
}

void UIProgressBarComponent::Start()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::Start");
    targetRate = ClampRate(targetRate);
    currentRate = ClampRate(currentRate);
    damageDelayRate = ClampRate(damageDelayRate);
    healDelayRate = ClampRate(healDelayRate);
    ResolveWidgets();
    ApplyLayoutRates();
    ApplyEffectVisuals();
}

void UIProgressBarComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::Update");

    if ((!backgroundWidgetId.empty() && backgroundWidget.expired()) ||
        (!flashWidgetId.empty() && (flashWidget.expired() || flashSprite.expired())) ||
        (!edgeHighlightWidgetId.empty() && (edgeHighlightWidget.expired() || edgeHighlightSprite.expired())) ||
        currentWidget.expired() ||
        damageDelayWidget.expired() || healDelayWidget.expired())
    {
        ResolveWidgets();
    }

    const float previousTargetRate_ = targetRate;
    targetRate = ResolveTargetRate();

    const float targetDelta_ = targetRate - previousTargetRate_;
    if (std::fabs(targetDelta_) > 0.0001f)
    {
        flashTimer = (std::max)(0.0f, flashDuration);
        activeFlashColor = targetDelta_ < 0.0f ? damageFlashColor : healFlashColor;
    }

    flashTimer = (std::max)(0.0f, flashTimer - deltaTime_);

    if (targetRate < currentRate)
    {
        currentRate = targetRate;
        healDelayRate = targetRate;
        damageDelayRate = MoveRate(damageDelayRate, targetRate, decreaseDelaySpeed, deltaTime_);
    }
    else if (targetRate > currentRate)
    {
        healDelayRate = targetRate;
        currentRate = MoveRate(currentRate, targetRate, increaseDelaySpeed, deltaTime_);
        damageDelayRate = currentRate;
    }
    else
    {
        damageDelayRate = MoveRate(damageDelayRate, targetRate, decreaseDelaySpeed, deltaTime_);
        healDelayRate = targetRate;
    }

    ApplyLayoutRates();
    ApplyEffectVisuals();
}

void UIProgressBarComponent::Gui()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::Gui");

    DrawWidgetIdInput("Background Widget Id", backgroundWidgetId);
    DrawWidgetIdInput("Current Widget Id", currentWidgetId);
    DrawWidgetIdInput("Damage Delay Widget Id", damageDelayWidgetId);
    DrawWidgetIdInput("Heal Delay Widget Id", healDelayWidgetId);
    DrawWidgetIdInput("Flash Widget Id", flashWidgetId);
    DrawWidgetIdInput("Edge Highlight Widget Id", edgeHighlightWidgetId);

    int sourceIndex_ = static_cast<int>(source);
    if (ImGui::Combo("Source", &sourceIndex_, UI_PROGRESS_SOURCE_NAMES, UI_PROGRESS_SOURCE_COUNT))
    {
        source = static_cast<UI_PROGRESS_SOURCE>(sourceIndex_);
    }

    if (source == UI_PROGRESS_SOURCE::MANUAL)
    {
        ImGui::DragFloat("Target Rate", &targetRate, 0.01f, 0.0f, 1.0f);
    }
    ImGui::DragFloat("Decrease Delay Speed", &decreaseDelaySpeed, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Increase Delay Speed", &increaseDelaySpeed, 0.01f, 0.01f, 10.0f);
    ImGui::DragFloat("Flash Duration", &flashDuration, 0.01f, 0.0f, 3.0f);
    ImGui::DragFloat("Edge Highlight Width", &edgeHighlightWidth, 0.5f, 1.0f, 64.0f);
    ImGui::DragFloat("Edge Highlight Base Alpha", &edgeHighlightBaseAlpha, 0.01f, 0.0f, 1.0f);
    ImGuiColorEditor::DrawColorEdit4("Damage Flash Color", &damageFlashColor.x);
    ImGuiColorEditor::DrawColorEdit4("Heal Flash Color", &healFlashColor.x);
    ImGuiColorEditor::DrawColorEdit4("Edge Highlight Color", &edgeHighlightColor.x);

    if (ImGui::Button("Resolve Widgets"))
    {
        ResolveWidgets();
    }
}

void UIProgressBarComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::Serialize");

    json_["Version"] = 1;
    json_["BackgroundWidgetId"] = backgroundWidgetId;
    json_["CurrentWidgetId"] = currentWidgetId;
    json_["DamageDelayWidgetId"] = damageDelayWidgetId;
    json_["HealDelayWidgetId"] = healDelayWidgetId;
    json_["FlashWidgetId"] = flashWidgetId;
    json_["EdgeHighlightWidgetId"] = edgeHighlightWidgetId;
    json_["Source"] = ToString(source);
    json_["TargetRate"] = targetRate;
    json_["CurrentRate"] = currentRate;
    json_["DamageDelayRate"] = damageDelayRate;
    json_["HealDelayRate"] = healDelayRate;
    json_["DecreaseDelaySpeed"] = decreaseDelaySpeed;
    json_["IncreaseDelaySpeed"] = increaseDelaySpeed;
    json_["FlashDuration"] = flashDuration;
    json_["FlashTimer"] = flashTimer;
    json_["EdgeHighlightWidth"] = edgeHighlightWidth;
    json_["EdgeHighlightBaseAlpha"] = edgeHighlightBaseAlpha;
    SerializeFloat4(json_, "DamageFlashColor", damageFlashColor);
    SerializeFloat4(json_, "HealFlashColor", healFlashColor);
    SerializeFloat4(json_, "EdgeHighlightColor", edgeHighlightColor);
}

void UIProgressBarComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::Deserialize");

    backgroundWidgetId = json_.value("BackgroundWidgetId", backgroundWidgetId);
    currentWidgetId = json_.value("CurrentWidgetId", currentWidgetId);
    damageDelayWidgetId = json_.value("DamageDelayWidgetId", damageDelayWidgetId);
    healDelayWidgetId = json_.value("HealDelayWidgetId", healDelayWidgetId);
    flashWidgetId = json_.value("FlashWidgetId", flashWidgetId);
    edgeHighlightWidgetId = json_.value("EdgeHighlightWidgetId", edgeHighlightWidgetId);
    source = ProgressSourceFromString(json_.value("Source", std::string(ToString(source))));
    targetRate = ClampRate(json_.value("TargetRate", targetRate));
    currentRate = ClampRate(json_.value("CurrentRate", targetRate));
    damageDelayRate = ClampRate(json_.value("DamageDelayRate", currentRate));
    healDelayRate = ClampRate(json_.value("HealDelayRate", currentRate));
    decreaseDelaySpeed = (std::max)(0.01f, json_.value("DecreaseDelaySpeed", decreaseDelaySpeed));
    increaseDelaySpeed = (std::max)(0.01f, json_.value("IncreaseDelaySpeed", increaseDelaySpeed));
    flashDuration = (std::max)(0.0f, json_.value("FlashDuration", flashDuration));
    flashTimer = (std::max)(0.0f, json_.value("FlashTimer", flashTimer));
    edgeHighlightWidth = (std::max)(1.0f, json_.value("EdgeHighlightWidth", edgeHighlightWidth));
    edgeHighlightBaseAlpha = std::clamp(json_.value("EdgeHighlightBaseAlpha", edgeHighlightBaseAlpha), 0.0f, 1.0f);
    DeserializeFloat4(json_, "DamageFlashColor", damageFlashColor);
    DeserializeFloat4(json_, "HealFlashColor", healFlashColor);
    DeserializeFloat4(json_, "EdgeHighlightColor", edgeHighlightColor);
    activeFlashColor = damageFlashColor;
}

void UIProgressBarComponent::SetTargetRate(float targetRate_)
{
    targetRate = ClampRate(targetRate_);
}

void UIProgressBarComponent::ResolveWidgets()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::ResolveWidgets");

    currentWidget.reset();
    backgroundWidget.reset();
    damageDelayWidget.reset();
    healDelayWidget.reset();
    flashWidget.reset();
    edgeHighlightWidget.reset();
    flashSprite.reset();
    edgeHighlightSprite.reset();

    for (const std::shared_ptr<GameObject>& object_ : GetManager().GetObjects())
    {
        if (!object_) continue;

        const std::shared_ptr<UIWidgetComponent> widget_ = object_->GetComponent<UIWidgetComponent>();
        if (!widget_) continue;

        const std::string& widgetId_ = widget_->GetWidgetId();
        if (!backgroundWidgetId.empty() && widgetId_ == backgroundWidgetId)
        {
            backgroundWidget = widget_;
        }
        else if (widgetId_ == currentWidgetId)
        {
            currentWidget = widget_;
        }
        else if (widgetId_ == damageDelayWidgetId)
        {
            damageDelayWidget = widget_;
        }
        else if (widgetId_ == healDelayWidgetId)
        {
            healDelayWidget = widget_;
        }
        else if (!flashWidgetId.empty() && widgetId_ == flashWidgetId)
        {
            flashWidget = widget_;
            flashSprite = object_->GetComponent<SpriteRenderer>();
        }
        else if (!edgeHighlightWidgetId.empty() && widgetId_ == edgeHighlightWidgetId)
        {
            edgeHighlightWidget = widget_;
            edgeHighlightSprite = object_->GetComponent<SpriteRenderer>();
        }
    }
}

void UIProgressBarComponent::ApplyLayoutRates()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::ApplyLayoutRates");

    ApplyBackgroundLayout();
    ApplyFlashLayout();
    ApplyRateToWidget(damageDelayWidget.lock().get(), damageDelayRate);
    ApplyRateToWidget(healDelayWidget.lock().get(), healDelayRate);
    ApplyRateToWidget(currentWidget.lock().get(), currentRate);
    ApplyEdgeHighlightLayout();
}

void UIProgressBarComponent::ApplyRateToWidget(UIWidgetComponent* widget_, float rate_)
{
    if (!widget_) return;

    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    rectTransform_.anchorMin.x = 0.0f;
    rectTransform_.anchorMax.x = ClampRate(rate_);
    rectTransform_.pivot.x = 0.0f;
    rectTransform_.anchoredPosition.x = 0.0f;
    widget_->MarkLayoutDirty();
}

void UIProgressBarComponent::ApplyFlashLayout()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::ApplyFlashLayout");

    UIWidgetComponent* widget_ = flashWidget.lock().get();
    if (!widget_) return;

    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    rectTransform_.anchorMin.x = 0.0f;
    rectTransform_.anchorMin.y = 0.0f;
    rectTransform_.anchorMax.x = 1.0f;
    rectTransform_.anchorMax.y = 1.0f;
    rectTransform_.pivot.x = 0.5f;
    rectTransform_.pivot.y = 0.5f;
    rectTransform_.anchoredPosition.x = 0.0f;
    rectTransform_.anchoredPosition.y = 0.0f;
    widget_->MarkLayoutDirty();
}

void UIProgressBarComponent::ApplyEdgeHighlightLayout()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::ApplyEdgeHighlightLayout");

    UIWidgetComponent* widget_ = edgeHighlightWidget.lock().get();
    if (!widget_) return;

    const float rate_ = ClampRate(currentRate);
    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    rectTransform_.anchorMin.x = rate_;
    rectTransform_.anchorMin.y = 0.0f;
    rectTransform_.anchorMax.x = rate_;
    rectTransform_.anchorMax.y = 1.0f;
    rectTransform_.pivot.x = 0.5f;
    rectTransform_.pivot.y = 0.5f;
    rectTransform_.anchoredPosition.x = 0.0f;
    rectTransform_.anchoredPosition.y = 0.0f;
    rectTransform_.sizeDelta.x = edgeHighlightWidth;
    widget_->MarkLayoutDirty();
}

void UIProgressBarComponent::ApplyBackgroundLayout()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::ApplyBackgroundLayout");

    UIWidgetComponent* widget_ = backgroundWidget.lock().get();
    if (!widget_) return;

    UIRectTransform& rectTransform_ = widget_->GetRectTransform();
    rectTransform_.anchorMin.x = 0.0f;
    rectTransform_.anchorMax.x = 1.0f;
    rectTransform_.pivot.x = 0.0f;
    rectTransform_.anchoredPosition.x = 0.0f;
    widget_->MarkLayoutDirty();
}

void UIProgressBarComponent::ApplyEffectVisuals()
{
    BS_PROFILE_SCOPE("UIProgressBarComponent::ApplyEffectVisuals");

    const float flashRate_ = flashDuration > 0.0f
        ? std::clamp(flashTimer / flashDuration, 0.0f, 1.0f)
        : 0.0f;
    const float easedFlashRate_ = flashRate_ * flashRate_;

    if (const std::shared_ptr<SpriteRenderer> sprite_ = flashSprite.lock())
    {
        sprite_->SetColor(WithAlpha(activeFlashColor, activeFlashColor.w * easedFlashRate_));
    }

    if (const std::shared_ptr<SpriteRenderer> sprite_ = edgeHighlightSprite.lock())
    {
        const float alpha_ = std::clamp(edgeHighlightBaseAlpha + easedFlashRate_ * 0.35f, 0.0f, 1.0f);
        sprite_->SetColor(WithAlpha(edgeHighlightColor, edgeHighlightColor.w * alpha_));
    }
}

float UIProgressBarComponent::ResolveTargetRate()
{
    const GameModeManager* gameModeManager_ = GetManager().GetGameModeManager();
    if (!gameModeManager_)
    {
        return ClampRate(targetRate);
    }

    switch (source)
    {
    case UI_PROGRESS_SOURCE::PLAYER_HP_RATE:
        return ClampRate(gameModeManager_->GetPlayerHpRate());
    case UI_PROGRESS_SOURCE::BOSS_HP_RATE:
        return ClampRate(gameModeManager_->GetBossHpRate());
    case UI_PROGRESS_SOURCE::MANUAL:
    default:
        return ClampRate(targetRate);
    }
}

float UIProgressBarComponent::MoveRate(
    float currentRate_,
    float targetRate_,
    float speed_,
    float deltaTime_) const
{
    const float clampedCurrentRate_ = ClampRate(currentRate_);
    const float clampedTargetRate_ = ClampRate(targetRate_);
    const float step_ = (std::max)(0.01f, speed_) * (std::max)(0.0f, deltaTime_);

    if (clampedCurrentRate_ < clampedTargetRate_)
    {
        return (std::min)(clampedCurrentRate_ + step_, clampedTargetRate_);
    }
    return (std::max)(clampedCurrentRate_ - step_, clampedTargetRate_);
}
