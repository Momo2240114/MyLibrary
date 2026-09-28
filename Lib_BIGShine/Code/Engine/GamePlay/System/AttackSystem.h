#pragma once

#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <DirectXMath.h>
#include "Engine/GamePlay/Combat/CombatDamageTypes.h"

class GameObject;
class HitBox;
class HurtBox;

// HitBoxとHurtBoxが重なった時に計算される接触情報。
// VFX、CameraShake、Hit方向Blackboardなどに使う。
struct AttackHitContact
{
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
    bool isValid = false;
};

// 1回の攻撃Hit成立時に発行されるイベント情報。
// Damageだけでなく、HitStop、VFX、SE、CameraShakeなど攻撃結果に必要な値をまとめる。
struct AttackHitEvent
{
    // 攻撃側と被弾側。GameObject が途中で消えてもよいよう weak_ptr で保持する。
    std::weak_ptr<GameObject> attacker;
    std::weak_ptr<GameObject> target;

    // 実際に重なった判定。通知直後の参照用で、所有は AttackSystem には移さない。
    HitBox* hitBox = nullptr;
    HurtBox* hurtBox = nullptr;

    // Health、HitReaction、CameraShake などに渡す攻撃結果パラメータ。
    float damage = 0.0f;
    float attackerAttackPower = 0.0f;
    float damageRate = 1.0f;
    float fixedDamage = 0.0f;
    DAMAGE_APPLY_TYPE damageApplyType = DAMAGE_APPLY_TYPE::SINGLE_HIT;
    float damageHitInterval = 0.0f;
    int maxHitCount = 1;
    float hitStopDuration = 0.0f;
    float hitStopScale = 0.0f;
    uint32_t hitStopLayerMask = 0u;
    float impactPower = 0.0f;
    float knockBackPower = 0.0f;
    float targetAirLiftPower = 0.0f;
    float attackerAirLiftPower = 0.0f;

    // 接触点が取れた場合は VFX/SE/Blackboard の基準位置として使う。
    DirectX::XMFLOAT3 hitPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 hitNormal = { 0.0f, 1.0f, 0.0f };
    bool hasHitContact = false;

    // 同一攻撃内の重複 Hit 制御に使う識別情報。
    uint32_t attackInstanceId = 0u;
    uint32_t hitGroupId = 0u;
    std::string sourceAction;
    int teamId = 0;
    std::string attackTag;
    std::string hurtTag;

    // Hit 時に生成する Particle 設定。
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

    // Hit 時に生成する MeshVFX 設定。
    std::string hitMeshVFXAssetPath;
    float hitMeshVFXLifeTime = 1.0f;
    float hitMeshVFXNormalOffset = 0.0f;
    // Hit時にまとめて再生するImpactEffect Asset。
    std::string hitImpactEffectAssetPath;

    // Hit SE の再生設定。
    std::string hitSoundPath;
    float hitSoundVolume = 1.0f;
    bool hitSoundMuffled = false;
    float hitSoundMuffledCutoff = 1200.0f;

    // Hit 成立時の CameraShake 設定。
    bool cameraShakeEnabled = false;
    float cameraShakeReceiveDuration = 0.0f;
    float cameraShakeReceivePositionStrength = 0.0f;
    float cameraShakeReceiveRotationStrength = 0.0f;
    float cameraShakeAttackScale = 0.5f;
    float cameraShakeFrequency = 24.0f;
    int cameraShakePriority = 0;
};

// HitBox/HurtBoxの重なりから攻撃Hitを確定し、重複Hit防止と通知を担当するSystem。
class AttackSystem
{
public:
    using HitCallback = std::function<void(const AttackHitEvent&)>;

    void BeginFrame();
    void Clear();
    // 攻撃Actionごとに一意なIDを作り、多段Hitや同一対象への再Hit管理に使う。
    uint32_t CreateAttackInstance();

    // 攻撃終了時に、その攻撃インスタンスで登録した重複 Hit 履歴を消す。
    void ClearAttackInstance(uint32_t attackInstanceId_);

    // 同一攻撃内でも HitGroup 単位で再 Hit 可能にしたい時に使う。
    void ClearAttackHitGroup(uint32_t attackInstanceId_, uint32_t hitGroupId_);

    // 接触点なしで HitBox/HurtBox の重なりを通知する互換入口。
    void HandleHit(HitBox& hitBox_, HurtBox& hurtBox_);

    // CollisionStepSystem が求めた接触情報つきで攻撃 Hit を処理する。
    void HandleHit(
        HitBox& hitBox_,
        HurtBox& hurtBox_,
        const AttackHitContact& contact_);

    // Hit 成立時に外部へイベントを流すための Callback 登録。
    void AddHitCallback(HitCallback callback_);

    const std::vector<AttackHitEvent>& GetFrameHitEvents() const
    {
        return frameHitEvents;
    }

private:
    // attackInstanceId + hitGroupId + targetで同じ攻撃の重複Hitを防ぐ。
    bool RegisterAttackHit(
        uint32_t attackInstanceId_,
        uint32_t hitGroupId_,
        const GameObject* target_,
        bool allowMultiHit_);
    bool IsSameHierarchy(
        const std::shared_ptr<GameObject>& objectA_,
        const std::shared_ptr<GameObject>& objectB_) const;
    std::shared_ptr<GameObject> GetHierarchyRoot(
        std::shared_ptr<GameObject> object_) const;

    // Damage や Hit 方向を Blackboard に書き、被弾側 Action が読めるようにする。
    void PublishHitToBlackboard(const AttackHitEvent& event_);

    // HitBox 設定に応じて、攻撃側/受け側のカメラ揺れを再生する。
    void PlayCameraShake(const AttackHitEvent& event_) const;

private:
    // 今フレーム成立した Hit 一覧。BeginFrame で毎回クリアする。
    std::vector<AttackHitEvent> frameHitEvents;

    // Hit 成立時に呼ぶ外部購読者。
    std::vector<HitCallback> hitCallbacks;
    // keyは上位32bitがattackInstanceId、下位32bitがhitGroupId。
    std::unordered_map<uint64_t, std::unordered_set<const GameObject*>> registeredHitTargets;
    uint32_t nextAttackInstanceId = 1u;
};

