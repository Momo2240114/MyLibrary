#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"

#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AttackEffectHitComponent;
class DragonNeckCommandIKComponent;
class DragonTailCommandIKComponent;
class GameObject;
class MeshVFXComponent;

// Dragonのブレスや尻尾攻撃の判定・VFX発生を、首/尻尾IKの位置に合わせて制御するComponent。
class DragonAttackControllerComponent : public Component
{
public:
    const std::string& GetName() const override;
    void Start() override;
    void Update(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;

    // 口元から前方へ伸びるブレス攻撃を開始する。
    void PlayBreath();
    // 口元周辺に短時間の噛みつき判定とVFXを発生させる。
    void PlayBite();
    // 尻尾IKの攻撃有効時間に合わせて通常の薙ぎ払い判定を開始する。
    void PlayTailSweep();
    // 尻尾IKの攻撃有効時間に合わせて回転薙ぎ払い判定を開始する。
    void PlaySpinSweep();
    // 着地や踏みつけの瞬間に、足元周辺へ範囲攻撃と岩VFXを発生させる。
    void PlayStamp();
    // 現在このComponentが生成した攻撃判定を停止する。
    void StopAttack();
    // ブレス攻撃の危険範囲だけを事前表示する。
    void ShowBreathDangerArea();
    // 噛みつき攻撃の危険範囲だけを事前表示する。
    void ShowBiteDangerArea();
    // 尻尾薙ぎ払い攻撃の危険範囲だけを事前表示する。
    void ShowTailSweepDangerArea();
    // 回転薙ぎ払い攻撃の危険範囲だけを事前表示する。
    void ShowSpinSweepDangerArea();
    // スタンプ攻撃の危険範囲だけを事前表示する。
    void ShowStampDangerArea();

private:
    struct RuntimeAttack
    {
        std::shared_ptr<GameObject> object;
        std::shared_ptr<GameObject> dangerAreaObject;
        std::weak_ptr<AttackEffectHitComponent> hitComponent;
        std::weak_ptr<MeshVFXComponent> dangerAreaVFX;
        DirectX::XMFLOAT3 dangerAreaLocalOffset = { 0.0f, 0.0f, 0.0f };
        float dangerAreaRadius = 1.0f;
        float dangerAreaHalfHeight = 0.0f;
        float lifeTimer = 0.0f;
        bool followMouth = false;
        bool followTail = false;
        bool requireTailAttackActive = false;
        bool hasDangerArea = false;
    };

    // ActionSequenceやDragonAIから呼べる攻撃ActionをOwnerへ登録する。
    void RegisterActions();
    void UnregisterActions();
    void EnsureDependencies();
    void UpdateRuntimeAttacks(float deltaTime_);
    void UpdateBreathVFX(float deltaTime_);
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
        float hitInterval_);
    void ApplyFollowTransform(RuntimeAttack& runtime_);
    void ApplyDangerAreaTransform(RuntimeAttack& runtime_);
    void SpawnBreathVFX();
    void SpawnBiteVFX();
    void SpawnStampHit();
    void SpawnStampRockVFX(const DirectX::XMFLOAT3& centerPosition_);
    void SpawnDangerArea(
        const std::string& objectName_,
        float lifeTime_,
        bool followMouth_,
        bool followTail_,
        const DirectX::XMFLOAT3& localOffset_,
        float radius_,
        float halfHeight_);
    void AttachDangerArea(
        RuntimeAttack& runtime_,
        const std::string& objectName_,
        const DirectX::XMFLOAT3& localOffset_,
        float radius_,
        float halfHeight_);
    void ConfigureDangerAreaVFX(
        const std::shared_ptr<MeshVFXComponent>& meshVFX_,
        float radius_,
        float halfHeight_) const;
    void StopRuntimeAttack(RuntimeAttack& runtime_);
    DirectX::XMFLOAT3 ResolveForwardRotation(const DirectX::XMFLOAT3& forward_) const;
    uint32_t CreateAttackInstanceId();

private:
    std::weak_ptr<DragonNeckCommandIKComponent> neckIK;
    std::weak_ptr<DragonTailCommandIKComponent> tailIK;
    std::string registeredActionCategory;
    std::vector<std::string> registeredActionNames;
    std::vector<RuntimeAttack> runtimeAttacks;

    std::string actionCategory = "DragonAttack";
    std::string breathVFXAssetPath = "Data/VFX/DragonBreath.json";
    std::string biteVFXAssetPath = "Data/VFX/DragonBite.json";
    std::string dangerAreaVFXAssetPath = "Data/VFX/EnemyDangerArea.json";

    bool dangerAreaEnabled = true;
    bool showDangerAreaOnAttackStart = true;
    float dangerAreaGroundOffset = 0.03f;
    float dangerAreaLengthPadding = 0.0f;
    float breathLifeTime = 2.0f;
    float breathVFXLifeTime = 0.35f;
    float breathVFXInterval = 0.08f;
    float breathVFXTimer = 0.0f;
    float breathDamage = 18.0f;
    float breathDamageRate = 0.18f;
    float breathFixedDamage = 0.0f;
    float breathHitInterval = 0.25f;
    float breathImpactPower = 12.0f;
    float breathKnockBackPower = 4.0f;
    float breathRadius = 1.2f;
    float breathHalfHeight = 4.0f;
    float breathForwardOffset = 4.0f;

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

    std::string stampRockVFXAssetPath = "Data/VFX/DragonStampRock.json";
    float stampLifeTime = 0.35f;
    float stampDangerLifeTime = 1.0f;
    float stampDamage = 28.0f;
    float stampDamageRate = 0.28f;
    float stampFixedDamage = 0.0f;
    float stampImpactPower = 26.0f;
    float stampKnockBackPower = 12.0f;
    float stampRadius = 5.0f;
    float stampForwardOffset = 1.5f;
    int stampRockCount = 7;
    float stampRockRadius = 3.5f;
    float stampRockArcDegrees = 180.0f;
    float stampRockLifeTime = 1.2f;

    uint32_t nextAttackInstanceId = 1u;
};
