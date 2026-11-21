#pragma once

#include "AttackEffectHitComponent.h"
#include "CombatDamageTypes.h"
#include "AttackTelegraphSettings.h"

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

enum class CAMERA_SHAKE_POWER_LEVEL
{
    MINIMUM,
    MEDIUM,
    HEAVY
};

// 生成された攻撃エフェクト側に持たせる当たり判定形状。
// 武器HitBoxとは別に、飛び道具や範囲攻撃が自前で判定を持つためのテンプレート。
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

// 攻撃開始時などに出す斬撃VFXの設定。
// 見た目だけでなく、必要ならEffectHitBoxTemplateで判定も同時に持たせられる。
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

// 武器や手足など、既存GameObject上のHitBoxを攻撃時間に合わせて有効化する設定。
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

// ヒットした瞬間に再生するParticle、MeshVFX、法線方向オフセットなどの演出設定。
struct AttackHitEffectTemplate
{
    bool spawnOnHit = true;
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
    std::string hitMeshVfxAssetPath;
    float hitMeshVfxLifeTime = 1.0f;
    float hitMeshVfxNormalOffset = 0.0f;
};

struct AttackCameraShakeTemplate
{
    bool enabled = true;
    CAMERA_SHAKE_POWER_LEVEL powerLevel = CAMERA_SHAKE_POWER_LEVEL::MINIMUM;
    float receiveDuration = 0.12f;
    float receivePositionStrength = 0.025f;
    float receiveRotationStrength = 0.18f;
    float attackScale = 0.5f;
    float frequency = 24.0f;
    int priority = 5;
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

// 1つの攻撃テンプレート本体。
// ダメージ、HitStop、ノックバック、VFX、派生攻撃エフェクトをJSON保存する単位。
struct AttackTemplate
{
    std::string id;
    std::string displayName;
    ATTACK_TEMPLATE_OWNER_TARGET ownerTarget = ATTACK_TEMPLATE_OWNER_TARGET::SELF;
    float damage = 5.0f;
    float damageRate = 0.05f;
    float fixedDamage = 0.0f;
    DAMAGE_APPLY_TYPE damageApplyType = DAMAGE_APPLY_TYPE::SINGLE_HIT;
    float damageHitInterval = 0.0f;
    int maxHitCount = 1;
    float impactPower = 5.0f;
    float hitStopDuration = 0.05f;
    float hitStopScale = 0.0f;
    float knockBackPower = 3.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;
    KNOCK_BACK_TYPE knockBackType = KNOCK_BACK_TYPE::OWNER_FORWARD;
    uint32_t hitGroupId = 0u;
    bool allowMultiHit = false;
    bool guardBreak = false; // 通常Guardを崩す。旧Templateはfalseで互換性を維持する。
    bool canBeGuarded = true; // 旧Templateは通常通りガード可能な攻撃として復元する。
    bool canHitOwner = false;
    WeaponHitBoxTemplate weaponHitBox;
    // 武器判定の有効開始時刻から予兆を導出し、独立した攻撃タイマーは持たない。
    AttackTelegraphSettings telegraph;
    AttackHitEffectTemplate hitEffect;
    AttackFeedbackSettings feedback; // 武器と派生エフェクトで同じ振動・音声設定を使用する。
    AttackCameraShakeTemplate cameraShake;
    std::vector<SlashEffectTemplate> slashEffects;
    std::vector<AttackEffectSpawnTemplate> attackEffects;
};

// Data/AttackTemplate以下のJSONを読み書きし、EditorとRuntimeの両方へ攻撃データを渡す管理クラス。
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

    const std::vector<AttackTemplate>& GetTemplates() const
    {
        return templates;
    }

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
