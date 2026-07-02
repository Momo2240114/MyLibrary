#include "HitReactionComponent.h"

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/TimeManager.h"

#include <algorithm>
#include <imgui.h>

void HitReactionComponent::Start()
{
    RegisterActions();
    currentHealth = (std::clamp)(currentHealth, 0.0f, maxHealth);
}

void HitReactionComponent::Update(float deltaTime_)
{
    if (overlayTimer <= 0.0f) return;

    overlayTimer -= deltaTime_;
    if (overlayTimer <= 0.0f)
    {
        overlayTimer = 0.0f;
        CallOwnerAction(overlayStopActionName);
    }
}

void HitReactionComponent::HandleHit()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    float damage_ = 0.0f;
    float hitStopDuration_ = 0.0f;
    float hitStopScale_ = 0.0f;
    float knockBackPower_ = 0.0f;
    DirectX::SimpleMath::Vector2 hitDirection_{ 0.0f, 0.0f };
    blackboard_->GetFloat("Hit", "Damage", damage_);
    if (!blackboard_->GetFloat("Hit", "HitStopDuration", hitStopDuration_))
    {
        blackboard_->GetFloat("Hit", "HitStopTime", hitStopDuration_);
    }
    blackboard_->GetFloat("Hit", "HitStopScale", hitStopScale_);
    blackboard_->GetFloat("Hit", "KnockBackPower", knockBackPower_);
    blackboard_->GetVector2("Hit", "Direction", hitDirection_);

    lastDamage = (std::max)(damage_, 0.0f);
    currentHealth = (std::max)(currentHealth - lastDamage, 0.0f);
    ++hitCount;
    blackboard_->SetFloat("Hit", "CurrentHealth", currentHealth);
    blackboard_->SetBool("Hit", "Dead", currentHealth <= 0.0f);

    if (useHitStop && hitStopDuration_ > 0.0f)
    {
        const std::shared_ptr<TimeManager> timeManager_ =
            owner_->GetManager().GetTimeManager();
        if (timeManager_)
        {
            timeManager_->TriggerHitStop(
                static_cast<uint32_t>(TarGetLayerType::All),
                hitStopDuration_,
                hitStopScale_);
        }
    }

    if (useKnockBack && knockBackPower_ > 0.0f)
    {
        const std::shared_ptr<CharacterMovementComponent> movement_ =
            owner_->GetComponent<CharacterMovementComponent>();
        if (movement_)
        {
            movement_->AddImpulse({
                hitDirection_.x * knockBackPower_,
                0.0f,
                hitDirection_.y * knockBackPower_ });
        }
    }

    CallOwnerAction(reactionActionName);
    CallOwnerAction(overlayStartActionName);
    overlayTimer = (std::max)(overlayDuration, 0.0f);
    if (overlayTimer <= 0.0f) CallOwnerAction(overlayStopActionName);
}

void HitReactionComponent::ResetHealth()
{
    currentHealth = maxHealth;
    lastDamage = 0.0f;
    hitCount = 0;
}

void HitReactionComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<HitReactionComponent> weakReaction_;
    for (const std::shared_ptr<Component>& component_ : owner_->components)
    {
        const std::shared_ptr<HitReactionComponent> reaction_ =
            std::dynamic_pointer_cast<HitReactionComponent>(component_);
        if (reaction_.get() == this)
        {
            weakReaction_ = reaction_;
            break;
        }
    }

    owner_->RegisterAction("Combat", "OnHit", [weakReaction_]()
    {
        if (const std::shared_ptr<HitReactionComponent> reaction_ = weakReaction_.lock())
        {
            reaction_->HandleHit();
        }
    });
}

void HitReactionComponent::CallOwnerAction(const std::string& actionName_)
{
    if (actionName_.empty() || actionName_ == "OnHit") return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (owner_) owner_->ExecuteAction(actionName_);
}

void HitReactionComponent::Gui()
{
    ImGui::Text("Health: %.1f / %.1f", currentHealth, maxHealth);
    ImGui::Text("Last Damage: %.1f  Hit Count: %d", lastDamage, hitCount);
    if (ImGui::DragFloat("Max Health", &maxHealth, 1.0f, 1.0f, 999999.0f))
    {
        currentHealth = (std::min)(currentHealth, maxHealth);
    }
    if (ImGui::Button("Test OnHit")) HandleHit();
    ImGui::SameLine();
    if (ImGui::Button("Reset Health")) ResetHealth();

    ImGui::SeparatorText("Hit Reaction");
    ImGui::Checkbox("Use Hit Stop", &useHitStop);
    ImGui::InputScalar("Hit Stop Layer Mask", ImGuiDataType_U32, &hitStopLayerMask);
    ImGui::Checkbox("Use Knock Back", &useKnockBack);

    char reactionActionBuffer_[128];
    strncpy_s(reactionActionBuffer_, reactionActionName.c_str(), sizeof(reactionActionBuffer_));
    if (ImGui::InputText("Reaction Action", reactionActionBuffer_, sizeof(reactionActionBuffer_)))
    {
        reactionActionName = reactionActionBuffer_;
    }
    char overlayStartBuffer_[128];
    strncpy_s(overlayStartBuffer_, overlayStartActionName.c_str(), sizeof(overlayStartBuffer_));
    if (ImGui::InputText("Overlay Start Action", overlayStartBuffer_, sizeof(overlayStartBuffer_)))
    {
        overlayStartActionName = overlayStartBuffer_;
    }
    char overlayStopBuffer_[128];
    strncpy_s(overlayStopBuffer_, overlayStopActionName.c_str(), sizeof(overlayStopBuffer_));
    if (ImGui::InputText("Overlay Stop Action", overlayStopBuffer_, sizeof(overlayStopBuffer_)))
    {
        overlayStopActionName = overlayStopBuffer_;
    }
    ImGui::DragFloat("Overlay Duration", &overlayDuration, 0.01f, 0.0f, 10.0f);
    ImGui::TextDisabled("Registered Action: Combat / OnHit");
}

void HitReactionComponent::Serialize(nlohmann::json& json_) const
{
    json_["MaxHealth"] = maxHealth;
    json_["CurrentHealth"] = currentHealth;
    json_["UseHitStop"] = useHitStop;
    json_["HitStopLayerMask"] = hitStopLayerMask;
    json_["UseKnockBack"] = useKnockBack;
    json_["ReactionActionName"] = reactionActionName;
    json_["OverlayStartActionName"] = overlayStartActionName;
    json_["OverlayStopActionName"] = overlayStopActionName;
    json_["OverlayDuration"] = overlayDuration;
}

void HitReactionComponent::Deserialize(const nlohmann::json& json_)
{
    maxHealth = (std::max)(json_.value("MaxHealth", 100.0f), 1.0f);
    currentHealth = (std::clamp)(
        json_.value("CurrentHealth", maxHealth),
        0.0f,
        maxHealth);
    useHitStop = json_.value("UseHitStop", true);
    hitStopLayerMask = json_.value("HitStopLayerMask", 3u);
    useKnockBack = json_.value("UseKnockBack", true);
    reactionActionName = json_.value("ReactionActionName", std::string{});
    overlayStartActionName = json_.value(
        "OverlayStartActionName",
        std::string("MaterialOverlay_Start"));
    overlayStopActionName = json_.value(
        "OverlayStopActionName",
        std::string("MaterialOverlay_Stop"));
    overlayDuration = (std::max)(json_.value("OverlayDuration", 0.12f), 0.0f);
}
