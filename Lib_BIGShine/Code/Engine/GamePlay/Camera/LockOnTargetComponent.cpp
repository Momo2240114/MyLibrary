#include "PrecompiledHeader.h"

#include "LockOnTargetComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

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

void LockOnTargetComponent::Gui()
{
    ImGui::DragFloat3("Offset", &offset.x, 0.05f);
}

void LockOnTargetComponent::Serialize(nlohmann::json& json_) const
{
    json_["Offset"] = { offset.x, offset.y, offset.z };
}

void LockOnTargetComponent::Deserialize(const nlohmann::json& json_)
{
    if (!json_.contains("Offset") || !json_["Offset"].is_array() || json_["Offset"].size() < 3) return;

    offset =
    {
        json_["Offset"][0].get<float>(),
        json_["Offset"][1].get<float>(),
        json_["Offset"][2].get<float>()
    };
}

REGISTER_COMPONENT(LockOnTargetComponent, "LockOnTargetComponent")
