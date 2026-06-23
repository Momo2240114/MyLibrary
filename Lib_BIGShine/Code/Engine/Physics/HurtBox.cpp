#include "HurtBox.h"

#include "HitBox.h"

#include <algorithm>

HurtBox::HurtBox()
{
    EnforceHurtBoxSettings();
    debugPrimitiveColor = { 0.2f, 1.0f, 0.3f, 1.0f };
}

const std::string& HurtBox::GetName() const
{
    static std::string name = "HurtBox";
    return name;
}

void HurtBox::Update(float deltaTime_)
{
    CapsuleCollider::Update(deltaTime_);
    invincibilityTimer = (std::max)(
        invincibilityTimer - (std::max)(deltaTime_, 0.0f),
        0.0f);
}

bool HurtBox::CanReceiveHit(const HitBox& hitBox_) const
{
    const auto receivedIterator_ = receivedActivationIds.find(&hitBox_);
    if (hitBox_.hitOncePerActivation &&
        receivedIterator_ != receivedActivationIds.end() &&
        receivedIterator_->second == hitBox_.GetActivationId())
    {
        return false;
    }
    return invincibilityTimer <= 0.0f;
}

void HurtBox::RecordHit(const HitBox& hitBox_)
{
    receivedActivationIds[&hitBox_] = hitBox_.GetActivationId();
    invincibilityTimer = invincibilityDuration;
}

void HurtBox::ResetHitHistory()
{
    receivedActivationIds.clear();
    invincibilityTimer = 0.0f;
}

void HurtBox::Gui()
{
    CapsuleCollider::Gui();
    ImGui::SeparatorText("Damage Reception");
    ImGui::DragFloat("Damage Multiplier", &damageMultiplier, 0.05f, 0.0f, 10.0f);
    ImGui::DragFloat(
        "Invincibility Duration",
        &invincibilityDuration,
        0.01f,
        0.0f,
        10.0f,
        "%.3f sec");
    ImGui::ProgressBar(
        invincibilityDuration > 0.0f
            ? invincibilityTimer / invincibilityDuration
            : 0.0f,
        ImVec2(-1.0f, 0.0f),
        invincibilityTimer > 0.0f ? "INVINCIBLE" : "READY");
    if (ImGui::Button("Reset Hit History")) ResetHitHistory();
    char hurtTagBuffer_[64];
    strncpy_s(hurtTagBuffer_, hurtTag.c_str(), sizeof(hurtTagBuffer_));
    if (ImGui::InputText("Hurt Tag", hurtTagBuffer_, sizeof(hurtTagBuffer_))) hurtTag = hurtTagBuffer_;
}

void HurtBox::Serialize(nlohmann::json& j_) const
{
    CapsuleCollider::Serialize(j_);
    j_["DamageMultiplier"] = damageMultiplier;
    j_["HurtTag"] = hurtTag;
    j_["InvincibilityDuration"] = invincibilityDuration;
}

void HurtBox::Deserialize(const nlohmann::json& j_)
{
    CapsuleCollider::Deserialize(j_);
    damageMultiplier = j_.value("DamageMultiplier", 1.0f);
    hurtTag = j_.value("HurtTag", std::string("Body"));
    invincibilityDuration = (std::max)(
        j_.value("InvincibilityDuration", 0.08f),
        0.0f);
    ResetHitHistory();
    EnforceHurtBoxSettings();
}

void HurtBox::EnforceHurtBoxSettings()
{
    collisionRole = CollisionRole::HURT_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = false;
}
