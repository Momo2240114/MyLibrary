#include "HitBox.h"

HitBox::HitBox()
{
    enabled = false;
    EnforceHitBoxSettings();
    debugPrimitiveColor = { 1.0f, 0.2f, 0.2f, 1.0f };
}

const std::string& HitBox::GetName() const
{
    static std::string name = "HitBox";
    return name;
}

void HitBox::Activate()
{
    currentTriggers.clear();
    SetEnabled(true);
}

void HitBox::Deactivate()
{
    SetEnabled(false);
    currentTriggers.clear();
}

void HitBox::Gui()
{
    BoxCollider::Gui();
    ImGui::SeparatorText("Attack");
    ImGui::DragFloat("Damage", &damage, 0.1f, 0.0f, 99999.0f);
    ImGui::Checkbox("Hit Once Per Activation", &hitOncePerActivation);
    char attackTagBuffer_[64];
    strncpy_s(attackTagBuffer_, attackTag.c_str(), sizeof(attackTagBuffer_));
    if (ImGui::InputText("Attack Tag", attackTagBuffer_, sizeof(attackTagBuffer_))) attackTag = attackTagBuffer_;
}

void HitBox::Serialize(nlohmann::json& j_) const
{
    BoxCollider::Serialize(j_);
    j_["Damage"] = damage;
    j_["HitOncePerActivation"] = hitOncePerActivation;
    j_["AttackTag"] = attackTag;
}

void HitBox::Deserialize(const nlohmann::json& j_)
{
    BoxCollider::Deserialize(j_);
    damage = j_.value("Damage", 1.0f);
    hitOncePerActivation = j_.value("HitOncePerActivation", true);
    attackTag = j_.value("AttackTag", std::string("Attack"));
    EnforceHitBoxSettings();
}

void HitBox::EnforceHitBoxSettings()
{
    collisionRole = CollisionRole::HIT_BOX;
    usePhysicsResponse = false;
    physicsControlsTransform = false;
    useGravity = false;
    isTrigger = false;
}
