#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class DragonBodyPartHurtBoxComponent;
class HealthComponent;
struct AttackHitEvent;

enum class DRAGON_BODY_PART
{
    HEAD,
    NECK,
    CHEST,
    BODY,
    LEFT_FRONT_LEG,
    RIGHT_FRONT_LEG,
    LEFT_BACK_LEG,
    RIGHT_BACK_LEG,
    LEFT_WING,
    RIGHT_WING,
    TAIL
};

struct DamageContext
{
    uint64_t attackInstanceId = 0;
    std::weak_ptr<GameObject> attacker;
    std::weak_ptr<GameObject> targetCombatOwner;
    float healthDamage = 0.0f;
    float durabilityDamage = 0.0f;
    DirectX::XMFLOAT3 hitPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 hitDirection = { 0.0f, 0.0f, 1.0f };
};

struct DragonBodyPartStatus
{
    DRAGON_BODY_PART bodyPart = DRAGON_BODY_PART::BODY;
    float maxDurability = 100.0f;
    float currentDurability = 100.0f;
    float healthDamageRate = 1.0f;
    float durabilityDamageRate = 1.0f;
    float brokenHealthDamageRate = 1.25f;
    float baseMaxStagger = 45.0f;
    float currentMaxStagger = 45.0f;
    float currentStagger = 0.0f;
    float staggerDamageRate = 1.0f;
    float staggerRecoveryBaseMultiplier = 1.25f;
    std::string brokenActionName;
    bool broken = false;
};

struct BodyPartDamageResult
{
    DRAGON_BODY_PART bodyPart = DRAGON_BODY_PART::BODY;
    float appliedHealthDamage = 0.0f;
    float appliedDurabilityDamage = 0.0f;
    bool bodyPartBrokenThisHit = false;
    bool alreadyBroken = false;
};

const char* GetDragonBodyPartName(DRAGON_BODY_PART bodyPart_);
DRAGON_BODY_PART ParseDragonBodyPart(const std::string& bodyPartName_);

class DragonBodyPartManagerComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name_ = "DragonBodyPartManagerComponent";
        return name_;
    }

    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    BodyPartDamageResult ApplyDamage(
        DRAGON_BODY_PART bodyPart_,
        const DamageContext& damageContext_);
    void HandleAttackHit(
        const AttackHitEvent& event_,
        const DragonBodyPartHurtBoxComponent& hurtBoxComponent_);

    const DragonBodyPartStatus* GetBodyPartStatus(
        DRAGON_BODY_PART bodyPart_) const;
    bool IsBodyPartBroken(DRAGON_BODY_PART bodyPart_) const;
    void ResetBodyParts();

private:
    void EnsureDefaultBodyParts();
    DragonBodyPartStatus* FindBodyPartStatus(DRAGON_BODY_PART bodyPart_);
    void ResolveHealthComponent();
    void RegisterAttackHitCallback();
    void UpdateBossHpRate();
    void PublishLastDamageToBlackboard();
    void TryRequestDamageReaction(DragonBodyPartStatus& status_);
    void ExecuteBodyPartBrokenActions(const DragonBodyPartStatus& status_);

    std::vector<DragonBodyPartStatus> bodyParts;
    HealthComponent* health = nullptr;
    bool callbackRegistered = false;
    bool debugLogEnabled = true;
    bool reactionEnabled = true;
    float reactionCooldownTime = 2.0f;
    float reactionCooldownTimer = 0.0f;
    std::string reactionActionName = "DragonAI_DamageReaction";
    std::string commonBrokenActionName = "DragonBodyPart_Broken";
    BodyPartDamageResult lastDamageResult;
    std::string lastAttackerName;
    std::string lastHurtBoxName;
    uint64_t lastAttackInstanceId = 0;
};
