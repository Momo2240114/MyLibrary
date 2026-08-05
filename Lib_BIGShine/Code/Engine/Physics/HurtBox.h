#pragma once

#include "CapsuleCollider.h"

#include <cstdint>
#include <unordered_map>

class HitBox;
class CharacterCollider;

// 攻撃を受けるためのCapsuleCollider。
// HitBoxからのDamage倍率、無敵時間、同一攻撃の重複Hit履歴を管理する。
class HurtBox : public CapsuleCollider
{
public:
    float damageMultiplier = 1.0f; // 部位別Damage倍率。
    std::string hurtTag = "Body"; // HitEventへ渡す受け側Tag。

    HurtBox();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    bool HasFixedPhysicsBehavior() const override { return true; }
    void Start() override;
    void Update(float deltaTime_) override;
    void SetInvincible(bool invincible_);
    bool IsInvincible() const { return invincible; }
    bool CanReceiveHit(const HitBox& hitBox_) const;
    void RecordHit(const HitBox& hitBox_);
    void ResetHitHistory();
    void Gui() override;
    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

private:
    // HurtBox用に固定したCollisionRoleとPhysics挙動を再適用する。
    void EnforceHurtBoxSettings();
    // ActionSequenceなどから呼ぶ無敵切り替えActionを登録する。
    void RegisterActions();
    void ResolveCharacterCollider();
    // CharacterColliderと同じ形状に追従する場合、Capsuleサイズを同期する。
    void SyncCharacterColliderShape();

private:
    float invincibilityDuration = 0.08f; // Hit後に自動で付く短い無敵時間。
    float invincibilityTimer = 0.0f; // 残り無敵時間。
    bool invincible = false; // 現在無敵かどうか。
    bool syncCharacterColliderShape = true; // CharacterColliderの形状へ同期するか。
    std::weak_ptr<CharacterCollider> characterCollider;
    // HitBoxごとのActivation IDを覚え、同じ攻撃判定の重複Hitを防ぐ。
    std::unordered_map<const HitBox*, uint64_t> receivedActivationIds;
};
