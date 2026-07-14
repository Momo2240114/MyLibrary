#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class GameObject;
class HitBox;

enum class ATTACK_EFFECT_TYPE
{
    INSTANT_AREA,
    PROJECTILE,
    AREA_OVER_TIME,
    BEAM,
    BREATH
};

enum class ATTACK_HIT_SHAPE
{
    NONE,
    SPHERE,
    BOX,
    CAPSULE,
    SPHERE_GROUP,
    FAN,
    CONE,
    RAY
};

ATTACK_HIT_SHAPE NormalizeAttackHitShape(ATTACK_HIT_SHAPE shape_);

struct AttackEffectSphereTemplate
{
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    float radius = 0.5f;
};

struct AttackEffectHitShapeTemplate
{
    bool enabled = true;
    std::string hitBoxName = "AttackEffectHitBox";
    ATTACK_HIT_SHAPE shape = ATTACK_HIT_SHAPE::SPHERE;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float radius = 0.5f;
    float halfHeight = 1.0f;
    float range = 5.0f;
    float angle = 45.0f;
    float activeStartTime = 0.0f;
    float activeEndTime = 0.1f;
    float hitInterval = 0.0f;
    bool destroyOnHit = false;
    bool allowMultiHit = false;
    std::vector<AttackEffectSphereTemplate> spheres;
};

class AttackEffectHitComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    void SetOwner(GameObject* owner_);
    void SetAttackInstanceId(uint32_t attackInstanceId_);
    void SetHitGroupId(uint32_t hitGroupId_);
    void SetDamage(float damage_);
    void SetImpactPower(float impactPower_);
    void SetHitStopDuration(float hitStopDuration_);
    void SetHitStopScale(float hitStopScale_);
    void SetKnockBackPower(float knockBackPower_);
    void SetAirLiftPower(float targetAirLiftPower_, float attackerAirLiftPower_);
    void SetHitParticleAssetPath(const std::string& path_);
    void SetHitParticleEmitterName(const std::string& emitterName_);
    void SetHitParticleLifeTime(float lifeTime_);
    void SetHitParticleNormalOffset(float normalOffset_);
    void SetHitMeshVFXAssetPath(const std::string& path_);
    void SetHitMeshVFXLifeTime(float lifeTime_);
    void SetHitMeshVFXNormalOffset(float normalOffset_);
    void SetAllowMultiHit(bool allowMultiHit_);
    void SetCanHitOwner(bool canHitOwner_);
    void SetEffectType(ATTACK_EFFECT_TYPE effectType_);
    void SetLifeTime(float lifeTime_);
    void SetDestroyOnLifeEnd(bool destroyOnLifeEnd_);
    void SetMovement(const DirectX::XMFLOAT3& direction_, float speed_);
    void SetHitShapes(const std::vector<AttackEffectHitShapeTemplate>& hitShapes_);

    void Play();
    void Stop();
    void ResetRuntime();
    void SubmitDebugShapes();

private:
    struct RuntimeHitShape
    {
        std::shared_ptr<GameObject> object;
        std::shared_ptr<HitBox> hitBox;
        size_t templateIndex = 0u;
    };

    void RebuildHitShapes();
    void ConfigureHitBox(
        const std::shared_ptr<HitBox>& hitBox_,
        const AttackEffectHitShapeTemplate& shape_,
        const std::string& name_);
    void ClearHitHistory();

    ATTACK_EFFECT_TYPE effectType = ATTACK_EFFECT_TYPE::INSTANT_AREA;
    GameObject* ownerObject = nullptr;
    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    float damage = 1.0f;
    float impactPower = 1.0f;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    float knockBackPower = 0.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;
    std::string hitParticleAssetPath;
    std::string hitParticleEmitterName;
    float hitParticleLifeTime = 1.0f;
    float hitParticleNormalOffset = 0.0f;
    std::string hitMeshVFXAssetPath;
    float hitMeshVFXLifeTime = 1.0f;
    float hitMeshVFXNormalOffset = 0.0f;
    float lifeTime = 1.0f;
    float elapsedTime = 0.0f;
    float hitIntervalTimer = 0.0f;
    bool playing = false;
    bool destroyOnLifeEnd = true;
    bool canHitOwner = false;
    bool allowMultiHit = false;
    DirectX::XMFLOAT3 movementDirection = { 0.0f, 0.0f, 1.0f };
    float movementSpeed = 0.0f;
    std::vector<AttackEffectHitShapeTemplate> hitShapes;
    std::vector<RuntimeHitShape> runtimeHitShapes;
};
