#pragma once

#include "CapsuleCollider.h"

class CharacterCollider : public CapsuleCollider
{
public:
    CharacterCollider();

    const std::string& GetName() const override;
    bool HasFixedCollisionRole() const override { return true; }
    void Deserialize(const nlohmann::json& j_) override;

private:
    void EnforceCharacterSettings();
};
