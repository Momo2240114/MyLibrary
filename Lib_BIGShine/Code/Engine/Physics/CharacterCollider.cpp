#include "CharacterCollider.h"

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

void CharacterCollider::Deserialize(const nlohmann::json& j_)
{
    CapsuleCollider::Deserialize(j_);
    EnforceCharacterSettings();
}

void CharacterCollider::EnforceCharacterSettings()
{
    collisionRole = CollisionRole::CHARACTER;
    usePhysicsResponse = true;
    physicsControlsTransform = true;
    isTrigger = false;
}
