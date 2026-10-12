#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <memory>
#include <string>

class GameObject;

class LockOnTargetComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "LockOnTargetComponent";
        return name;
    }

    // LockOn Cameraが注視するWorld位置を返す。
    DirectX::XMFLOAT3 GetFocusPosition() const;
    // 攻撃/HPなどCombat情報を持つOwnerを返す。
    std::shared_ptr<GameObject> GetCombatOwner() const;
    bool IsLockOnEnabled() const { return lockOnEnabled; }
    // 生存・戦闘許可・削除予約・物理の状態を合わせて候補資格を判定する。
    bool IsAvailable() const;
    bool IsDefaultTarget() const { return defaultTarget; }
    float GetPriority() const { return priority; }
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // Object位置から注視点をずらすOffset。
    DirectX::XMFLOAT3 offset{ 0.0f, 1.2f, 0.0f };
    // CombatOwnerを別Objectにしたい場合の名前。
    std::string combatOwnerName;
    std::weak_ptr<GameObject> combatOwner;
    // LockOn候補として有効かどうか。
    bool lockOnEnabled = true;
    // 初期LockOn候補として優先するかどうか。
    bool defaultTarget = false;
    // 候補選択時の優先度。
    float priority = 1.0f;
};
