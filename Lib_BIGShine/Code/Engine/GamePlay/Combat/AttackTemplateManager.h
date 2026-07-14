#pragma once

#include "AttackEffectHitComponent.h"

#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class GameObjectManager;

enum class ATTACK_TEMPLATE_OWNER_TARGET
{
    SELF,
    OWNER,
    WEAPON,
    TARGET,
    LOCK_ON_TARGET
};

enum class EFFECT_HIT_SHAPE
{
    NONE,
    BOX,
    CAPSULE,
    SPHERE_GROUP,
    FAN
};

enum class KNOCK_BACK_TYPE
{
    OWNER_FORWARD,
    EFFECT_FORWARD,
    FROM_OWNER_TO_TARGET,
    FROM_EFFECT_TO_TARGET
};

struct EffectHitBoxTemplate
{
    bool enabled = false;
    std::string hitBoxName = "EffectHitBox";
    EFFECT_HIT_SHAPE shape = EFFECT_HIT_SHAPE::BOX;
    float activeStartTime = 0.0f;
    float activeEndTime = 0.08f;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float radius = 0.5f;
    float halfHeight = 1.0f;
};

struct SlashEffectTemplate
{
    bool enabled = true;
    std::string vfxPath;
    float spawnTimeOffset = 0.0f;
    float lifeTime = 0.18f;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 1.0f, 1.2f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float alpha = 1.0f;
    bool followOwner = false;
    bool followNode = false;
    std::string baseNodeName;
    EffectHitBoxTemplate hitBox;
};

struct WeaponHitBoxTemplate
{
    bool enabled = false;
    std::string hitBoxName = "HitBox";
    std::string startNodeName = "WeaponRoot";
    std::string endNodeName = "WeaponTip";
    float radius = 0.18f;
    float activeStartTime = 0.0f;
    float activeEndTime = 0.18f;
};

struct AttackHitEffectTemplate
{
    bool spawnOnHit = true;
    std::string hitParticleAssetPath;
    std::string hitParticleEmitterName;
    float hitParticleLifeTime = 1.0f;
    float hitParticleNormalOffset = 0.0f;
    std::string hitMeshVfxAssetPath;
    float hitMeshVfxLifeTime = 1.0f;
    float hitMeshVfxNormalOffset = 0.0f;
};

struct AttackEffectSpawnTemplate
{
    bool enabled = true;
    ATTACK_EFFECT_TYPE effectType = ATTACK_EFFECT_TYPE::INSTANT_AREA;
    std::string particlePath;
    std::string meshVfxPath;
    float spawnTimeOffset = 0.0f;
    float lifeTime = 1.0f;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    bool followOwner = false;
    bool followNode = false;
    std::string baseNodeName;
    float speed = 0.0f;
    DirectX::XMFLOAT3 localDirection = { 0.0f, 0.0f, 1.0f };
    std::vector<AttackEffectHitShapeTemplate> hitShapes;
};

struct AttackTemplate
{
    std::string id;
    std::string displayName;
    ATTACK_TEMPLATE_OWNER_TARGET ownerTarget = ATTACK_TEMPLATE_OWNER_TARGET::SELF;
    float damage = 5.0f;
    float impactPower = 5.0f;
    float hitStopDuration = 0.05f;
    float hitStopScale = 0.0f;
    float knockBackPower = 3.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;
    KNOCK_BACK_TYPE knockBackType = KNOCK_BACK_TYPE::OWNER_FORWARD;
    uint32_t hitGroupId = 0u;
    bool allowMultiHit = false;
    bool canHitOwner = false;
    WeaponHitBoxTemplate weaponHitBox;
    AttackHitEffectTemplate hitEffect;
    std::vector<SlashEffectTemplate> slashEffects;
    std::vector<AttackEffectSpawnTemplate> attackEffects;
};

class AttackTemplateManager
{
public:
    static AttackTemplateManager& Instance();

    void LoadAll();
    bool LoadTemplate(const std::string& path_);
    bool SaveTemplate(const AttackTemplate& attackTemplate_);
    bool DuplicateTemplate(const AttackTemplate& source_, const std::string& newId_);
    AttackTemplate* FindTemplate(const std::string& id_);
    const AttackTemplate* FindTemplate(const std::string& id_) const;
    const std::vector<AttackTemplate>& GetTemplates() const { return templates; }
    std::string NormalizeAttackTemplatePath(const std::string& pathOrId_) const;
    void OpenEditor(const std::string& id_);
    void DrawEditorGui();
    void DrawInlineEditor(std::string& id_, GameObjectManager* gameObjectManager_);
    bool DrawEditorContent(const std::string& id_, GameObjectManager* gameObjectManager_);

    void OpenWindowEditor(const std::string& id_);

    void DrawInlineEditor(const std::string& id_);
    bool DrawEditorContent(const std::string& id_);

private:
    std::string ResolveRenamedTemplateId(const std::string& id_) const;

    std::vector<AttackTemplate> templates;
    std::string editingTemplateId;
    std::string duplicateTemplateId;
    std::string editingIdBuffer;
    std::string renamedTemplateId;
    std::unordered_map<std::string, std::string> renamedTemplateIds;
    bool editorOpen = false;
    bool loaded = false;
};
