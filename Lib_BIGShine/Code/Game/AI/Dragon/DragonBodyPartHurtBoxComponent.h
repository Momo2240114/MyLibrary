#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Game/AI/Dragon/DragonBodyPartManagerComponent.h"

#include <memory>
#include <string>

class DragonBodyPartHurtBoxComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "DragonBodyPartHurtBoxComponent";
        return name_;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    DRAGON_BODY_PART GetBodyPart() const { return bodyPart; }
    std::shared_ptr<GameObject> GetCombatOwner();
    std::shared_ptr<DragonBodyPartManagerComponent> GetBodyPartManager();

private:
    void ResolveReferences();
    std::shared_ptr<GameObject> FindOwnerFromHierarchy();

    DRAGON_BODY_PART bodyPart = DRAGON_BODY_PART::BODY;
    std::string combatOwnerObjectName;
    std::weak_ptr<GameObject> cachedCombatOwner;
    std::weak_ptr<DragonBodyPartManagerComponent> cachedBodyPartManager;
};
