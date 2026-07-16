#include "PrecompiledHeader.h"

#include "GuardCollider.h"

#include "HitBox.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"

#include <algorithm>
#include <imgui.h>

GuardCollider::GuardCollider()
{
    enabled = false;
    EnforceGuardSettings();
    debugPrimitiveColor = { 0.2f, 0.8f, 1.0f, 1.0f };
}

const std::string& GuardCollider::GetName() const
{
    static std::string name_ = "GuardCollider";
    return name_;
}

void GuardCollider::Start()
{
    CapsuleCollider::Start();
    RegisterActions();
    Deactivate();
}

void GuardCollider::Activate()
{
    ClearGuardedAttackHistory();
    currentTriggers.clear();
    SetEnabled(true);
}

void GuardCollider::Deactivate()
{
    justGuardEnabled = false;
    SetEnabled(false);
    currentTriggers.clear();
    ClearGuardedAttackHistory();
}

bool GuardCollider::CanGuardAttack(const HitBox& hitBox_) const
{
    const std::shared_ptr<GameObject> owner_ = GetGuardOwner();
    const std::shared_ptr<GameObject> attackOwner_ =
        const_cast<HitBox&>(hitBox_).GetAttackOwner();

    if (!canGuardOwnerAttack &&
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

    const auto iterator_ = guardedAttackActivationIds.find(&hitBox_);
    if (oncePerAttackActivation &&
        iterator_ != guardedAttackActivationIds.end() &&
        iterator_->second == hitBox_.GetActivationId())
    {
        return false;
    }

    return true;
}

void GuardCollider::RecordGuardedAttack(const HitBox& hitBox_)
{
    guardedAttackActivationIds[&hitBox_] = hitBox_.GetActivationId();
}

void GuardCollider::ClearGuardedAttackHistory()
{
    guardedAttackActivationIds.clear();
}

void GuardCollider::SetGuardOwner(GameObject* guardOwner_)
{
    guardOwner = guardOwner_;
}

std::shared_ptr<GameObject> GuardCollider::GetGuardOwner() const
{
    if (guardOwner != nullptr)
    {
        return guardOwner->shared_from_this();
    }

    return ResolveGuardOwnerFromHierarchy();
}

void GuardCollider::SetJustGuardEnabled(bool enabled_)
{
    justGuardEnabled = enabled_ && IsEnabled();
}

void GuardCollider::Gui()
{
    CapsuleCollider::Gui();

    ImGui::SeparatorText("Guard Collider");
    ImGui::TextColored(
        enabled ? ImVec4(0.2f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.3f, 0.2f, 1.0f),
        enabled ? "GuardCollider: ACTIVE" : "GuardCollider: INACTIVE");
    ImGui::Text("Just Guard: %s", justGuardEnabled ? "ON" : "OFF");

    if (ImGui::Button("Activate GuardCollider")) Activate();
    ImGui::SameLine();
    if (ImGui::Button("Deactivate GuardCollider")) Deactivate();
    if (ImGui::Button("Enable JustGuard")) SetJustGuardEnabled(true);
    ImGui::SameLine();
    if (ImGui::Button("Disable JustGuard")) SetJustGuardEnabled(false);

    ImGui::Checkbox("Can Guard Owner Attack", &canGuardOwnerAttack);
    ImGui::Checkbox("Once Per Attack Activation", &oncePerAttackActivation);
    ImGui::DragInt("Team ID", &teamId, 1, 0, 999);

    ImGui::SeparatorText("Actions");
    char actionPrefixBuffer_[64];
    strncpy_s(actionPrefixBuffer_, actionPrefix.c_str(), sizeof(actionPrefixBuffer_));
    if (ImGui::InputText("Action Prefix", actionPrefixBuffer_, sizeof(actionPrefixBuffer_)))
    {
        actionPrefix = actionPrefixBuffer_;
    }

    ImGui::Text("Activate          : %s", GetActivateActionName().c_str());
    ImGui::Text("Deactivate        : %s", GetDeactivateActionName().c_str());
    ImGui::Text("Enable JustGuard  : %s", GetEnableJustGuardActionName().c_str());
    ImGui::Text("Disable JustGuard : %s", GetDisableJustGuardActionName().c_str());

    if (ImGui::Button("Register Guard Actions"))
    {
        RegisterActions();
    }
}

void GuardCollider::Serialize(nlohmann::json& json_) const
{
    CapsuleCollider::Serialize(json_);

    json_["ActionPrefix"] = actionPrefix;
    json_["ActivateActionName"] = activateActionName;
    json_["DeactivateActionName"] = deactivateActionName;
    json_["EnableJustGuardActionName"] = enableJustGuardActionName;
    json_["DisableJustGuardActionName"] = disableJustGuardActionName;
    json_["CanGuardOwnerAttack"] = canGuardOwnerAttack;
    json_["OncePerAttackActivation"] = oncePerAttackActivation;
    json_["TeamId"] = teamId;
}

void GuardCollider::Deserialize(const nlohmann::json& json_)
{
    CapsuleCollider::Deserialize(json_);

    actionPrefix = json_.value("ActionPrefix", std::string{});
    activateActionName = json_.value(
        "ActivateActionName",
        std::string("EnableGuardCollider"));
    deactivateActionName = json_.value(
        "DeactivateActionName",
        std::string("DisableGuardCollider"));
    enableJustGuardActionName = json_.value(
        "EnableJustGuardActionName",
        std::string("EnableJustGuard"));
    disableJustGuardActionName = json_.value(
        "DisableJustGuardActionName",
        std::string("DisableJustGuard"));
    canGuardOwnerAttack = json_.value("CanGuardOwnerAttack", false);
    oncePerAttackActivation = json_.value("OncePerAttackActivation", true);
    teamId = json_.value("TeamId", 0);

    justGuardEnabled = false;
    ClearGuardedAttackHistory();
    EnforceGuardSettings();
}

std::string GuardCollider::GetActionPrefix() const
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

    return "Guard";
}

std::string GuardCollider::GetActivateActionName() const
{
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return activateActionName;
    return prefix_ + "_" + activateActionName;
}

std::string GuardCollider::GetDeactivateActionName() const
{
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return deactivateActionName;
    return prefix_ + "_" + deactivateActionName;
}

std::string GuardCollider::GetEnableJustGuardActionName() const
{
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return enableJustGuardActionName;
    return prefix_ + "_" + enableJustGuardActionName;
}

std::string GuardCollider::GetDisableJustGuardActionName() const
{
    const std::string prefix_ = GetActionPrefix();
    if (prefix_.empty()) return disableJustGuardActionName;
    return prefix_ + "_" + disableJustGuardActionName;
}

void GuardCollider::EnforceGuardSettings()
{
    collisionRole = CollisionRole::GUARD_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = true;
}

void GuardCollider::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::weak_ptr<GuardCollider> weakCollider_;
    for (const std::shared_ptr<Component>& component_ : owner_->colliderComponents)
    {
        const std::shared_ptr<GuardCollider> collider_ =
            std::dynamic_pointer_cast<GuardCollider>(component_);
        if (collider_.get() == this)
        {
            weakCollider_ = collider_;
            break;
        }
    }

    owner_->RegisterAction("Combat", GetActivateActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->Activate();
        }
    });
    owner_->RegisterAction("Combat", GetDeactivateActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->Deactivate();
        }
    });
    owner_->RegisterAction("Combat", GetEnableJustGuardActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->SetJustGuardEnabled(true);
        }
    });
    owner_->RegisterAction("Combat", GetDisableJustGuardActionName(), [weakCollider_]()
    {
        if (const std::shared_ptr<GuardCollider> collider_ = weakCollider_.lock())
        {
            collider_->SetJustGuardEnabled(false);
        }
    });
}

std::shared_ptr<GameObject> GuardCollider::ResolveGuardOwnerFromHierarchy() const
{
    std::shared_ptr<GameObject> object_ = gameObject.lock();
    std::shared_ptr<GameObject> rootObject_ = object_;

    while (object_)
    {
        const std::string guardActionName_ = object_->BuildActionName("OnGuardSucceeded");
        const std::string justGuardActionName_ = object_->BuildActionName("OnJustGuardSucceeded");
        if (object_->GetComponent<BlackboardComponent>() ||
            object_->localRegistry.HasAction(guardActionName_) ||
            object_->localRegistry.HasAction(justGuardActionName_) ||
            object_->localRegistry.HasAction("OnGuardSucceeded") ||
            object_->localRegistry.HasAction("OnJustGuardSucceeded"))
        {
            return object_;
        }

        if (!object_->transform) break;

        const std::shared_ptr<Transform> parentTransform_ =
            object_->transform->parent.lock();
        if (!parentTransform_) break;

        const std::shared_ptr<GameObject> parentObject_ =
            parentTransform_->gameObject.lock();
        if (!parentObject_) break;

        rootObject_ = parentObject_;
        object_ = parentObject_;
    }

    return rootObject_;
}
