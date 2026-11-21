#pragma once

#include "CapsuleCollider.h"

// PlayerやEnemyなど、歩行Character用のCapsuleCollider。
// PhysX CharacterController向けの段差、接地吸着、非地面滑り設定を持つ。
class CharacterCollider : public CapsuleCollider
{
public:
    CharacterCollider();

    const std::string& GetName() const override;

    bool HasFixedCollisionRole() const override
    {
        return true;
    }

    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& j_) override;

    float GetStepOffset() const
    {
        return stepOffset;
    }

    float GetContactOffset() const
    {
        return contactOffset;
    }

    float GetGroundSnapDistance() const
    {
        return groundSnapDistance;
    }

    bool ShouldLimitHorizontalLift() const
    {
        return limitHorizontalLift;
    }

    float GetMaxHorizontalLift() const
    {
        return maxHorizontalLift;
    }

    bool ShouldSlideOffNonGround() const
    {
        return slideOffNonGround;
    }

    float GetNonGroundSlideSpeed() const
    {
        return nonGroundSlideSpeed;
    }

    float GetNonGroundLift() const
    {
        return nonGroundLift;
    }

private:
    // Character用として固定したCollisionRoleとPhysics挙動を再適用する。
    void EnforceCharacterSettings();

    float stepOffset = 0.5f; // 乗り越え可能な段差高さ。
    float contactOffset = 0.02f; // CharacterControllerの接触余白。
    float groundSnapDistance = 0.9f; // 地面へ吸着する探索距離。
    bool limitHorizontalLift = true; // 横移動中の不自然な持ち上がりを抑えるか。
    float maxHorizontalLift = 0.05f; // 1Frameで許可する最大持ち上がり量。
    bool slideOffNonGround = true; // 地面扱いできない面から滑り落とすか。
    float nonGroundSlideSpeed = 3.0f; // 非地面上での滑り速度。
    float nonGroundLift = 0.16f; // 滑り処理時に少し浮かせる量。
};
