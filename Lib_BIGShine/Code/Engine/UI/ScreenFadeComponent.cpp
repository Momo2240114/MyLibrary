#include "PrecompiledHeader.h"
#include "ScreenFadeComponent.h"
#include "Engine/GamePlay/Scene/SceneManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Graphics/Resource/Sprite.h"
#include <algorithm>
#include <cmath>

const std::string& ScreenFadeComponent::GetName() const
{
    static const std::string NAME = "ScreenFadeComponent";
    return NAME;
}

void ScreenFadeComponent::Start()
{
    BS_PROFILE_SCOPE("ScreenFadeComponent::Start");
    elapsedTime = 0.0f;
    fadingOut = false;
    completionDispatched = false;
    completionAction.clear();
    sceneTransitionHandled = SceneManager::IsTransitionActive();
    ApplyAlpha(fadeInOnStart && GetManager().GetEditorPlayMode() == EditorPlayMode::PLAY ? 1.0f : 0.0f);
}

void ScreenFadeComponent::ApplyAlpha(float alpha_)
{
    const auto owner_ = GetGameObject();
    const auto sprite_ = owner_ ? owner_->GetComponent<SpriteRenderer>() : nullptr;
    if (!sprite_) return;
    auto color_ = sprite_->GetConfiguredColor();
    color_.w *= std::clamp(alpha_, 0.0f, 1.0f);
    sprite_->SetRuntimeColor(color_);
    sprite_->SetRuntimeVisible(color_.w > 0.0f);
}

bool ScreenFadeComponent::BeginFadeOut(const std::string& action_)
{
    if (fadingOut || (!action_.empty() && !ActionRegistry::Instance().HasAction(action_))) return false;
    fadingOut = true;
    elapsedTime = 0.0f;
    completionDispatched = false;
    completionAction = action_;
    return true;
}

void ScreenFadeComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("ScreenFadeComponent::Update");
    if (GetManager().GetSceneRuntimeState() != SCENE_RUNTIME_STATE::RUNNING ||
        GetManager().GetEditorPlayMode() != EditorPlayMode::PLAY) return;
    elapsedTime += (std::max)(0.0f, deltaTime_);
    if (!fadingOut)
    {
        // 共通マスクへ引き継いだ初回Fadeを再開せず、開き終わった直後の暗転戻りを防ぐ。
        sceneTransitionHandled = sceneTransitionHandled || SceneManager::IsTransitionActive();
        ApplyAlpha(fadeInOnStart && !sceneTransitionHandled ? 1.0f - std::clamp(elapsedTime / (std::max)(fadeInDuration, 0.001f), 0.0f, 1.0f) : 0.0f);
        return;
    }
    // Scene Actionは共通の最前面マスクへ引き継ぎ、終了等の既存ActionはSpriteフェードを維持する。
    if (ActionRegistry::Instance().HasActionInCategory("Scene",completionAction))
    {
        ApplyAlpha(0.0f);
        if (!completionDispatched && elapsedTime >= fadeOutDelay)
        {
            completionDispatched = true;
            ActionRegistry::Instance().Call(completionAction);
        }
        return;
    }
    ApplyAlpha(std::clamp((elapsedTime - fadeOutDelay) / (std::max)(fadeOutDuration, 0.001f), 0.0f, 1.0f));
    if (!completionDispatched && elapsedTime >= fadeOutDelay + fadeOutDuration)
    {
        // Actionから次Sceneが予約されても、同じ暗転で二度実行しない。
        completionDispatched = true;
        if (!completionAction.empty()) ActionRegistry::Instance().Call(completionAction);
    }
}

void ScreenFadeComponent::Gui()
{
    ImGui::Checkbox("Fade In On Start", &fadeInOnStart);
    ImGui::DragFloat("Fade In Duration", &fadeInDuration, 0.01f, 0.001f, 10.0f);
    ImGui::DragFloat("Fade Out Delay", &fadeOutDelay, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Fade Out Duration", &fadeOutDuration, 0.01f, 0.001f, 10.0f);
}

void ScreenFadeComponent::Serialize(nlohmann::json& json_) const
{
    json_ = {{"FadeInOnStart", fadeInOnStart}, {"FadeInDuration", fadeInDuration},
        {"FadeOutDelay", fadeOutDelay}, {"FadeOutDuration", fadeOutDuration}};
}

void ScreenFadeComponent::Deserialize(const nlohmann::json& json_)
{
    fadeInOnStart = json_.value("FadeInOnStart", true);
    const auto duration_ = [&](const char* key_, float fallback_)
    {
        const float value_ = json_.value(key_, fallback_);
        return std::isfinite(value_) ? (std::max)(0.0f, value_) : fallback_;
    };
    fadeInDuration = duration_("FadeInDuration", 0.7f);
    fadeOutDelay = duration_("FadeOutDelay", 0.0f);
    fadeOutDuration = duration_("FadeOutDuration", 0.6f);
}
