#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <string>
#include <unordered_map>

class HitBox;

// ジャスト回避受付用のCapsuleCollider。
// 有効中にHitBoxと重なるとJustDodgeSystemへ成功候補として渡される。
class JustDodgeCollider : public CapsuleCollider
{
public:
    JustDodgeCollider();

    const std::string& GetName() const override;

    bool HasFixedCollisionRole() const override
    {
        return true;
    }

    bool HasFixedPhysicsBehavior() const override
    {
        return true;
    }

    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void Activate();
    void Deactivate();

    bool CanReactToAttack(const HitBox& hitBox_) const;
    void RecordDodgedAttack(const HitBox& hitBox_);
    void ClearDodgedAttackHistory();

    std::string GetActivateActionName() const;
    std::string GetDeactivateActionName() const;
    std::string GetActionPrefix() const;

private:
    // JustDodge専用のCollisionRoleとPhysics挙動を再適用する。
    void EnforceJustDodgeSettings();
    // ActionSequenceからColliderの有効/無効を切り替えるActionを登録する。
    void RegisterActions();

private:
    std::string actionPrefix = "JustDodge"; // 登録Action名のPrefix。
    std::string activateActionName = "EnableJustDodgeCollider"; // 有効化Action名。
    std::string deactivateActionName = "DisableJustDodgeCollider"; // 無効化Action名。

    bool canReactToOwnerAttack = false; // 自分/同階層の攻撃へ反応するか。
    bool oncePerAttackActivation = true; // 同じ攻撃判定へ1回だけ反応するか。
    int teamId = 0; // Team判定用ID。

    // HitBoxごとのActivation IDを覚え、同じ攻撃への重複JustDodgeを防ぐ。
    std::unordered_map<const HitBox*, uint64_t> dodgedAttackActivationIds;
};
