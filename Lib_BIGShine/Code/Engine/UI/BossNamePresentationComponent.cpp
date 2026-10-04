#include "PrecompiledHeader.h"
#include "BossNamePresentationComponent.h"
#include "UITextComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include <algorithm>
#include <imgui.h>
#include <imgui_stdlib.h>

void BossNamePresentationComponent::Start()
{
    HideBossName();
    RegisterActions();
}

void BossNamePresentationComponent::RegisterActions()
{
    const auto owner_ = GetGameObject();
    if (!owner_) return;
    const std::weak_ptr<GameObject> weakOwner_ = owner_;
    owner_->RegisterGlobalAction("UI", "ShowBossName", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        const auto component_ = owner_ ? owner_->GetComponent<BossNamePresentationComponent>() : nullptr;
        if (component_) component_->ShowBossName();
    });
    owner_->RegisterGlobalAction("UI", "HideBossName", [weakOwner_]()
    {
        const auto owner_ = weakOwner_.lock();
        const auto component_ = owner_ ? owner_->GetComponent<BossNamePresentationComponent>() : nullptr;
        if (component_) component_->HideBossName();
    });
}

void BossNamePresentationComponent::ShowBossName()
{
    BS_PROFILE_SCOPE("BossNamePresentationComponent::ShowBossName");
    const auto owner_ = GetGameObject();
    const auto text_ = owner_ ? owner_->GetComponent<UITextComponent>() : nullptr;
    if (!text_) return;
    text_->SetText(bossName + (subName.empty() ? "" : "\n" + subName));
    elapsed = 0.0f;
    playing = true;
    ApplyAlpha(fadeInDuration <= 0.0f ? 1.0f : 0.0f);
}

void BossNamePresentationComponent::HideBossName()
{
    playing = false;
    elapsed = 0.0f;
    ApplyAlpha(0.0f);
}

void BossNamePresentationComponent::ApplyAlpha(float alpha_)
{
    const auto owner_ = GetGameObject();
    const auto sprite_ = owner_ ? owner_->GetComponent<SpriteRenderer>() : nullptr;
    if (!sprite_) return;
    auto color_ = sprite_->GetColor();
    color_.w = std::clamp(alpha_, 0.0f, 1.0f);
    sprite_->SetColor(color_);
}

void BossNamePresentationComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("BossNamePresentationComponent::Update");
    if (!playing) return;
    elapsed += (std::max)(deltaTime_, 0.0f);
    // 表示時間は完全に見える時間として扱い、前後にFade時間を加える。
    const float fadeOutStart_ = fadeInDuration + displayDuration;
    if (elapsed >= fadeOutStart_ + fadeOutDuration) { HideBossName(); return; }
    float alpha_ = 1.0f;
    if (fadeInDuration > 0.0f && elapsed < fadeInDuration) alpha_ = elapsed / fadeInDuration;
    else if (fadeOutDuration > 0.0f && elapsed > fadeOutStart_)
        alpha_ = 1.0f - (elapsed - fadeOutStart_) / fadeOutDuration;
    ApplyAlpha(alpha_);
}

void BossNamePresentationComponent::Gui()
{
    ImGui::InputText("Boss Name", &bossName);
    ImGui::InputText("Sub Name", &subName);
    ImGui::DragFloat("Display Duration", &displayDuration, 0.01f, 0.0f, 60.0f);
    ImGui::DragFloat("Fade In Duration", &fadeInDuration, 0.01f, 0.0f, 60.0f);
    ImGui::DragFloat("Fade Out Duration", &fadeOutDuration, 0.01f, 0.0f, 60.0f);
    if (ImGui::Button("Show Boss Name")) ShowBossName();
}

void BossNamePresentationComponent::Serialize(nlohmann::json& json_) const
{
    json_ = { {"BossName", bossName}, {"SubName", subName}, {"DisplayDuration", displayDuration},
        {"FadeInDuration", fadeInDuration}, {"FadeOutDuration", fadeOutDuration} };
}

void BossNamePresentationComponent::Deserialize(const nlohmann::json& json_)
{
    bossName = json_.value("BossName", std::string("黒焔竜"));
    subName = json_.value("SubName", std::string("DRAGON NAME"));
    displayDuration = (std::max)(json_.value("DisplayDuration", 1.2f), 0.0f);
    fadeInDuration = (std::max)(json_.value("FadeInDuration", 0.35f), 0.0f);
    fadeOutDuration = (std::max)(json_.value("FadeOutDuration", 0.45f), 0.0f);
    playing = false;
}
