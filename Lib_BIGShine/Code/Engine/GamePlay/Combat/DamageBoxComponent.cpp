#include "DamageBoxComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Physics/HitBox.h"

#include <algorithm>
#include <imgui.h>

void DamageBoxComponent::Start()
{
    ResolveHitBox();
    ApplySettings();
    if (autoActivate) Rearm();
}

void DamageBoxComponent::Update(float elapsedTime_)
{
    ResolveHitBox();
    const std::shared_ptr<HitBox> hitBox_ = hitBox.lock();
    if (!hitBox_) return;

    ApplySettings();
    if (!autoActivate)
    {
        hitBox_->Deactivate();
        rearmTimer = 0.0f;
        return;
    }

    rearmTimer += (std::max)(elapsedTime_, 0.0f);
    if (rearmTimer >= (std::max)(rearmInterval, 0.01f)) Rearm();
}

void DamageBoxComponent::ResolveHitBox()
{
    if (!hitBox.expired()) return;
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_) hitBox = owner_->GetOrAddComponent<HitBox>();
}

void DamageBoxComponent::ApplySettings()
{
    const std::shared_ptr<HitBox> hitBox_ = hitBox.lock();
    if (!hitBox_) return;

    hitBox_->damage = (std::max)(damage, 0.0f);
    hitBox_->hitStopDuration = (std::max)(hitStopDuration, 0.0f);
    hitBox_->hitStopScale = (std::clamp)(hitStopScale, 0.0f, 1.0f);
    hitBox_->knockBackPower = (std::max)(knockBackPower, 0.0f);
    hitBox_->allowMultiHit = false;
    hitBox_->hitOncePerActivation = true;
    hitBox_->canHitOwner = false;
    hitBox_->attackTag = "DamageBox";
}

void DamageBoxComponent::Rearm()
{
    const std::shared_ptr<HitBox> hitBox_ = hitBox.lock();
    if (!hitBox_) return;
    hitBox_->Deactivate();
    hitBox_->Activate();
    rearmTimer = 0.0f;
}

void DamageBoxComponent::Gui()
{
    ImGui::Checkbox("Auto Activate", &autoActivate);
    ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Hit Stop Duration", &hitStopDuration, 0.001f, 0.0f, 10.0f);
    ImGui::DragFloat("Hit Stop Scale", &hitStopScale, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Knock Back Power", &knockBackPower, 0.1f, 0.0f, 99999.0f);
    ImGui::DragFloat("Rearm Interval", &rearmInterval, 0.01f, 0.01f, 30.0f);
    if (ImGui::Button("Rearm Now")) Rearm();
}

void DamageBoxComponent::Serialize(nlohmann::json& json_) const
{
    json_["Damage"] = damage;
    json_["HitStopDuration"] = hitStopDuration;
    json_["HitStopScale"] = hitStopScale;
    json_["KnockBackPower"] = knockBackPower;
    json_["RearmInterval"] = rearmInterval;
    json_["AutoActivate"] = autoActivate;
}

void DamageBoxComponent::Deserialize(const nlohmann::json& json_)
{
    damage = (std::max)(json_.value("Damage", 5.0f), 0.0f);
    hitStopDuration = (std::max)(json_.value("HitStopDuration", 0.05f), 0.0f);
    hitStopScale = (std::clamp)(json_.value("HitStopScale", 0.0f), 0.0f, 1.0f);
    knockBackPower = (std::max)(json_.value("KnockBackPower", 5.0f), 0.0f);
    rearmInterval = (std::max)(json_.value("RearmInterval", 0.8f), 0.01f);
    autoActivate = json_.value("AutoActivate", true);
}
