#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Game/AI/Dragon/DragonBodyPartManagerComponent.h"

#include <memory>
#include <string>

// Dragonの子HurtBoxに付け、Hitした部位と本体Managerへの参照を解決するComponent
// 実際のダメージ計算はDragonBodyPartManagerComponentへ委譲する
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

    // このHurtBoxが表すDragon部位を返す
    DRAGON_BODY_PART GetBodyPart() const { return bodyPart; }
    // ダメージを集約するDragon本体Objectを返す
    std::shared_ptr<GameObject> GetCombatOwner();
    // Dragon本体に付いている部位管理Componentを返す
    std::shared_ptr<DragonBodyPartManagerComponent> GetBodyPartManager();

private:
    // combatOwnerObjectName、ColliderのOwner、親階層から本体参照を解決する
    void ResolveReferences();
    // 親Transformをたどり、DragonBodyPartManagerComponentを持つObjectを探す
    std::shared_ptr<GameObject> FindOwnerFromHierarchy();

    // このHurtBoxが担当する部位
    DRAGON_BODY_PART bodyPart = DRAGON_BODY_PART::BODY;
    // 明示的に本体Objectを指定する場合の名前
    std::string combatOwnerObjectName;
    // 解決済みDragon本体Object
    std::weak_ptr<GameObject> cachedCombatOwner;
    // 解決済み部位管理Component
    std::weak_ptr<DragonBodyPartManagerComponent> cachedBodyPartManager;
};
