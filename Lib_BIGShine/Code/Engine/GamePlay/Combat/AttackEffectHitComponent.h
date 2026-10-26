#pragma once

#include "CombatDamageTypes.h"
#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class GameObject;
class HitBox;

enum class ATTACK_EFFECT_TYPE
{
    // 生成した瞬間に範囲判定を置く攻撃。
    INSTANT_AREA,

    // 前方へ移動しながら判定を持つ攻撃。
    PROJECTILE,

    // 一定時間その場に残り、継続して判定する攻撃。
    AREA_OVER_TIME,

    // 直線状に伸びるビーム攻撃。
    BEAM,

    // 広がる息・ブレス系の攻撃。
    BREATH
};

enum class ATTACK_HIT_SHAPE
{
    // 判定なし。
    NONE,

    // 単一球判定。
    SPHERE,
    BOX,
    CAPSULE,
    SPHERE_GROUP,
    FAN,
    CONE,
    RAY
};

// 古いデータや未対応形状を、実行時に扱える形状へ丸める。
ATTACK_HIT_SHAPE NormalizeAttackHitShape(ATTACK_HIT_SHAPE shape_);

struct AttackEffectSphereTemplate
{
    // SPHERE_GROUP の各球を、エフェクト基準からどこへ置くか。
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    float radius = 0.5f;
};

struct AttackEffectHitShapeTemplate
{
    // falseならこの形状は生成しない。
    bool enabled = true;

    // Runtimeで生成するHitBox GameObject名。
    std::string hitBoxName = "AttackEffectHitBox";

    // どの形状で攻撃判定を作るか。
    ATTACK_HIT_SHAPE shape = ATTACK_HIT_SHAPE::SPHERE;

    // エフェクト本体を基準にした判定のローカルTransform。
    DirectX::XMFLOAT3 localOffset = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localRotation = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 localScale = { 1.0f, 1.0f, 1.0f };
    float radius = 0.5f;
    float halfHeight = 1.0f;
    float range = 5.0f;
    float angle = 45.0f;

    // エフェクト再生時間内で、この判定が有効になる区間。
    float activeStartTime = 0.0f;
    float activeEndTime = 0.1f;

    // 継続判定時、同じ対象へ再Hitできる間隔。
    float hitInterval = 0.0f;
    bool destroyOnHit = false;
    bool allowMultiHit = false;

    // SPHERE_GROUP用の複数球設定。
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

    // 攻撃者として扱うGameObjectを設定する。
    void SetOwner(GameObject* owner_);

    // AttackSystemの重複Hit管理に使う攻撃インスタンスID。
    void SetAttackInstanceId(uint32_t attackInstanceId_);
    void SetHitGroupId(uint32_t hitGroupId_);
    void SetDamage(float damage_);
    void SetDamageRate(float damageRate_);
    void SetFixedDamage(float fixedDamage_);
    void SetDamageApplyType(DAMAGE_APPLY_TYPE damageApplyType_);
    void SetDamageHitInterval(float damageHitInterval_);
    void SetMaxHitCount(int maxHitCount_);
    void SetImpactPower(float impactPower_);
    void SetHitStopDuration(float hitStopDuration_);
    void SetHitStopScale(float hitStopScale_);
    void SetKnockBackPower(float knockBackPower_);
    void SetAirLiftPower(float targetAirLiftPower_, float attackerAirLiftPower_);

    AttackFeedbackSettings feedback; // 生成する全HitBoxへ渡す攻撃の手応え。

    // Hit成立時にCameraShakeへ渡す揺れ設定。
    void SetCameraShake(
        bool enabled_,
        float receiveDuration_,
        float receivePositionStrength_,
        float receiveRotationStrength_,
        float attackScale_,
        float frequency_,
        int priority_);
    void SetHitParticleAssetPath(const std::string& path_);
    void SetHitParticleEmitterName(const std::string& emitterName_);
    void SetHitParticleLifeTime(float lifeTime_);
    void SetHitParticleNormalOffset(float normalOffset_);
    void SetHitParticleImpactAxis(bool enabled_, float axisToAttackerBlend_);
    void SetHitParticleTargetAttraction(
        bool enabled_,
        float strength_,
        float range_,
        float falloff_);
    void SetHitMeshVFXAssetPath(const std::string& path_);
    void SetHitMeshVFXLifeTime(float lifeTime_);
    void SetHitMeshVFXNormalOffset(float normalOffset_);
    void SetHitImpactEffectAssetPath(const std::string& path_);
    void SetAllowMultiHit(bool allowMultiHit_);
    void SetCanHitOwner(bool canHitOwner_);
    // 派生エフェクトのHitBoxにも、元の攻撃のガード属性を引き継ぐ。
    void SetGuardProperties(bool canBeGuarded_, bool guardBreak_);
    void SetSourceAction(const std::string& sourceAction_);
    void SetJustGuardedActionName(const std::string& actionName_);
    void SetRuntimeHitOwner(GameObject* owner_);
    void SetRuntimeHitTeamId(int teamId_);

    // 攻撃タイプによって移動や寿命の扱いを変える。
    void SetEffectType(ATTACK_EFFECT_TYPE effectType_);
    void SetLifeTime(float lifeTime_);
    void SetDestroyOnLifeEnd(bool destroyOnLifeEnd_);
    void SetMovement(const DirectX::XMFLOAT3& direction_, float speed_);

    // 実行時に生成するHitBox形状リストを差し替える。
    void SetHitShapes(const std::vector<AttackEffectHitShapeTemplate>& hitShapes_);
    // 命中履歴を作り直さず、カプセルを指定したWorld線分へ追従させる。
    bool SetCapsuleWorldSpan(std::size_t shapeIndex_, const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_);

    // 判定再生を開始する。
    void Play();
    void Stop();
    // 反射で所有者が変わった攻撃を、元の攻撃者の中断対象から除外する。
    bool IsOwnedBy(const GameObject* owner_) const { return ownerObject == owner_; }
    void ResetRuntime();

    // Editor/Debug描画用に、現在のHitShape情報を提出する。
    void SubmitDebugShapes();

private:
    struct RuntimeHitShape
    {
        // 実際に生成した判定用GameObjectとHitBox。
        std::shared_ptr<GameObject> object;
        std::shared_ptr<HitBox> hitBox;

        // 元になったhitShapesのIndex。時間制御やGUI表示で対応を取る。
        size_t templateIndex = 0u;
    };

    // hitShapesテンプレートをもとにRuntimeHitShapeを作り直す。
    void RebuildHitShapes();

    // AttackSystemへ渡すDamage/VFX/CameraShakeなどをHitBoxへ設定する。
    void ConfigureHitBox(
        const std::shared_ptr<HitBox>& hitBox_,
            const AttackEffectHitShapeTemplate& shape_,
            const std::string& name_);

    // 継続攻撃や再生し直し時に、HitBox側の重複Hit履歴を消す。
    void ClearHitHistory();

    // 実行中の攻撃タイプと攻撃者情報。
    ATTACK_EFFECT_TYPE effectType = ATTACK_EFFECT_TYPE::INSTANT_AREA;
    GameObject* ownerObject = nullptr;
    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;

    // Hit成立時にAttackSystemへ渡す攻撃パラメータ。
    float damage = 1.0f;
    float damageRate = 0.01f;
    float fixedDamage = 0.0f;
    DAMAGE_APPLY_TYPE damageApplyType = DAMAGE_APPLY_TYPE::SINGLE_HIT;
    float damageHitInterval = 0.0f;
    int maxHitCount = 1;
    float impactPower = 1.0f;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    float knockBackPower = 0.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;

    // Hit成立時のカメラ揺れ。
    bool cameraShakeEnabled = true;
    float cameraShakeReceiveDuration = 0.12f;
    float cameraShakeReceivePositionStrength = 0.025f;
    float cameraShakeReceiveRotationStrength = 0.18f;
    float cameraShakeAttackScale = 0.5f;
    float cameraShakeFrequency = 24.0f;
    int cameraShakePriority = 5;

    // Hit成立時に追加で出すParticle/MeshVFX設定。
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
    std::string hitImpactEffectAssetPath;

    // この攻撃エフェクト自身の寿命と再生状態。
    float lifeTime = 1.0f;
    float elapsedTime = 0.0f;
    float hitIntervalTimer = 0.0f;
    bool playing = false;
    bool destroyOnLifeEnd = true;
    bool canHitOwner = false;
    bool canBeGuarded = true;
    bool guardBreak = false;
    bool allowMultiHit = false;
    std::string sourceAction;
    std::string justGuardedActionName;

    // PROJECTILEなどでエフェクト本体を移動させるための設定。
    DirectX::XMFLOAT3 movementDirection = { 0.0f, 0.0f, 1.0f };
    float movementSpeed = 0.0f;
    std::vector<AttackEffectHitShapeTemplate> hitShapes;

    // 再生時に生成された実体判定。
    std::vector<RuntimeHitShape> runtimeHitShapes;
};
