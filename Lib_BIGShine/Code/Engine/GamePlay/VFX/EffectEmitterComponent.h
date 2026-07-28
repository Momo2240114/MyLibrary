#pragma once

#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GameObject;

enum class EFFECT_EMITTER_ENTRY_TYPE
{
    PARTICLE,
    MESH_VFX,
    IMPACT_EFFECT
};

enum class EFFECT_EMITTER_SHAPE_TYPE
{
    AREA,
    PROJECTILE,
    BEAM,
    FAN
};

struct EffectEmitterEntryTemplate
{
    EFFECT_EMITTER_ENTRY_TYPE type = EFFECT_EMITTER_ENTRY_TYPE::PARTICLE;
    bool enabled = true;
    std::string assetPath;
    std::string emitterName;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
    float playbackSpeed = 1.0f;
    bool followOwner = false;
    bool overrideMeshVFXColorRamp = false;
    std::string colorRampTexturePath;
    bool forceAdditiveBlend = false;
    float glowMultiplier = 1.0f;
};

struct EffectEmitterHitTemplate
{
    bool enabled = true;
    float damage = 10.0f;
    float impactPower = 10.0f;
    float knockBackPower = 3.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    uint32_t hitGroupId = 0u;
    bool allowMultiHit = false;
    bool canHitOwner = false;
    bool destroyOnHit = false;
    std::string hitParticleAssetPath;
    std::string hitParticleEmitterName;
    float hitParticleLifeTime = 1.0f;
    float hitParticleNormalOffset = 0.0f;
    bool hitParticleUseImpactAxis = false;
    float hitParticleAxisToAttackerBlend = 0.0f;
    bool hitParticleUseTargetAttraction = false;
    float hitParticleTargetAttractionStrength = 0.0f;
    float hitParticleTargetAttractionRange = 0.0f;
    float hitParticleTargetAttractionFalloff = 1.0f;
    std::string hitMeshVFXAssetPath;
    float hitMeshVFXLifeTime = 1.0f;
    float hitMeshVFXNormalOffset = 0.0f;
    bool cameraShakeEnabled = true;
    float cameraShakeReceiveDuration = 0.12f;
    float cameraShakeReceivePositionStrength = 0.025f;
    float cameraShakeReceiveRotationStrength = 0.18f;
    float cameraShakeAttackScale = 0.5f;
    float cameraShakeFrequency = 24.0f;
    int cameraShakePriority = 5;
};

struct EffectEmitterTemplate
{
    std::string name = "EffectEmitter";
    std::string actionName = "EffectEmitter_Fire";
    EFFECT_EMITTER_SHAPE_TYPE shapeType = EFFECT_EMITTER_SHAPE_TYPE::PROJECTILE;
    std::string sourceNodeName;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float range = 8.0f;
    float radius = 0.5f;
    float speed = 8.0f;
    float lifeTime = 1.0f;
    float fanAngleDegrees = 45.0f;
    int fanSampleCount = 5;
    int fanRangeSteps = 3;
    bool spawnHit = true;
    std::vector<EffectEmitterEntryTemplate> entries;
    EffectEmitterHitTemplate hit;
};

class EffectEmitterComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override { return componentName; }
    void Start() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    void Emit(size_t templateIndex_);
    void EmitByName(const std::string& templateName_);

    std::vector<EffectEmitterTemplate>& GetTemplates() { return templates; }
    const std::vector<EffectEmitterTemplate>& GetTemplates() const { return templates; }

private:
    struct EmitFrame
    {
        DirectX::XMMATRIX sourceMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
    };

    EmitFrame BuildEmitFrame(const EffectEmitterTemplate& template_) const;
    DirectX::XMMATRIX BuildLocalMatrix(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& rotation_,
        const DirectX::XMFLOAT3& scale_) const;
    void SpawnEntry(
        const EffectEmitterTemplate& template_,
        const EffectEmitterEntryTemplate& entry_,
        const EmitFrame& frame_,
        const std::shared_ptr<GameObject>& followTarget_);
    std::shared_ptr<GameObject> SpawnHitObject(
        const EffectEmitterTemplate& template_,
        const EmitFrame& frame_);
    std::vector<AttackEffectHitShapeTemplate> BuildHitShapes(
        const EffectEmitterTemplate& template_) const;
    void ApplyMeshVFXOverrides(
        const std::shared_ptr<GameObject>& object_,
        const EffectEmitterEntryTemplate& entry_) const;

private:
    static inline const std::string componentName = "EffectEmitterComponent";
    std::vector<EffectEmitterTemplate> templates;
};
