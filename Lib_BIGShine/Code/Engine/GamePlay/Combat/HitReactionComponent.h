#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <cstdint>
#include <string>

class HitReactionComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "HitReactionComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void HandleHit();
    void ResetHealth();
    void SetReactionActionName(const std::string& actionName_)
    {
        reactionActionName = actionName_;
    }

private:
    void RegisterActions();
    void CallOwnerAction(const std::string& actionName_);

private:
    float maxHealth = 100.0f;
    float currentHealth = 100.0f;
    float lastDamage = 0.0f;
    int hitCount = 0;

    bool useHitStop = true;
    uint32_t hitStopLayerMask = 0;
    bool useKnockBack = true;

    std::string reactionActionName;
    std::string overlayStartActionName = "MaterialOverlay_Start";
    std::string overlayStopActionName = "MaterialOverlay_Stop";
    float overlayDuration = 0.12f;
    float overlayTimer = 0.0f;

};
