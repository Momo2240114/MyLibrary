#include "PrecompiledHeader.h"

#include "DragonBodyPartHurtBoxComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Physics/ColliderComponent.h"

#include <imgui.h>

// 起動時に本体Objectと部位Managerの参照を解決する
void DragonBodyPartHurtBoxComponent::Start()
{
    ResolveReferences();
}

// Hit処理を集約するDragon本体Objectを返す
std::shared_ptr<GameObject> DragonBodyPartHurtBoxComponent::GetCombatOwner()
{
    ResolveReferences();
    return cachedCombatOwner.lock();
}

// Dragon本体側の部位管理Componentを返す
std::shared_ptr<DragonBodyPartManagerComponent>
DragonBodyPartHurtBoxComponent::GetBodyPartManager()
{
    ResolveReferences();
    return cachedBodyPartManager.lock();
}

// 明示名、ColliderのOwner、親階層の順でDragon本体参照を解決する
void DragonBodyPartHurtBoxComponent::ResolveReferences()
{
    std::shared_ptr<GameObject> owner_ = cachedCombatOwner.lock();
    if (!owner_)
    {
        const std::shared_ptr<GameObject> self_ = GetGameObject();
        if (!self_)
        {
            return;
        }

        if (!combatOwnerObjectName.empty() && self_->manager)
        {
            // Editorで本体Object名を指定している場合は、それを最優先で使う
            owner_ = self_->GetManager().Find(combatOwnerObjectName);
        }

        if (!owner_)
        {
            // Colliderが別Objectを戦闘Ownerとして解決できる場合はそれを使う
            if (const std::shared_ptr<ColliderComponent> collider_ =
                self_->GetComponent<ColliderComponent>())
            {
                owner_ = collider_->ResolveCollisionOwnerObject();
            }
        }

        if (!owner_)
        {
            // 最後のFallbackとして親Transformをたどり、Managerを持つObjectを探す。
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

// 親階層をたどり、DragonBodyPartManagerComponentを持つ本体Objectを探す。
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
        if (!parentTransform_)
        {
            break;
        }
        current_ = parentTransform_->gameObject.lock();
    }
    return nullptr;
}

// 部位種類とCombat Owner名を編集し、解決結果をDebug表示する。
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

// このHurtBoxが担当する部位と明示Owner名をJSONへ保存する。
void DragonBodyPartHurtBoxComponent::Serialize(nlohmann::json& json_) const
{
    json_["BodyPart"] = GetDragonBodyPartName(bodyPart);
    json_["CombatOwnerObjectName"] = combatOwnerObjectName;
}

// JSONから部位とOwner名を復元し、キャッシュ済み参照を一度破棄する。
void DragonBodyPartHurtBoxComponent::Deserialize(const nlohmann::json& json_)
{
    bodyPart = ParseDragonBodyPart(
        json_.value("BodyPart", std::string("BODY")));
    combatOwnerObjectName =
        json_.value("CombatOwnerObjectName", std::string{});
    cachedCombatOwner.reset();
    cachedBodyPartManager.reset();
}
