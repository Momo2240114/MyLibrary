#include "CharacterCollider.h"

#include <algorithm>

CharacterCollider::CharacterCollider()
{
    EnforceCharacterSettings();
    mass = 1.0f;
    debugPrimitiveColor = { 0.2f, 0.6f, 1.0f, 1.0f };
}

const std::string& CharacterCollider::GetName() const
{
    static std::string name = "CharacterCollider";
    return name;
}

void CharacterCollider::Gui()
{
    CapsuleCollider::Gui();

    ImGui::SeparatorText("Character Grounding");
    bool recreateController_ = false;
    recreateController_ |= ImGui::DragFloat("Step Offset", &stepOffset, 0.01f, 0.0f, 5.0f);
    recreateController_ |= ImGui::DragFloat("Contact Offset", &contactOffset, 0.001f, 0.001f, 1.0f);
    ImGui::DragFloat("Ground Snap Distance", &groundSnapDistance, 0.01f, 0.0f, 5.0f);

    stepOffset = (std::max)(stepOffset, 0.0f);
    contactOffset = (std::max)(contactOffset, 0.001f);
    groundSnapDistance = (std::max)(groundSnapDistance, 0.0f);
    if (recreateController_)
    {
        RecreatePhysXActor();
    }
}

void CharacterCollider::Serialize(nlohmann::json& json_) const
{
    CapsuleCollider::Serialize(json_);
    json_["StepOffset"] = stepOffset;
    json_["ContactOffset"] = contactOffset;
    json_["GroundSnapDistance"] = groundSnapDistance;
}

void CharacterCollider::Deserialize(const nlohmann::json& j_)
{
    CapsuleCollider::Deserialize(j_);
    stepOffset = (std::max)(j_.value("StepOffset", 0.5f), 0.0f);
    contactOffset = (std::max)(j_.value("ContactOffset", 0.02f), 0.001f);
    groundSnapDistance = (std::max)(j_.value("GroundSnapDistance", 0.9f), 0.0f);
    EnforceCharacterSettings();
}

void CharacterCollider::EnforceCharacterSettings()
{
    collisionRole = CollisionRole::CHARACTER;
    usePhysicsResponse = true;
    physicsControlsTransform = true;
    isTrigger = false;
}
