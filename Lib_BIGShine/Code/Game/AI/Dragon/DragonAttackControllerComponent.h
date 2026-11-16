#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/RenderSystem/RadialBlurSystem.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AttackEffectHitComponent;
class DragonNeckCommandIKComponent;
class DragonTailCommandIKComponent;
class AttackEmitterComponent;
class GameObject;
class MeshVFXComponent;
class ColliderComponent;
class HurtBox;
class CameraActionDirectorComponent;

// Dragonのブレスや尻尾攻撃の判定・VFX発生を、首/尻尾IKの位置に合わせて制御するComponent。
class DragonAttackControllerComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    // 本体の破棄時にも独立した攻撃Objectと予告範囲を終了する。
    void OnDestroy() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // ブレス用Emitterまたは口元から、前方へ伸びるブレス攻撃を開始する。
    void PlayBreath();
    // ブレス用Emitterまたは口元から単発ブレスProjectileを発生させる。
    void PlayBreathProjectileSingle();
    // ブレス用Emitterまたは口元から直線ブレスProjectileを発生させる。
    void PlayBreathProjectileBeam();
    // ブレス用Emitterまたは口元から扇状ブレスProjectileを発生させる。
    void PlayBreathProjectileFan();
    // 口元周辺に短時間の噛みつき判定とVFXを発生させる。
    void PlayBite();
    // 尻尾IKの攻撃有効時間に合わせて通常の薙ぎ払い判定を開始する。
    void PlayTailSweep();
    void PlaySpinSweep(float rotationDuration_ = 0.0f);
    void StopSpinSweep();
    // 着地や踏みつけの瞬間に、足元周辺へ範囲攻撃と岩VFXを発生させる。
    void PlayStamp();
    void PlaySpecialImpact(const DirectX::XMFLOAT3& position_, float radius_, float damage_, const std::string& effectPath_);
    bool BeginSpecialAction(bool dive_);
    bool IsSpecialActionActive() const { return specialPhase >= 0; }
    bool IsSpecialDive() const { return specialDive; }
    int GetSpecialPhase() const { return specialPhase; }
    void StopSpecialAction();
    void BeginSpecialCameraReturn();
    // 現在このComponentが生成した攻撃判定を停止する。
    void StopAttack();
    // ブレス攻撃の危険範囲だけを事前表示する。
    void ShowBreathDangerArea();
    // 単発ブレスProjectileの着弾予定地点だけを事前表示する。
    void ShowBreathProjectileSingleDangerArea();
    // 直線ブレスProjectileの軌道上だけを事前表示する。
    void ShowBreathProjectileBeamDangerArea();
    // 扇状ブレスProjectileの広がりだけを事前表示する。
    void ShowBreathProjectileFanDangerArea();
    // 噛みつき攻撃の危険範囲だけを事前表示する。
    void ShowBiteDangerArea();
    // 尻尾薙ぎ払い攻撃の危険範囲だけを事前表示する。
    void ShowTailSweepDangerArea();
    // 回転薙ぎ払い攻撃の危険範囲だけを事前表示する。
    void ShowSpinSweepDangerArea();
    // スタンプ攻撃の危険範囲だけを事前表示する。
    void ShowStampDangerArea();

private:
    void UpdateSpecialAction(float deltaTime_);
    bool LoadSpecialSettings();
    bool ResolveSpecialGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const;
    void ReleaseSpecialIntro();
    void PlaceSpecialDragon(const DirectX::XMFLOAT3& position_);
    void SpawnSpecialImpact(const DirectX::XMFLOAT3& position_, float radius_, bool final_, bool applyDamage_ = true);
    bool IsSpecialTargetSheltered() const;
    void DrawSpecialGui();

    struct SpecialMeteor
    {
        DirectX::XMFLOAT3 ground{};
        std::shared_ptr<GameObject> body;
        std::shared_ptr<GameObject> trail;
        std::shared_ptr<GameObject> warning;
        std::shared_ptr<GameObject> obstacle;
        float radius = 1.0f;
        float coverRadius = 1.0f;
        float height = 1.0f;
        float warningRemaining = 0.0f;
        bool landed = false;
        bool final = false;
    };
    int specialPhase = -1;
    bool specialDive = false;
    float specialTimer = 0.0f;
    int specialWave = 0;
    DirectX::XMFLOAT3 specialStart{};
    DirectX::XMFLOAT3 specialCenter{};
    DirectX::XMFLOAT3 specialTarget{};
    std::shared_ptr<GameObject> specialDanger;
    std::vector<SpecialMeteor> specialMeteors;
    std::vector<std::shared_ptr<GameObject>> specialAura;
    std::vector<std::weak_ptr<GameObject>> specialDecals;
    std::vector<std::pair<std::weak_ptr<Component>, bool>> specialPlayerSuppression;
    std::vector<std::pair<std::weak_ptr<HurtBox>, bool>> specialPlayerInvincibility;
    std::weak_ptr<CameraActionDirectorComponent> specialCamera;
    std::weak_ptr<ColliderComponent> specialCollider;
    DirectX::XMFLOAT3 specialRotation{};
    DirectX::XMFLOAT3 specialWorldScale{1.0f,1.0f,1.0f};
    DirectX::XMFLOAT3 specialAirPosition{};
    float specialGroundOffset = 0.0f;
    float specialAngle = 0.0f;
    float specialElapsed = 0.0f;
    float specialAppliedBlurRadius = 0.0f;
    bool specialMovementSuppressed = false;
    bool specialAnimationControlled = false;
    bool specialGravity = false;
    bool specialPhysicsControlsTransform = false;
    bool specialIntroLocked = false;
    bool specialPreviousPlayerLock = false;
    bool specialCameraReturning = false;
    bool specialCompleted = false;
    unsigned int specialSequenceGeneration = 0;
    int specialGameState = -1;
    RadialBlurSystem::Parameters specialPreviousBlur;
    nlohmann::json specialSettings;
    std::string specialStatus;
    // 無効化・撃破・破棄済みのドラゴンから予告や攻撃を再生成しない。
    bool CanSpawnAttack() const;

    struct RuntimeAttack
    {
        std::shared_ptr<GameObject> object;
        std::shared_ptr<GameObject> dangerAreaObject;
        std::weak_ptr<AttackEffectHitComponent> hitComponent;
        std::weak_ptr<MeshVFXComponent> dangerAreaVFX;
        // 回転攻撃の各節が残す軌跡。攻撃の中断・終了時にまとめて破棄する。
        std::vector<std::shared_ptr<GameObject>> spinTailTrailObjects;
        DirectX::XMFLOAT3 sourceLocalOffset = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 dangerAreaLocalOffset = { 0.0f, 0.0f, 0.0f };
        float dangerAreaRadius = 1.0f;
        float dangerAreaHalfHeight = 0.0f;
        bool dangerAreaIsFan = false;
        // 直線ブレスは予兆で方向を確定し、発射後の表示位置を保持する。
        bool dangerAreaFixedFrame = false;
        DirectX::XMFLOAT3 dangerAreaFixedPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 dangerAreaFixedForward = { 0.0f, 0.0f, 1.0f };
        float dangerAreaFanAngleDegrees = 0.0f;
        // 地面クエリの結果を短時間保持し、停止中の予兆で毎フレームRayを飛ばさない。
        bool dangerAreaGroundCached = false;
        bool dangerAreaGroundResolved = false;
        float dangerAreaGroundQueryTimer = 0.0f;
        float dangerAreaGroundHeight = 0.0f;
        DirectX::XMFLOAT3 dangerAreaGroundSamplePosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 dangerAreaGroundForward = { 0.0f, 0.0f, 1.0f };
        DirectX::XMFLOAT3 dangerAreaGroundNormal = { 0.0f, 1.0f, 0.0f };
        float lifeTimer = 0.0f;
        bool followMouth = false;
        bool followBreathSource = false;
        bool followStampSource = false;
        bool followTail = false;
        bool spinSweep = false;
        bool spinSweepUsesAIRotation = false;
        bool spinSweepHitActive = false;
        bool followSpinDanger = false;
        bool requireTailAttackActive = false;
        bool hasDangerArea = false;
    };

    struct PendingStampRock
    {
        // スタンプ後に時間差で発生させる岩VFXの予約情報。
        DirectX::XMFLOAT3 centerPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f };
        float distance = 0.0f;
        float scale = 1.0f;
        float verticalOffset = 0.0f;
        float timer = 0.0f;
    };

    // ActionSequenceやDragonAIから呼べる攻撃ActionをOwnerへ登録する。
    void RegisterActions();
    void UnregisterActions();
    void EnsureDependencies();
    void UpdateRuntimeAttacks(float deltaTime_);
    void UpdateBreathVFX(float deltaTime_);
    void UpdatePendingStampRocks(float deltaTime_);
    std::shared_ptr<AttackEffectHitComponent> SpawnAttackHit(
        const std::string& objectName_,
        float lifeTime_,
        bool followMouth_,
        bool followTail_,
        bool requireTailAttackActive_,
        float damage_,
        float damageRate_,
        float fixedDamage_,
        float impactPower_,
        float knockBackPower_,
        float radius_,
        float halfHeight_,
        const DirectX::XMFLOAT3& localOffset_,
        float hitInterval_, bool spinSweep_ = false);
    void ApplyFollowTransform(RuntimeAttack& runtime_);
    void ApplyDangerAreaTransform(RuntimeAttack& runtime_);
    // 地面の法線と範囲内の接地点から、危険範囲を埋めない表示高さを求める。
    bool ResolveDangerAreaGroundHeight(
        RuntimeAttack& runtime_,
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& forward_,
        float rayReferenceHeight_,
        float& height_,
        DirectX::XMFLOAT3& normal_);
    // 危険範囲の前方向を維持して、地面法線に沿う回転を求める。
    DirectX::XMFLOAT3 ResolveDangerAreaGroundRotation(
        const DirectX::XMFLOAT3& forward_,
        const DirectX::XMFLOAT3& normal_) const;
    void SpawnBreathVFX();
    void SpawnBreathChargeEffect();
    bool ResolveActualMouthFrame(
        DirectX::XMFLOAT3& position_,
        DirectX::XMFLOAT3& forward_) const;
    bool ResolveMouthFrame(
        DirectX::XMFLOAT3& position_,
        DirectX::XMFLOAT3& forward_) const;
    bool ResolveBreathSourceFrame(
        DirectX::XMFLOAT3& position_,
        DirectX::XMFLOAT3& forward_,
        float targetAimBlendOverride_ = -1.0f) const;
    bool ResolveBreathBaseForward(
        DirectX::XMFLOAT3& forward_) const;
    bool ResolveBreathAimForward(
        const DirectX::XMFLOAT3& sourcePosition_,
        const DirectX::XMFLOAT3& fallbackForward_,
        DirectX::XMFLOAT3& forward_,
        float targetAimBlendOverride_ = -1.0f) const;
    bool ResolveNamedObjectFrame(
        const std::string& objectName_,
        DirectX::XMFLOAT3& position_,
        DirectX::XMFLOAT3& forward_) const;
    bool ResolveStampSourceFrame(
        DirectX::XMFLOAT3& position_,
        DirectX::XMFLOAT3& forward_) const;
    std::shared_ptr<AttackEmitterComponent> ResolveBreathAttackEmitterComponent(
        DirectX::XMFLOAT3& position_,
        DirectX::XMFLOAT3& forward_,
        float targetAimBlendOverride_ = -1.0f) const;
    void SpawnBreathProjectile(
        const std::string& templateName_,
        float targetAimBlendOverride_ = -1.0f);
    void SpawnBiteVFX();
    void SpawnStampHit();
    float ResolveSpinSweepRadius() const;
    void UpdateSpinTailTrails(RuntimeAttack& runtime_, bool emitting_);
    void SpawnStampRockVFX(const DirectX::XMFLOAT3& centerPosition_);
    void SpawnStampRockVFXAt(
        const PendingStampRock& pendingRock_);

    void SpawnDangerArea(
        const std::string& objectName_,
        float lifeTime_,
        bool followMouth_,
        bool followBreathSource_,
        bool followTail_,
        const DirectX::XMFLOAT3& localOffset_,
        float radius_,
        float halfHeight_);
    void SpawnFanDangerArea(
        const std::string& objectName_,
        float lifeTime_,
        bool followBreathSource_,
        float range_,
        float angleDegrees_);
    void AttachDangerArea(
        RuntimeAttack& runtime_,
        const std::string& objectName_,
        const DirectX::XMFLOAT3& localOffset_,
        float radius_,
        float halfHeight_,
        bool useFanShape_ = false,
        float fanAngleDegrees_ = 0.0f);
    void ConfigureDangerAreaVFX(
        const std::shared_ptr<MeshVFXComponent>& meshVFX_,
        float radius_,
        float halfHeight_,
        bool useFanShape_ = false,
        float fanAngleDegrees_ = 0.0f) const;
    void StopRuntimeAttack(RuntimeAttack& runtime_);
    DirectX::XMFLOAT3 ResolveForwardRotation(const DirectX::XMFLOAT3& forward_) const;
    uint32_t CreateAttackInstanceId();

private:
    std::weak_ptr<DragonNeckCommandIKComponent> neckIK;
    std::weak_ptr<DragonTailCommandIKComponent> tailIK;
    std::weak_ptr<AttackEmitterComponent> attackEmitter;
    std::string registeredActionCategory;
    std::vector<std::string> registeredActionNames;
    std::vector<RuntimeAttack> runtimeAttacks;
    std::vector<PendingStampRock> pendingStampRocks;
    // 予兆で確定した直線ブレスの発射基準。予兆の寿命を超えた予約は使わない。
    bool pendingBeamFrameValid = false;
    float pendingBeamFrameTimer = 0.0f;
    DirectX::XMFLOAT3 pendingBeamPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 pendingBeamForward = { 0.0f, 0.0f, 1.0f };
    std::weak_ptr<AttackEmitterComponent> pendingBeamEmitter;

    std::string actionCategory = "DragonAttack";
    std::string breathVFXAssetPath = "Data/VFX/DragonBreath.json";
    std::string breathProjectileEmitterAssetPath = "Data/AttackEmitter/DragonBreathLight.json";
    std::string breathChargeEffectTemplateName = "DragonBreathCharge";
    // ブレスの俯角補正で狙うTarget Object名。
    std::string targetObjectName = "PlayerModel";
    // 空でなければ、このObjectに付いたAttackEmitterComponentをブレス発射元として優先する。
    std::string breathEmitterObjectName;
    // ブレス用Emitter Objectが無い場合に、発射位置と危険範囲追従へ使うモデルノード名。
    std::string breathSourceNodeName = "Mouth";
    std::string biteVFXAssetPath = "Data/VFX/DragonBite.json";
    std::string dangerAreaVFXAssetPath = "Data/VFX/EnemyDangerArea.json";

    bool dangerAreaEnabled = true;
    bool showDangerAreaOnAttackStart = true;
    bool showBreathChargeOnDanger = true;
    // 地面から法線方向へ浮かせる距離。深度の競合を避けるため正の値で使用する。
    float dangerAreaGroundOffset = 0.03f;
    float dangerAreaRayStartHeight = 5.0f;
    float dangerAreaRayDistance = 250.0f;
    float dangerAreaGroundQueryInterval = 0.05f;
    bool dangerAreaAlignToGround = true;
    bool dangerAreaMultiSample = true;
    float dangerAreaLengthPadding = 0.0f;
    float breathLifeTime = 3.2f;
    float breathVFXLifeTime = 0.6f;
    float breathVFXInterval = 0.08f;
    float breathVFXTimer = 0.0f;
    float breathDamage = 18.0f;
    float breathDamageRate = 0.18f;
    float breathFixedDamage = 0.0f;
    float breathHitInterval = 0.25f;
    float breathImpactPower = 12.0f;
    float breathKnockBackPower = 4.0f;
    float breathRadius = 2.8f;
    float breathHalfHeight = 8.0f;
    float breathForwardOffset = 4.0f;
    float breathProjectileMouthForwardOffset = 0.35f;
    float breathTargetAimBlend = 0.35f;
    float breathAirGroundAimHeight = 0.35f;
    float breathDangerLifeTime = 1.9f;
    float breathDangerRange = 52.0f;
    float breathDangerWidth = 6.0f;
    float breathDangerStartOffset = 0.25f;
    float breathProjectileImpactDangerRange = 44.0f;
    float breathProjectileImpactDangerRadius = 4.2f;
    // 扇型ブレスの射程と角度に合わせて、予兆と判定配置の設定を揃える。
    float breathFanDangerRange = 22.0f;
    float breathFanDangerAngleDegrees = 120.0f;
    float breathFanDangerRadius = 3.4f;
    int breathFanDangerSampleCount = 13;
    int breathFanDangerRangeSteps = 5;

    float biteLifeTime = 0.25f;
    float biteVFXLifeTime = 0.35f;
    float biteDamage = 20.0f;
    float biteDamageRate = 0.20f;
    float biteFixedDamage = 0.0f;
    float biteImpactPower = 16.0f;
    float biteKnockBackPower = 6.0f;
    float biteRadius = 1.4f;
    float biteHalfHeight = 0.7f;
    float biteForwardOffset = 0.7f;

    float tailSweepLifeTime = 1.2f;
    float spinSweepLifeTime = 1.8f;
    float tailDamage = 22.0f;
    float tailDamageRate = 0.22f;
    float spinDamage = 30.0f;
    float spinDamageRate = 0.30f;
    float tailFixedDamage = 0.0f;
    float tailHitInterval = 0.35f;
    float tailImpactPower = 18.0f;
    float tailKnockBackPower = 8.0f;
    float tailRadius = 1.1f;
    float tailHalfHeight = 1.8f;
    float spinTailRadius = 3.0f;
    bool spinTailTrailEnabled = true; // 回転攻撃中の視認性を補う軌跡の表示設定。
    float spinTailTrailLifeTime = 0.2f; // 軌跡の残像時間。攻撃終了時は即座に消す。

    std::string stampRockVFXAssetPath = "Data/VFX/DragonStampRock.json";
    // 空の場合は敵本体原点を基準にし、必要な場合だけ部位Objectからスタンプを発生させる。
    std::string stampSourceObjectName;
    // 部位Objectが無い場合に、必要ならスタンプ発生位置へ使うモデルノード名。
    std::string stampSourceNodeName;
    float stampLifeTime = 0.5f;
    float stampDangerLifeTime = 1.8f;
    float stampDamage = 28.0f;
    float stampDamageRate = 0.28f;
    float stampFixedDamage = 0.0f;
    float stampImpactPower = 26.0f;
    float stampKnockBackPower = 12.0f;
    float stampRadius = 15.75f;
    float stampForwardOffset = 2.6f;
    int stampRockCount = 20;
    float stampRockRadius = 13.125f;
    float stampRockArcDegrees = 360.0f;
    float stampRockSpawnInterval = 0.04f;
    float stampRockLifeTime = 1.8f;
    int stampRockGroupCount = 5;
    float stampRockGroupInterval = 0.10f;
    float stampRockScaleMin = 1.3125f;
    float stampRockScaleMax = 2.3625f;
    float stampRockRadiusJitter = 2.1f;
    float stampRockRiseStartOffset = -1.1375f;

    uint32_t nextAttackInstanceId = 1u;
};
