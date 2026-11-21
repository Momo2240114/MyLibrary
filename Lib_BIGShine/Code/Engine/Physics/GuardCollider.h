#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class GameObject;
class HitBox;

// ガード受付用のCapsuleCollider。
// 有効中にHitBoxと重なると通常GuardまたはJustGuardのSystemへ渡される。
class GuardCollider : public CapsuleCollider
{
public:
    GuardCollider();

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

    bool CanGuardAttack(const HitBox& hitBox_) const;
    bool HasGuardedAttackRecord(const HitBox& hitBox_) const;
    void RecordGuardedAttack(const HitBox& hitBox_);
    void ClearGuardedAttackHistory();

    bool GuardsBodyContact() const
    {
        return guardBodyContact;
    }

    float GetGuardBreakDamageRate() const
    {
        return guardBreakDamageRate;
    }

    void SetGuardOwner(GameObject* guardOwner_);
    std::shared_ptr<GameObject> GetGuardOwner() const;

    bool IsJustGuardEnabled() const
    {
        return justGuardEnabled;
    }

    void SetJustGuardEnabled(bool enabled_);

    std::string GetActivateActionName() const;
    std::string GetDeactivateActionName() const;
    std::string GetEnableJustGuardActionName() const;
    std::string GetDisableJustGuardActionName() const;
    std::string GetActionPrefix() const;

private:
    // Guard専用のCollisionRoleとPhysics挙動を再適用する。
    void EnforceGuardSettings();
    // ActionSequenceからGuard受付やJustGuard受付を切り替えるActionを登録する。
    void RegisterActions();
    // 子ObjectにGuardColliderがある場合、親方向からGuard所有者を解決する。
    std::shared_ptr<GameObject> ResolveGuardOwnerFromHierarchy() const;

private:
    std::string actionPrefix; // 登録Action名のPrefix。
    std::string activateActionName = "EnableGuardCollider"; // GuardCollider有効化Action名。
    std::string deactivateActionName = "DisableGuardCollider"; // GuardCollider無効化Action名。
    std::string enableJustGuardActionName = "EnableJustGuard"; // JustGuard受付開始Action名。
    std::string disableJustGuardActionName = "DisableJustGuard"; // JustGuard受付終了Action名。

    bool canGuardOwnerAttack = false; // 自分/同階層の攻撃をGuard対象にするか。
    bool oncePerAttackActivation = true; // 同じ攻撃判定へ1回だけGuardするか。
    int teamId = 0; // Team判定用ID。
    bool justGuardEnabled = false; // 現在JustGuard受付中か。
    float guardAngle = 360.0f; // 旧データは全方向を維持し、Playerは正面の許容角度を設定する。
    bool guardBodyContact = false; // 盾と重なる前に本体へ届いた攻撃も、防御方向を満たせば受け止める。
    bool reactToGuardBreak = false; // Playerの専用リアクションへGuardBreakを渡す。
    float guardBreakDamageRate = 1.0f; // GuardBreak時のHPダメージ倍率。旧来の通常被弾量を既定値にする。

    GameObject* guardOwner = nullptr;
    // HitBoxごとのActivation IDを覚え、同じ攻撃への重複Guardを防ぐ。
    std::unordered_map<const HitBox*, uint64_t> guardedAttackActivationIds;
};
