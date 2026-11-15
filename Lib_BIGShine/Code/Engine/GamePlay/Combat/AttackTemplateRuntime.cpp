#include "PrecompiledHeader.h"

#include "AttackTemplateRuntime.h"

#include "AttackTemplateManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Resource/Animation/AnimationState.h"
#include "Engine/Physics/HitBox.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/GamePlay/Input/ControllerInput.h"

#include <cstdio>
#include <algorithm>
#include <cmath>
#include "Engine/Graphics/Resource/GlTFModel.h"

namespace
{
    // HitBox が武器などの子階層に付いていても、攻撃の所有者は一番上の GameObject として扱う。
    std::shared_ptr<GameObject> GetRoot(std::shared_ptr<GameObject> object_)
    {
        while (object_ && object_->transform)
        {
            const std::shared_ptr<Transform> parentTransform_ = object_->transform->parent.lock();
            if (!parentTransform_) break;
            const std::shared_ptr<GameObject> parentObject_ = parentTransform_->gameObject.lock();
            if (!parentObject_) break;
            object_ = parentObject_;
        }
        return object_;
    }

    std::shared_ptr<HitBox> FindHitBox(
        const std::shared_ptr<GameObject>& object_,
        const std::string& hitBoxName_)
    {
        if (!object_) return nullptr;

        // 現在の GameObject に付いている ColliderComponent 群から、名前または ActionPrefix で探す。
        for (const std::shared_ptr<Component>& component_ : object_->colliderComponents)
        {
            const std::shared_ptr<HitBox> hitBox_ = std::dynamic_pointer_cast<HitBox>(component_);
            if (hitBox_ && (hitBox_->GetActionPrefix() == hitBoxName_ || hitBox_->GetName() == hitBoxName_))
                return hitBox_;
        }
        if (!object_->transform) return nullptr;

        // 見つからない場合は子階層も再帰的に探し、武器やエフェクト用の HitBox を拾う。
        for (const std::shared_ptr<Transform>& childTransform_ : object_->transform->children)
        {
            if (!childTransform_) continue;
            if (const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
                childTransform_->gameObject.lock(), hitBoxName_)) return hitBox_;
        }
        return nullptr;
    }

    const AttackTemplate* GetTemplate(const AnimEvent& event_)
    {
        AttackTemplateManager& manager_ = AttackTemplateManager::Instance();

        // AnimationEvent から直接呼ばれても使えるよう、未ロードならここでテンプレートを読み込む。
        if (manager_.GetTemplates().empty()) manager_.LoadAll();
        return manager_.FindTemplate(event_.attackTemplateId);
    }

    void ConfigureWeaponHitBox(
        const std::shared_ptr<GameObject>& owner_,
        const AttackTemplate& attackTemplate_,
        uint32_t attackInstanceId_)
    {
        if (!attackTemplate_.weaponHitBox.enabled) return;

        // AttackTemplateの数値と演出設定を、実際に衝突判定を出すHitBoxへ流し込む。
        const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
            GetRoot(owner_), attackTemplate_.weaponHitBox.hitBoxName);
        if (!hitBox_)
        {
            std::printf("AttackTemplate WeaponHitBox not found: %s\n",
                attackTemplate_.weaponHitBox.hitBoxName.c_str());
            return;
        }
        hitBox_->SetOwner(owner_.get());
        hitBox_->SetAttackInstanceId(attackInstanceId_);
        hitBox_->SetHitGroupId(attackTemplate_.hitGroupId);
        hitBox_->SetDamage(attackTemplate_.damage);
        hitBox_->SetDamageRate(attackTemplate_.damageRate);
        hitBox_->SetFixedDamage(attackTemplate_.fixedDamage);
        hitBox_->SetDamageApplyType(attackTemplate_.damageApplyType);
        hitBox_->SetDamageHitInterval(attackTemplate_.damageHitInterval);
        hitBox_->SetMaxHitCount(attackTemplate_.maxHitCount);
        hitBox_->SetImpactPower(attackTemplate_.impactPower);
        hitBox_->hitStopDuration = attackTemplate_.hitStopDuration;
        hitBox_->hitStopScale = attackTemplate_.hitStopScale;
        hitBox_->SetKnockBackPower(attackTemplate_.knockBackPower);
        hitBox_->SetAirLiftPower(
            attackTemplate_.targetAirLiftPower,
            attackTemplate_.attackerAirLiftPower);
        hitBox_->hitParticleAssetPath = attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitParticleAssetPath
            : std::string{};
        hitBox_->hitParticleEmitterName = attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitParticleEmitterName
            : std::string{};
        hitBox_->hitParticleLifeTime = attackTemplate_.hitEffect.hitParticleLifeTime;
        hitBox_->hitParticleNormalOffset = attackTemplate_.hitEffect.hitParticleNormalOffset;
        hitBox_->hitParticleUseImpactAxis = attackTemplate_.hitEffect.hitParticleUseImpactAxis;
        hitBox_->hitParticleAxisToAttackerBlend =
            attackTemplate_.hitEffect.hitParticleAxisToAttackerBlend;
        hitBox_->hitParticleUseTargetAttraction =
            attackTemplate_.hitEffect.hitParticleUseTargetAttraction;
        hitBox_->hitParticleTargetAttractionStrength =
            attackTemplate_.hitEffect.hitParticleTargetAttractionStrength;
        hitBox_->hitParticleTargetAttractionRange =
            attackTemplate_.hitEffect.hitParticleTargetAttractionRange;
        hitBox_->hitParticleTargetAttractionFalloff =
            attackTemplate_.hitEffect.hitParticleTargetAttractionFalloff;
        hitBox_->hitMeshVFXAssetPath = attackTemplate_.hitEffect.spawnOnHit
            ? attackTemplate_.hitEffect.hitMeshVfxAssetPath
            : std::string{};
        hitBox_->hitMeshVFXLifeTime = attackTemplate_.hitEffect.hitMeshVfxLifeTime;
        hitBox_->hitMeshVFXNormalOffset = attackTemplate_.hitEffect.hitMeshVfxNormalOffset;
        hitBox_->feedback = attackTemplate_.feedback;
        hitBox_->cameraShakeEnabled = attackTemplate_.cameraShake.enabled;
        hitBox_->cameraShakeReceiveDuration = attackTemplate_.cameraShake.receiveDuration;
        hitBox_->cameraShakeReceivePositionStrength =
            attackTemplate_.cameraShake.receivePositionStrength;
        hitBox_->cameraShakeReceiveRotationStrength =
            attackTemplate_.cameraShake.receiveRotationStrength;
        hitBox_->cameraShakeAttackScale = attackTemplate_.cameraShake.attackScale;
        hitBox_->cameraShakeFrequency = attackTemplate_.cameraShake.frequency;
        hitBox_->cameraShakePriority = attackTemplate_.cameraShake.priority;
        hitBox_->SetHitEffectBlackboardOverrideEnabled(false);
        hitBox_->SetAttackDataBlackboardOverrideEnabled(false);
        hitBox_->canHitOwner = attackTemplate_.canHitOwner;
        hitBox_->guardBreak = attackTemplate_.guardBreak;
        hitBox_->canBeGuarded = attackTemplate_.canBeGuarded;
        hitBox_->allowMultiHit =
            attackTemplate_.allowMultiHit ||
            attackTemplate_.damageApplyType != DAMAGE_APPLY_TYPE::SINGLE_HIT;

        // Start 時点では設定だけ済ませ、実際の有効化は Update の activeStart/End で行う。
        hitBox_->Deactivate();
    }
}

void AttackTemplateRuntime::Start(
    const std::shared_ptr<GameObject>& owner_,
    AnimEvent& event_)
{
    if (!owner_) return;
    const AttackTemplate* attackTemplate_ = GetTemplate(event_);
    if (!attackTemplate_) return;

    BS_PROFILE_SCOPE("AttackTemplateRuntime::StartFeedback");
    const auto actor_ = GetRoot(owner_);
    // 敵の空振りでPadを揺らさず、プレイヤー自身の攻撃開始だけを通知する。
    if (actor_ && actor_->GetLayer() == LayerType::Player && attackTemplate_->feedback.rumbleEnabled)
        ControllerInput::Instance().PlayRumble(attackTemplate_->feedback.startStrength,
            attackTemplate_->feedback.startStrength * 0.5f, attackTemplate_->feedback.duration);
    if (owner_->transform)
        AudioManager::Instance().PlaySpatialAttackSound(attackTemplate_->feedback.startSoundPath,
            owner_->transform->GetWorldPosition(), attackTemplate_->feedback.volume,
            attackTemplate_->feedback.minDistance, attackTemplate_->feedback.maxDistance);

    // AnimEvent側にRuntime状態を持たせ、Update/Endで同じ攻撃インスタンスを追跡する。
    event_.attackTemplateRuntimeActive = true;
    if (attackTemplate_->weaponHitBox.enabled)
        if (const auto hitBox_ = FindHitBox(GetRoot(owner_), attackTemplate_->weaponHitBox.hitBoxName))
            event_.previousCanBeGuarded = hitBox_->canBeGuarded;
    event_.attackTemplateInstanceId = owner_->GetManager().GetAttackSystem().CreateAttackInstance();
    event_.spawnedAttackEffects.assign(attackTemplate_->attackEffects.size(), false);
    ConfigureWeaponHitBox(owner_, *attackTemplate_, event_.attackTemplateInstanceId);

    if (event_.attackTemplateId == "RashAttack_04_Sheild" && actor_ && actor_->transform)
    {
        if (const auto shield_ = FindHitBox(actor_, attackTemplate_->weaponHitBox.hitBoxName))
        {
            const auto shape_ = shield_->GetCapsuleData();
            DirectX::XMFLOAT3 center_{};
            DirectX::XMStoreFloat3(&center_, DirectX::XMVectorScale(DirectX::XMVectorAdd(shape_.p0, shape_.p1), 0.5f));
            owner_->GetManager().GetEffectManager().SpawnGroundMark(GroundMarkShape::IMPACT,
                center_, shape_.radius, shape_.radius);
        }
    }

    for (const SlashEffectTemplate& slash_ : attackTemplate_->slashEffects)
    {
        if (!slash_.enabled || slash_.vfxPath.empty()) continue;

        // SlashEffectは攻撃開始時にまとめて生成し、必要ならVFX側にもHitBox情報を持たせる。
        AttackEffectSpawnRequest request_{};
        request_.ownerObject = owner_.get();
        request_.attackInstanceId = event_.attackTemplateInstanceId;
        request_.setting.vfxPath = slash_.vfxPath;
        request_.setting.spawnTime = slash_.spawnTimeOffset;
        request_.setting.lifeTime = slash_.lifeTime;
        request_.setting.localOffset = slash_.localOffset;
        request_.setting.localRotation = slash_.localRotation;
        request_.setting.localScale = slash_.localScale;
        // 保存済みのボーン追従設定を渡し、斬撃を実際の武器を持つ手の姿勢へ同期する。
        request_.setting.followOwner = slash_.followOwner || slash_.followNode;
        request_.setting.attachNodeName = slash_.followNode ? slash_.baseNodeName : std::string{};
        request_.setting.hasHitBox = slash_.hitBox.enabled;
        request_.setting.hitBoxName = slash_.hitBox.hitBoxName;
        request_.setting.hitStartTime = slash_.hitBox.activeStartTime;
        request_.setting.hitEndTime = slash_.hitBox.activeEndTime;
        request_.setting.hitBoxRadius = slash_.hitBox.radius;
        request_.setting.hitBoxHalfHeight = slash_.hitBox.halfHeight;
        request_.setting.damage = attackTemplate_->damage;
        request_.setting.damageRate = attackTemplate_->damageRate;
        request_.setting.fixedDamage = attackTemplate_->fixedDamage;
        request_.setting.damageApplyType = attackTemplate_->damageApplyType;
        request_.setting.damageHitInterval = attackTemplate_->damageHitInterval;
        request_.setting.maxHitCount = attackTemplate_->maxHitCount;
        request_.setting.impactPower = attackTemplate_->impactPower;
        request_.setting.knockBackPower = attackTemplate_->knockBackPower;
        request_.setting.targetAirLiftPower = attackTemplate_->targetAirLiftPower;
        request_.setting.attackerAirLiftPower = attackTemplate_->attackerAirLiftPower;
        request_.setting.hitStopDuration = attackTemplate_->hitStopDuration;
        request_.setting.hitStopScale = attackTemplate_->hitStopScale;
        request_.setting.hitEffectPath = attackTemplate_->hitEffect.hitMeshVfxAssetPath;
        request_.setting.hitGroupId = attackTemplate_->hitGroupId;
        request_.setting.canBeGuarded = attackTemplate_->canBeGuarded;
        request_.setting.guardBreak = attackTemplate_->guardBreak;
        request_.setting.allowMultiHit =
            attackTemplate_->allowMultiHit ||
            attackTemplate_->damageApplyType != DAMAGE_APPLY_TYPE::SINGLE_HIT;
        request_.setting.feedback = attackTemplate_->feedback;
        request_.setting.cameraShakeEnabled = attackTemplate_->cameraShake.enabled;
        request_.setting.cameraShakeReceiveDuration = attackTemplate_->cameraShake.receiveDuration;
        request_.setting.cameraShakeReceivePositionStrength =
            attackTemplate_->cameraShake.receivePositionStrength;
        request_.setting.cameraShakeReceiveRotationStrength =
            attackTemplate_->cameraShake.receiveRotationStrength;
        request_.setting.cameraShakeAttackScale = attackTemplate_->cameraShake.attackScale;
        request_.setting.cameraShakeFrequency = attackTemplate_->cameraShake.frequency;
        request_.setting.cameraShakePriority = attackTemplate_->cameraShake.priority;
        owner_->GetManager().GetEffectManager().SpawnSlashEffect(request_);
    }
}

void AttackTemplateRuntime::Update(
    const std::shared_ptr<GameObject>& owner_,
    AnimEvent& event_,
    float elapsedTime_)
{
    if (!owner_ || !event_.attackTemplateRuntimeActive) return;
    const AttackTemplate* attackTemplate_ = GetTemplate(event_);
    if (!attackTemplate_) return;

    // アニメーション内の経過時間に合わせて、武器HitBoxと派生攻撃エフェクトを切り替える。
    if (attackTemplate_->weaponHitBox.enabled)
    {
        const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
            GetRoot(owner_), attackTemplate_->weaponHitBox.hitBoxName);
        if (hitBox_)
        {
            const bool active_ = elapsedTime_ >= attackTemplate_->weaponHitBox.activeStartTime &&
                elapsedTime_ <= attackTemplate_->weaponHitBox.activeEndTime;
            if (active_ && !hitBox_->IsEnabled()) hitBox_->Activate();
            if (!active_ && hitBox_->IsEnabled()) hitBox_->Deactivate();
        }
    }
    if (event_.spawnedAttackEffects.size() != attackTemplate_->attackEffects.size())
        event_.spawnedAttackEffects.assign(attackTemplate_->attackEffects.size(), false);

    // 通常の攻撃エフェクトは spawnTimeOffset を越えた瞬間に一度だけ生成する。
    for (size_t effectIndex_ = 0; effectIndex_ < attackTemplate_->attackEffects.size(); ++effectIndex_)
    {
        const AttackEffectSpawnTemplate& effect_ = attackTemplate_->attackEffects[effectIndex_];
        if (!event_.spawnedAttackEffects[effectIndex_] && effect_.enabled && elapsedTime_ >= effect_.spawnTimeOffset)
        {
            owner_->GetManager().GetEffectManager().SpawnAttackEffect(
                effect_, *attackTemplate_, owner_.get(), event_.attackTemplateInstanceId);
            event_.spawnedAttackEffects[effectIndex_] = true;
        }
    }
}

void AttackTemplateRuntime::End(
    const std::shared_ptr<GameObject>& owner_,
    AnimEvent& event_)
{
    if (!owner_) return;
    const AttackTemplate* attackTemplate_ = GetTemplate(event_);
    if (attackTemplate_ && attackTemplate_->weaponHitBox.enabled)
    {
        // 攻撃終了時は一時的に上書きしたHitBox設定を戻し、次の通常利用へ影響を残さない。
        if (const std::shared_ptr<HitBox> hitBox_ = FindHitBox(
            GetRoot(owner_), attackTemplate_->weaponHitBox.hitBoxName))
        {
            hitBox_->Deactivate();
            hitBox_->guardBreak = false;
            hitBox_->canBeGuarded = event_.previousCanBeGuarded;
            hitBox_->SetHitEffectBlackboardOverrideEnabled(true);
            hitBox_->SetAttackDataBlackboardOverrideEnabled(true);
        }
    }
    event_.attackTemplateRuntimeActive = false;

    // Runtime 中だけ持っていた「どの追加エフェクトを出したか」の印を破棄する。
    event_.spawnedAttackEffects.clear();
}

// アニメーション上の判定窓で切り取った軌道を、既存HitBoxの共通判定へ蓄える。
void AttackTemplateRuntime::SampleWeaponInterval(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_,
    GltfModelCommponennt* model_, const AnimState& state_, float previousTime_, float currentTime_)
{
    const auto template_ = GetTemplate(event_);
    if (!owner_ || !model_ || !template_ || !template_->weaponHitBox.enabled || event_.attackTemplateInstanceId == 0 ||
        state_.animationIndex < 0 || state_.animationIndex >= static_cast<int>(model_->GetAnimations().size())) return;
    const float start_ = (std::max)(previousTime_, event_.startTime + template_->weaponHitBox.activeStartTime);
    const float end_ = (std::min)(currentTime_, (std::min)(event_.endTime, event_.startTime + template_->weaponHitBox.activeEndTime));
    if (end_ < start_ || currentTime_ <= previousTime_) return;
    const auto hitBox_ = FindHitBox(GetRoot(owner_), template_->weaponHitBox.hitBoxName);
    if (!hitBox_ || hitBox_->attackInstanceId != event_.attackTemplateInstanceId) return;
    BS_PROFILE_SCOPE("AttackTemplateRuntime::SampleWeaponInterval");
    std::vector<GltfModelCommponennt::NodePose> savedPose_;
    std::vector<GltfModelCommponennt::NodePose> sampledPose_;
    model_->GetNodePoses(savedPose_);
    const std::string rootName_ = state_.rootMotion.sourceNodeName.empty() ? model_->GetRootMotionName() : state_.rootMotion.sourceNodeName;
    const int rootIndex_ = model_->GetNodeIndex(rootName_.empty() ? "root" : rootName_.c_str());
    constexpr float WEAPON_SAMPLE_RATE = 120.0f;
    const int steps_ = std::clamp(static_cast<int>(std::ceil((end_ - start_) * WEAPON_SAMPLE_RATE)), 1, 64);
    for (int step_ = 0; step_ <= steps_; ++step_)
    {
        const float time_ = start_ + (end_ - start_) * static_cast<float>(step_) / steps_;
        model_->ComputeAnimation(state_.animationIndex, time_, sampledPose_);
        if (rootIndex_ >= 0 && static_cast<std::size_t>(rootIndex_) < sampledPose_.size())
        {
            // 通常描画と同じroot抽出を行い、元移動が武器位置へ二重適用されないようにする。
            auto& position_ = sampledPose_[rootIndex_].position;
            const auto& base_ = savedPose_[rootIndex_].position;
            if (state_.rootMotion.applyPositionX || state_.rootMotion.cutVisualPositionX) position_.x = base_.x;
            if (state_.rootMotion.applyPositionY || state_.rootMotion.cutVisualPositionY) position_.y = base_.y;
            if (state_.rootMotion.applyPositionZ || state_.rootMotion.cutVisualPositionZ) position_.z = base_.z;
        }
        model_->SetNodePoses(sampledPose_);
        model_->UpdateTransformIfDirty();
        hitBox_->AddFrameSweepSample(hitBox_->GetCapsuleData());
    }
    // 一時サンプリングを描画や次フレームのRootMotion履歴へ残さない。
    model_->SetNodePoses(savedPose_);
    model_->UpdateTransformIfDirty();
}
