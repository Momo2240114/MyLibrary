#include "PrecompiledHeader.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/Physics/CharacterCollider.h"

#include "LockOnTargetComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

#include <imgui_stdlib.h>

#include <algorithm>

DirectX::XMFLOAT3 LockOnTargetComponent::GetFocusPosition() const
{
    // Transformが無い場合でもOffsetを返し、呼び出し側が不定値を扱わないようにする。
    auto owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform)
    {
        return offset;
    }

    const DirectX::XMFLOAT3 position_ = owner_->transform->GetWorldPosition();
    // LockOnの注視点はObject原点ではなく、胸/頭付近へOffsetする。
    return
    {
        position_.x + offset.x,
        position_.y + offset.y,
        position_.z + offset.z
    };
}

std::shared_ptr<GameObject> LockOnTargetComponent::GetCombatOwner() const
{
    // 既に解決済みのCombatOwnerがあればそれを使う。
    if (const std::shared_ptr<GameObject> owner_ = combatOwner.lock())
    {
        return owner_;
    }

    const std::shared_ptr<GameObject> currentObject_ = gameObject.lock();
    if (!currentObject_) return nullptr;

    if (!combatOwnerName.empty() && currentObject_->manager)
    {
        // 名前指定がある場合はScene内からCombatOwnerを探す。
        if (const std::shared_ptr<GameObject> namedOwner_ =
            currentObject_->manager->Find(combatOwnerName))
        {
            return namedOwner_;
        }
    }

    // 指定が無い場合はLockOnTarget自身をCombatOwnerとして扱う。
    return currentObject_;
}

bool LockOnTargetComponent::IsAvailable() const
{
    const auto object_ = gameObject.lock();
    const auto owner_ = GetCombatOwner();
    if (!IsEnabled() || !lockOnEnabled || !object_ || !object_->manager || object_->IsPendingDestroy() ||
        !owner_ || !owner_->manager || !owner_->IsCombatReady()) return false;
    if (const auto health_ = owner_->GetComponent<HealthComponent>()) if (health_->IsDead()) return false;
    if (const auto collider_ = owner_->GetComponent<CharacterCollider>()) if (!collider_->IsEnabled()) return false;
    return true;
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
    // LockOn候補として必要な注視点/Owner/優先度設定を保存する。
    json_["Offset"] = { offset.x, offset.y, offset.z };
    json_["CombatOwnerName"] = combatOwnerName;
    json_["LockOnEnabled"] = lockOnEnabled;
    json_["DefaultTarget"] = defaultTarget;
    json_["Priority"] = priority;
}

void LockOnTargetComponent::Deserialize(const nlohmann::json& json_)
{
    // Offsetは3要素配列のときだけ復元し、古い/壊れたDataでは既定値を残す。
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
