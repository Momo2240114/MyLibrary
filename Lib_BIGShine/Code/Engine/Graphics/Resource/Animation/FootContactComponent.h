#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

class FootContactComponent : public Component
{
public:
    FootContactComponent();

    const std::string& GetName() const override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

private:
    struct Foot
    {
        std::string boneName;
        std::string particleEffect;
        float effectScale = 1.0f;
        bool enabled = true;

        DirectX::XMFLOAT3 previousPosition{};
        float velocityY = 0.0f;
        float lastImpactSpeed = 0.0f;
        float cooldownRemaining = 0.0f;
        unsigned effectCount = 0;
        bool hasSample = false;
        bool grounded = false;
        bool boneFound = false;
    };

    std::vector<Foot> feet;
    float contactDistance = 0.16f;
    float releaseDistance = 0.28f;
    float minimumImpactSpeed = 0.8f;
    float effectCooldown = 0.12f;
    bool contactEnabled = true;

    std::string previousAnimation;
    float previousAnimationTime = 0.0f;
    std::uint64_t previousSampleTime = 0;
};
