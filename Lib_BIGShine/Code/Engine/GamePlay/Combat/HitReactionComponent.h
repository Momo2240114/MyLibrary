#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <algorithm>
#include <cstdint>
#include <array>
#include <string>
#include <vector>

enum class HIT_REACTION_KIND
{
    GROUND_LIGHT,
    GROUND_MEDIUM,
    GROUND_HEAVY,
    AIR_MEDIUM,
    AIR_HEAVY,
    COUNT
};

class BlackboardComponent;

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
        SetDefaultReactionActionName(actionName_);
    }
    void SetDamageActionName(const std::string& actionName_)
    {
        damageActionName = actionName_;
    }
    void SetDefaultReactionActionName(const std::string& actionName_);
    void SetReactionImpactThresholds(
        float mediumImpactThreshold_,
        float heavyImpactThreshold_,
        float airHeavyImpactThreshold_);
    void AddCommonReactionAction(const std::string& actionName_);
    void AddReactionAction(HIT_REACTION_KIND kind_, const std::string& actionName_);
    void SetArmor(float armor_)
    {
        armor = (std::max)(armor_, 0.0f);
    }
    float GetArmor() const { return armor; }

private:
    void RegisterActions();
    void CallOwnerAction(const std::string& actionName_);
    void CallOwnerActions(const std::vector<std::string>& actionNames_);
    HIT_REACTION_KIND DecideReactionKind(
        const BlackboardComponent& blackboard_,
        bool canReact_) const;
    std::vector<std::string>& GetReactionActions(HIT_REACTION_KIND kind_);
    const std::vector<std::string>& GetReactionActions(HIT_REACTION_KIND kind_) const;

private:
    float maxHealth = 100.0f;
    float currentHealth = 100.0f;
    float lastDamage = 0.0f;
    float lastImpactPower = 0.0f;
    int hitCount = 0;

    float armor = 0.0f;
    float mediumImpactThreshold = 20.0f;
    float heavyImpactThreshold = 50.0f;
    float airHeavyImpactThreshold = 50.0f;
    bool useHitStop = true;
    uint32_t hitStopLayerMask = 0;
    bool useKnockBack = true;

    std::string damageActionName;
    std::string reactionActionName;
    std::vector<std::string> commonReactionActionNames;
    std::array<std::vector<std::string>, static_cast<size_t>(HIT_REACTION_KIND::COUNT)> reactionActionNamesByKind;
    std::string overlayStartActionName = "MaterialOverlay_Start";
    std::string overlayStopActionName = "MaterialOverlay_Stop";
    float overlayDuration = 0.12f;
    float overlayTimer = 0.0f;

};
