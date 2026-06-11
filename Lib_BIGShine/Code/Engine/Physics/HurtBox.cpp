#include "HurtBox.h"

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

void HurtBox::Gui()
{
    CapsuleCollider::Gui();
    ImGui::SeparatorText("Damage Reception");
    ImGui::DragFloat("Damage Multiplier", &damageMultiplier, 0.05f, 0.0f, 10.0f);
    char hurtTagBuffer_[64];
    strncpy_s(hurtTagBuffer_, hurtTag.c_str(), sizeof(hurtTagBuffer_));
    if (ImGui::InputText("Hurt Tag", hurtTagBuffer_, sizeof(hurtTagBuffer_))) hurtTag = hurtTagBuffer_;
}

void HurtBox::Serialize(nlohmann::json& j_) const
{
    CapsuleCollider::Serialize(j_);
    j_["DamageMultiplier"] = damageMultiplier;
    j_["HurtTag"] = hurtTag;
}

void HurtBox::Deserialize(const nlohmann::json& j_)
{
    CapsuleCollider::Deserialize(j_);
    damageMultiplier = j_.value("DamageMultiplier", 1.0f);
    hurtTag = j_.value("HurtTag", std::string("Body"));
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
