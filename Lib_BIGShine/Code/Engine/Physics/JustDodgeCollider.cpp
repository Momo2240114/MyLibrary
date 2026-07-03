#include "JustDodgeCollider.h"

#include "HitBox.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

JustDodgeCollider::JustDodgeCollider()
{
    enabled = false;
    EnforceJustDodgeSettings();
    debugPrimitiveColor = { 0.2f, 0.6f, 1.0f, 1.0f };
}

const std::string& JustDodgeCollider::GetName() const
{
    static std::string name_ = "JustDodgeCollider";
    return name_;
}

void JustDodgeCollider::Start()
{
    CapsuleCollider::Start();
    RegisterActions();
    Deactivate();
}

void JustDodgeCollider::Activate()
{
    ClearDodgedAttackHistory();
    currentTriggers.clear();
    SetEnabled(true);
}

void JustDodgeCollider::Deactivate()
{
    SetEnabled(false);
    currentTriggers.clear();
    ClearDodgedAttackHistory();
}

bool JustDodgeCollider::CanReactToAttack(const HitBox& hitBox_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    const std::shared_ptr<GameObject> attackOwner_ =
        const_cast<HitBox&>(hitBox_).GetAttackOwner();

    if (!canReactToOwnerAttack &&
        owner_ &&
        attackOwner_ &&
        owner_.get() == attackOwner_.get())
    {
        return false;
    }

    if (teamId != 0 && hitBox_.teamId != 0 && teamId == hitBox_.teamId)
    {
        return false;
    }

    const auto iterator_ = dodgedAttackActivationIds.find(&hitBox_);
    if (oncePerAttackActivation &&
        iterator_ != dodgedAttackActivationIds.end() &&
        iterator_->second == hitBox_.GetActivationId())
    {
        return false;
    }

    return true;
}

void JustDodgeCollider::RecordDodgedAttack(const HitBox& hitBox_)
{
    dodgedAttackActivationIds[&hitBox_] = hitBox_.GetActivationId();
}

void JustDodgeCollider::ClearDodgedAttackHistory()
{
    dodgedAttackActivationIds.clear();
}

void JustDodgeCollider::Gui()
{
    CapsuleCollider::Gui();

    ImGui::SeparatorText("Just Dodge Collider");

    ImGui::TextColored(
        enabled ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.2f, 1.0f),
        enabled ? "JustDodgeCollider: ACTIVE" : "JustDodgeCollider: INACTIVE");

    if (ImGui::Button("Activate JustDodgeCollider")) Activate();
    ImGui::SameLine();
    if (ImGui::Button("Deactivate JustDodgeCollider")) Deactivate();

    ImGui::Checkbox("Can React To Owner Attack", &canReactToOwnerAttack);
    ImGui::Checkbox("Once Per Attack Activation", &oncePerAttackActivation);
    ImGui::DragInt("Team ID", &teamId, 1, 0, 999);

    ImGui::SeparatorText("Actions");

    char actionPrefixBuffer_[64];
    strncpy_s(actionPrefixBuffer_, actionPrefix.c_str(), sizeof(actionPrefixBuffer_));
    if (ImGui::InputText("Action Prefix", actionPrefixBuffer_, sizeof(actionPrefixBuffer_)))
    {
        actionPrefix = actionPrefixBuffer_;
    }

    ImGui::Text("Activate   : %s", GetActivateActionName().c_str());
    ImGui::Text("Deactivate : %s", GetDeactivateActionName().c_str());

    if (ImGui::Button("Register Just Dodge Actions"))
    {
        RegisterActions();
    }
}

void JustDodgeCollider::Serialize(nlohmann::json& json_) const
{
    CapsuleCollider::Serialize(json_);

    json_["ActionPrefix"] = actionPrefix;
    json_["ActivateActionName"] = activateActionName;
    json_["DeactivateActionName"] = deactivateActionName;
    json_["CanReactToOwnerAttack"] = canReactToOwnerAttack;
    json_["OncePerAttackActivation"] = oncePerAttackActivation;
    json_["TeamId"] = teamId;
}

void JustDodgeCollider::Deserialize(const nlohmann::json& json_)
{
    CapsuleCollider::Deserialize(json_);

    actionPrefix = json_.value("ActionPrefix", std::string("JustDodge"));
    activateActionName = json_.value(
        "ActivateActionName",
        std::string("EnableJustDodgeCollider"));
    deactivateActionName = json_.value(
        "DeactivateActionName",
        std::string("DisableJustDodgeCollider"));

    canReactToOwnerAttack = json_.value("CanReactToOwnerAttack", false);
    oncePerAttackActivation = json_.value("OncePerAttackActivation", true);
    teamId = json_.value("TeamId", 0);

    ClearDodgedAttackHistory();
    EnforceJustDodgeSettings();
}

std::string JustDodgeCollider::GetActionPrefix() const
{
    if (!actionPrefix.empty())
    {
        return actionPrefix;
    }

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (owner_)
    {
        return owner_->GetName();
    }

    return "JustDodge";
}

std::string JustDodgeCollider::GetActivateActionName() const
{
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return activateActionName;
    return prefix_ + "_" + activateActionName;
}

std::string JustDodgeCollider::GetDeactivateActionName() const
{
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return deactivateActionName;
    return prefix_ + "_" + deactivateActionName;
}

void JustDodgeCollider::EnforceJustDodgeSettings()
{
    collisionRole = CollisionRole::JUST_DODGE_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = false;
}

void JustDodgeCollider::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<JustDodgeCollider> weakCollider_;
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        const std::shared_ptr<JustDodgeCollider> collider_ =
            std::dynamic_pointer_cast<JustDodgeCollider>(component_);

        if (collider_.get() == this)
        {
            weakCollider_ = collider_;
            break;
        }
    }

    owner_->RegisterAction("Player", GetActivateActionName(), [weakCollider_]()
        {
            if (const std::shared_ptr<JustDodgeCollider> collider_ = weakCollider_.lock())
            {
                collider_->Activate();
            }
        });

    owner_->RegisterAction("Player", GetDeactivateActionName(), [weakCollider_]()
        {
            if (const std::shared_ptr<JustDodgeCollider> collider_ = weakCollider_.lock())
            {
                collider_->Deactivate();
            }
        });
}