#include "PrecompiledHeader.h"

#include "LockOnTargetComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <imgui_stdlib.h>

#include <algorithm>

DirectX::XMFLOAT3 LockOnTargetComponent::GetFocusPosition() const
{
    auto owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform)
    {
        return offset;
    }

    const DirectX::XMFLOAT3 position_ = owner_->transform->GetWorldPosition();
    return
    {
        position_.x + offset.x,
        position_.y + offset.y,
        position_.z + offset.z
    };
}

std::shared_ptr<GameObject> LockOnTargetComponent::GetCombatOwner() const
{
    if (const std::shared_ptr<GameObject> owner_ = combatOwner.lock())
    {
        return owner_;
    }

    const std::shared_ptr<GameObject> currentObject_ = gameObject.lock();
    if (!currentObject_) return nullptr;

    if (!combatOwnerName.empty() && currentObject_->manager)
    {
        if (const std::shared_ptr<GameObject> namedOwner_ =
            currentObject_->manager->Find(combatOwnerName))
        {
            return namedOwner_;
        }
    }

    return currentObject_;
}

void LockOnTargetComponent::Gui()
{
    ImGui::DragFloat3("Offset", &offset.x, 0.05f);
    if (ImGui::InputText("Combat Owner", &combatOwnerName))
    {
        combatOwner.reset();
    }
    ImGui::Checkbox("Lock On Enabled", &lockOnEnabled);
    ImGui::Checkbox("Default Target", &defaultTarget);
    ImGui::DragFloat("Priority", &priority, 0.05f, 0.0f, 100.0f);
}

void LockOnTargetComponent::Serialize(nlohmann::json& json_) const
{
    json_["Offset"] = { offset.x, offset.y, offset.z };
    json_["CombatOwnerName"] = combatOwnerName;
    json_["LockOnEnabled"] = lockOnEnabled;
    json_["DefaultTarget"] = defaultTarget;
    json_["Priority"] = priority;
}

void LockOnTargetComponent::Deserialize(const nlohmann::json& json_)
{
    if (json_.contains("Offset") && json_["Offset"].is_array() && json_["Offset"].size() >= 3)
    {
        offset =
        {
            json_["Offset"][0].get<float>(),
            json_["Offset"][1].get<float>(),
            json_["Offset"][2].get<float>()
        };
    }
    combatOwnerName = json_.value("CombatOwnerName", std::string{});
    lockOnEnabled = json_.value("LockOnEnabled", true);
    defaultTarget = json_.value("DefaultTarget", false);
    priority = (std::max)(json_.value("Priority", 1.0f), 0.0f);
    combatOwner.reset();
}

REGISTER_COMPONENT(LockOnTargetComponent, "LockOnTargetComponent")
