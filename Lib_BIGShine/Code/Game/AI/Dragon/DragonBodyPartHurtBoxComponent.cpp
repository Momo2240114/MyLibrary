#include "PrecompiledHeader.h"

#include "DragonBodyPartHurtBoxComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Physics/ColliderComponent.h"

#include <imgui.h>

void DragonBodyPartHurtBoxComponent::Start()
{
    ResolveReferences();
}

std::shared_ptr<GameObject> DragonBodyPartHurtBoxComponent::GetCombatOwner()
{
    ResolveReferences();
    return cachedCombatOwner.lock();
}

std::shared_ptr<DragonBodyPartManagerComponent>
DragonBodyPartHurtBoxComponent::GetBodyPartManager()
{
    ResolveReferences();
    return cachedBodyPartManager.lock();
}

void DragonBodyPartHurtBoxComponent::ResolveReferences()
{
    std::shared_ptr<GameObject> owner_ = cachedCombatOwner.lock();
    if (!owner_)
    {
        const std::shared_ptr<GameObject> self_ = GetGameObject();
        if (!self_) return;

        if (!combatOwnerObjectName.empty() && self_->manager)
        {
            owner_ = self_->GetManager().Find(combatOwnerObjectName);
        }

        if (!owner_)
        {
            if (const std::shared_ptr<ColliderComponent> collider_ =
                self_->GetComponent<ColliderComponent>())
            {
                owner_ = collider_->ResolveCollisionOwnerObject();
            }
        }

        if (!owner_)
        {
            owner_ = FindOwnerFromHierarchy();
        }

        cachedCombatOwner = owner_;
    }

    if (owner_ && cachedBodyPartManager.expired())
    {
        cachedBodyPartManager =
            owner_->GetComponent<DragonBodyPartManagerComponent>();
    }
}

std::shared_ptr<GameObject> DragonBodyPartHurtBoxComponent::FindOwnerFromHierarchy()
{
    std::shared_ptr<GameObject> current_ = GetGameObject();
    while (current_ && current_->transform)
    {
        if (current_->GetComponent<DragonBodyPartManagerComponent>())
        {
            return current_;
        }

        const std::shared_ptr<Transform> parentTransform_ =
            current_->transform->parent.lock();
        if (!parentTransform_) break;
        current_ = parentTransform_->gameObject.lock();
    }
    return nullptr;
}

void DragonBodyPartHurtBoxComponent::Gui()
{
    ResolveReferences();

    const char* bodyPartNames_[] =
    {
        "HEAD",
        "NECK",
        "CHEST",
        "BODY",
        "LEFT_FRONT_LEG",
        "RIGHT_FRONT_LEG",
        "LEFT_BACK_LEG",
        "RIGHT_BACK_LEG",
        "LEFT_WING",
        "RIGHT_WING",
        "TAIL"
    };

    int bodyPartIndex_ = static_cast<int>(bodyPart);
    if (ImGui::Combo(
        "Body Part",
        &bodyPartIndex_,
        bodyPartNames_,
        IM_ARRAYSIZE(bodyPartNames_)))
    {
        bodyPart = static_cast<DRAGON_BODY_PART>(bodyPartIndex_);
    }

    char ownerNameBuffer_[128];
    strncpy_s(
        ownerNameBuffer_,
        combatOwnerObjectName.c_str(),
        sizeof(ownerNameBuffer_));
    if (ImGui::InputText(
        "Combat Owner",
        ownerNameBuffer_,
        sizeof(ownerNameBuffer_)))
    {
        combatOwnerObjectName = ownerNameBuffer_;
        cachedCombatOwner.reset();
        cachedBodyPartManager.reset();
        ResolveReferences();
    }

    const std::shared_ptr<GameObject> owner_ = cachedCombatOwner.lock();
    ImGui::Text(
        "Resolved Owner: %s",
        owner_ ? owner_->GetName().c_str() : "None");
    ImGui::Text(
        "Manager: %s",
        cachedBodyPartManager.expired() ? "None" : "Ready");
}

void DragonBodyPartHurtBoxComponent::Serialize(nlohmann::json& json_) const
{
    json_["BodyPart"] = GetDragonBodyPartName(bodyPart);
    json_["CombatOwnerObjectName"] = combatOwnerObjectName;
}

void DragonBodyPartHurtBoxComponent::Deserialize(const nlohmann::json& json_)
{
    bodyPart = ParseDragonBodyPart(
        json_.value("BodyPart", std::string("BODY")));
    combatOwnerObjectName =
        json_.value("CombatOwnerObjectName", std::string{});
    cachedCombatOwner.reset();
    cachedBodyPartManager.reset();
}
