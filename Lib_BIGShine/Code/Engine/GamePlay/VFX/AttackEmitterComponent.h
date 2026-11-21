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

enum class ATTACK_EMITTER_ENTRY_TYPE
{
    // ParticleComponentを持つ一時GameObjectを生成する。
    PARTICLE,

    // MeshVFXComponentを持つ一時GameObjectを生成する。
    MESH_VFX,

    // ImpactEffectComponentなど、ヒット演出専用Componentを生成する。
    IMPACT_EFFECT
};

enum class ATTACK_EMITTER_SHAPE_TYPE
{
    // その場の範囲攻撃。
    AREA,

    // 前方へ飛ぶ攻撃。
    PROJECTILE,
    BEAM,
    FAN
};

struct AttackEmitterEntryTemplate
{
    // 生成する演出の種類。
    ATTACK_EMITTER_ENTRY_TYPE type = ATTACK_EMITTER_ENTRY_TYPE::PARTICLE;
    bool enabled = true;

    // Particle/MeshVFX/ImpactEffectのAssetパス。
    std::string assetPath;
    std::string emitterName;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };

    // 生成した演出GameObjectの寿命。
    float lifeTime = 1.0f;
    float normalOffset = 0.0f;
    float playbackSpeed = 1.0f;
    bool followOwner = false;

    // MeshVFXの色RampやBlendを、このEntryだけ上書きしたい時に使う。
    bool overrideMeshVFXColorRamp = false;
    std::string colorRampTexturePath;
    bool forceAdditiveBlend = false;
    float glowMultiplier = 1.0f;
};

struct AttackEmitterHitTemplate
{
    // trueなら演出と一緒にAttackEffectHitComponentも生成する。
    bool enabled = true;

    // HitBoxへ設定する攻撃パラメータ。
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
    std::string justGuardedActionName;

    // Hit成立時に追加で出す演出設定。
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
    // Hit位置の法線に合わせて再生するImpactEffect Asset。
    std::string hitImpactEffectAssetPath;
    bool cameraShakeEnabled = true;
    float cameraShakeReceiveDuration = 0.12f;
    float cameraShakeReceivePositionStrength = 0.025f;
    float cameraShakeReceiveRotationStrength = 0.18f;
    float cameraShakeAttackScale = 0.5f;
    float cameraShakeFrequency = 24.0f;
    int cameraShakePriority = 5;
};

struct AttackEmitterTemplate
{
    // GUI表示名と、ActionSequenceから呼ぶ時のAction名。
    std::string name = "AttackEmitter";
    std::string actionName = "AttackEmitter_Fire";
    ATTACK_EMITTER_SHAPE_TYPE shapeType = ATTACK_EMITTER_SHAPE_TYPE::PROJECTILE;

    // 指定Nodeがあれば、そのNode Transformを発生基準にする。
    std::string sourceNodeName;
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float range = 8.0f;
    float radius = 0.5f;
    float speed = 8.0f;
    float lifeTime = 1.0f;
    float fanAngleDegrees = 45.0f;
    bool projectileMotionEnabled = true;
    float projectileAcceleration = 0.0f;
    float projectileGravity = 0.0f;
    float projectileMaxSpeed = 0.0f;

    // FAN形状で複数方向・複数距離にサンプル判定を置くための設定。
    int fanSampleCount = 5;
    int fanRangeSteps = 3;
    // FAN形状の見た目Entryを何方向に複製するか。判定用サンプル数とは分けて負荷を抑える。
    int fanVisualSampleCount = 1;
    bool spawnHit = true;
    // 直線Particleを判定カプセルの軸と幅に合わせる。既存Templateは従来の演出を維持する。
    bool fitBeamVisualToHitShape = false;
    bool leaveFireGroundMark = false;

    // 同時に生成する見た目演出と、必要なら攻撃判定設定。
    std::vector<AttackEmitterEntryTemplate> entries;
    AttackEmitterHitTemplate hit;
};

//Effectを伴うbreathなどの攻撃の出現を管理するためのコンポーネント
class AttackEmitterComponent : public Component, public IActionProvider
{
public:
    const std::string& GetName() const override
    {
        return componentName;
    }

    void Start() override;
    // 発生元の破棄時に、所有する判定と独立した見た目のEffectも停止する。
    void OnDestroy() override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    void RegisterActions() override;

    // 指定IndexのTemplateを発火する。
    void Emit(size_t templateIndex_);

    // Template名から検索して発火する。
    void EmitByName(const std::string& templateName_);
    // 指定したワールド位置と方向を基準に、Template名から検索して発火する。
    void EmitByNameAt(
        const std::string& templateName_,
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& direction_);
    // 発射補正を含むBEAMの両端と半径を、予兆表示にも同じ形で渡す。
    bool ResolveBeamGeometryAt(
        const std::string& templateName_,
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& direction_,
        DirectX::XMFLOAT3& start_,
        DirectX::XMFLOAT3& end_,
        float& radius_) const;
    // AttackEmitter Asset JSONを読み込む。
    bool LoadAsset(const std::string& path_, bool forceReload_ = false);
    // 現在のTemplate設定をAttackEmitter Asset JSONとして保存する。
    bool SaveAsset(const std::string& path_) const;

    std::vector<AttackEmitterTemplate>& GetTemplates()
    {
        return templates;
    }

    const std::vector<AttackEmitterTemplate>& GetTemplates() const
    {
        return templates;
    }

private:
    struct EmitFrame
    {
        // 発生源NodeまたはOwner Transformから作った基準行列。
        DirectX::XMMATRIX sourceMatrix = DirectX::XMMatrixIdentity();

        // ワールド空間の発生位置と前方方向。
        DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
    };

    // TemplateのsourceNode/localOffsetから、今回の発生基準を作る。
    EmitFrame BuildEmitFrame(const AttackEmitterTemplate& template_) const;
    // 外部Action側で解決済みのワールド位置と方向から、今回の発生基準を作る。
    EmitFrame BuildEmitFrameAt(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& direction_) const;
    DirectX::XMMATRIX BuildLocalMatrix(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& rotation_,
        const DirectX::XMFLOAT3& scale_) const;
    void EmitWithFrame(
        const AttackEmitterTemplate& template_,
        const EmitFrame& frame_);
    void SpawnEntry(
        const AttackEmitterTemplate& template_,
        const AttackEmitterEntryTemplate& entry_,
        const EmitFrame& frame_,
        const std::shared_ptr<GameObject>& followTarget_);

    // spawnHitが有効なTemplateから、攻撃判定用GameObjectを生成する。
    std::shared_ptr<GameObject> SpawnHitObject(
        const AttackEmitterTemplate& template_,
        const EmitFrame& frame_);
    std::vector<AttackEffectHitShapeTemplate> BuildHitShapes(
        const AttackEmitterTemplate& template_) const;

    // Entry固有の色RampやBlend設定を生成済みMeshVFXへ反映する。
    void ApplyMeshVFXOverrides(
        const std::shared_ptr<GameObject>& object_,
        const AttackEmitterEntryTemplate& entry_) const;

private:
    static inline const std::string componentName = "AttackEmitterComponent";

    // 外部AttackEmitter Assetを使う場合のPath。
    std::string assetPath;
    // 同じAssetを毎回発射時に読み直さないための、実際に読み込み済みのPath。
    std::string loadedAssetPath;
    // Editorで保存する発生Template一覧。
    std::vector<AttackEmitterTemplate> templates;
};
