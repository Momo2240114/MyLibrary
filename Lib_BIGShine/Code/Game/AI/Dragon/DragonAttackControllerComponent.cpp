#include "PrecompiledHeader.h"

#include "DragonAttackControllerComponent.h"

#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/Animation/DragonNeckCommandIKComponent.h"
#include "Engine/Graphics/Resource/Animation/DragonTailCommandIKComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr float EPSILON = 0.00001f;
    constexpr const char* ACTION_PLAY_BREATH = "DragonAttack_PlayBreath";
    constexpr const char* ACTION_PLAY_BITE = "DragonAttack_PlayBite";
    constexpr const char* ACTION_PLAY_TAIL_SWEEP = "DragonAttack_PlayTailSweep";
    constexpr const char* ACTION_PLAY_SPIN_SWEEP = "DragonAttack_PlaySpinSweep";
    constexpr const char* ACTION_PLAY_STAMP = "DragonAttack_PlayStamp";
    constexpr const char* ACTION_SHOW_BREATH_DANGER_AREA = "DragonAttack_ShowBreathDangerArea";
    constexpr const char* ACTION_SHOW_BITE_DANGER_AREA = "DragonAttack_ShowBiteDangerArea";
    constexpr const char* ACTION_SHOW_TAIL_SWEEP_DANGER_AREA = "DragonAttack_ShowTailSweepDangerArea";
    constexpr const char* ACTION_SHOW_SPIN_SWEEP_DANGER_AREA = "DragonAttack_ShowSpinSweepDangerArea";
    constexpr const char* ACTION_SHOW_STAMP_DANGER_AREA = "DragonAttack_ShowStampDangerArea";
    constexpr const char* ACTION_STOP = "DragonAttack_Stop";
    constexpr const char* DANGER_AREA_MODEL_PATH = "__InternalMeshDangerArea__";

    DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& value_)
    {
        const float length_ = std::sqrt(
            value_.x * value_.x +
            value_.y * value_.y +
            value_.z * value_.z);
        if (length_ <= EPSILON) return { 0.0f, 0.0f, 1.0f };
        return {
            value_.x / length_,
            value_.y / length_,
            value_.z / length_
        };
    }

    DirectX::XMFLOAT3 Add(
        const DirectX::XMFLOAT3& a_,
        const DirectX::XMFLOAT3& b_)
    {
        return { a_.x + b_.x, a_.y + b_.y, a_.z + b_.z };
    }

    DirectX::XMFLOAT3 Scale(
        const DirectX::XMFLOAT3& value_,
        float scale_)
    {
        return { value_.x * scale_, value_.y * scale_, value_.z * scale_ };
    }
}

const std::string& DragonAttackControllerComponent::GetName() const
{
    static const std::string name_ = "DragonAttackControllerComponent";
    return name_;
}

void DragonAttackControllerComponent::Start()
{
    EnsureDependencies();
    RegisterActions();
}

void DragonAttackControllerComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::Update");

    EnsureDependencies();
    UpdateRuntimeAttacks(deltaTime_);
    UpdateBreathVFX(deltaTime_);
}

void DragonAttackControllerComponent::PlayBreath()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayBreath");

    breathVFXTimer = 0.0f;
    SpawnAttackHit(
        "RuntimeDragonBreathHit",
        breathLifeTime,
        true,
        false,
        false,
        breathDamage,
        breathDamageRate,
        breathFixedDamage,
        breathImpactPower,
        breathKnockBackPower,
        breathRadius,
        breathHalfHeight,
        { 0.0f, 0.0f, breathForwardOffset },
        breathHitInterval);
    SpawnBreathVFX();
}

void DragonAttackControllerComponent::PlayBite()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayBite");

    SpawnAttackHit(
        "RuntimeDragonBiteHit",
        biteLifeTime,
        true,
        false,
        false,
        biteDamage,
        biteDamageRate,
        biteFixedDamage,
        biteImpactPower,
        biteKnockBackPower,
        biteRadius,
        biteHalfHeight,
        { 0.0f, 0.0f, biteForwardOffset },
        0.0f);
    SpawnBiteVFX();
}

void DragonAttackControllerComponent::PlayTailSweep()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayTailSweep");

    SpawnAttackHit(
        "RuntimeDragonTailSweepHit",
        tailSweepLifeTime,
        false,
        true,
        true,
        tailDamage,
        tailDamageRate,
        tailFixedDamage,
        tailImpactPower,
        tailKnockBackPower,
        tailRadius,
        tailHalfHeight,
        { 0.0f, 0.0f, 0.0f },
        tailHitInterval);
}

void DragonAttackControllerComponent::PlaySpinSweep()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlaySpinSweep");

    SpawnAttackHit(
        "RuntimeDragonSpinSweepHit",
        spinSweepLifeTime,
        false,
        true,
        true,
        spinDamage,
        spinDamageRate,
        tailFixedDamage,
        tailImpactPower,
        tailKnockBackPower,
        tailRadius,
        tailHalfHeight,
        { 0.0f, 0.0f, 0.0f },
        tailHitInterval);
}

void DragonAttackControllerComponent::PlayStamp()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayStamp");

    SpawnStampHit();
}

void DragonAttackControllerComponent::ShowBreathDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowBreathDangerArea");

    SpawnDangerArea(
        "RuntimeDragonBreathDangerArea",
        breathLifeTime,
        true,
        false,
        { 0.0f, 0.0f, breathForwardOffset },
        breathRadius,
        breathHalfHeight + dangerAreaLengthPadding);
}

void DragonAttackControllerComponent::ShowBiteDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowBiteDangerArea");

    SpawnDangerArea(
        "RuntimeDragonBiteDangerArea",
        biteLifeTime,
        true,
        false,
        { 0.0f, 0.0f, biteForwardOffset },
        biteRadius,
        biteHalfHeight + dangerAreaLengthPadding);
}

void DragonAttackControllerComponent::ShowTailSweepDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowTailSweepDangerArea");

    SpawnDangerArea(
        "RuntimeDragonTailSweepDangerArea",
        tailSweepLifeTime,
        false,
        true,
        { 0.0f, 0.0f, 0.0f },
        tailRadius,
        tailHalfHeight + dangerAreaLengthPadding);
}

void DragonAttackControllerComponent::ShowSpinSweepDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowSpinSweepDangerArea");

    SpawnDangerArea(
        "RuntimeDragonSpinSweepDangerArea",
        spinSweepLifeTime,
        false,
        true,
        { 0.0f, 0.0f, 0.0f },
        tailRadius,
        tailHalfHeight + dangerAreaLengthPadding);
}

void DragonAttackControllerComponent::ShowStampDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowStampDangerArea");

    SpawnDangerArea(
        "RuntimeDragonStampDangerArea",
        stampDangerLifeTime,
        false,
        false,
        { 0.0f, 0.0f, stampForwardOffset },
        stampRadius,
        0.0f);
}

void DragonAttackControllerComponent::StopAttack()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::StopAttack");

    for (RuntimeAttack& runtime_ : runtimeAttacks)
    {
        StopRuntimeAttack(runtime_);
    }
    runtimeAttacks.clear();
}

void DragonAttackControllerComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    UnregisterActions();
    registeredActionCategory = actionCategory;

    auto registerAction_ = [this, &owner_](const std::string& actionName_, const ActionRegistry::ActionFunc& function_)
        {
            // AIとActionSequenceのどちらからでも、Dragon本体のローカルActionとして攻撃制御を呼べるようにする。
            owner_->RegisterAction(actionCategory, actionName_, function_);
            registeredActionNames.push_back(owner_->BuildActionName(actionName_));
        };

    registerAction_(ACTION_PLAY_BREATH, [this]() { PlayBreath(); });
    registerAction_(ACTION_PLAY_BITE, [this]() { PlayBite(); });
    registerAction_(ACTION_PLAY_TAIL_SWEEP, [this]() { PlayTailSweep(); });
    registerAction_(ACTION_PLAY_SPIN_SWEEP, [this]() { PlaySpinSweep(); });
    registerAction_(ACTION_PLAY_STAMP, [this]() { PlayStamp(); });
    registerAction_(ACTION_SHOW_BREATH_DANGER_AREA, [this]() { ShowBreathDangerArea(); });
    registerAction_(ACTION_SHOW_BITE_DANGER_AREA, [this]() { ShowBiteDangerArea(); });
    registerAction_(ACTION_SHOW_TAIL_SWEEP_DANGER_AREA, [this]() { ShowTailSweepDangerArea(); });
    registerAction_(ACTION_SHOW_SPIN_SWEEP_DANGER_AREA, [this]() { ShowSpinSweepDangerArea(); });
    registerAction_(ACTION_SHOW_STAMP_DANGER_AREA, [this]() { ShowStampDangerArea(); });
    registerAction_(ACTION_STOP, [this]() { StopAttack(); });
}

void DragonAttackControllerComponent::UnregisterActions()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    const std::string removeCategory_ =
        registeredActionCategory.empty() ? actionCategory : registeredActionCategory;
    for (const std::string& actionName_ : registeredActionNames)
    {
        owner_->GetLocalRegistry().RemoveAction(removeCategory_, actionName_);
    }
    registeredActionNames.clear();
    registeredActionCategory.clear();
}

void DragonAttackControllerComponent::EnsureDependencies()
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    neckIK = owner_->GetComponent<DragonNeckCommandIKComponent>();
    tailIK = owner_->GetComponent<DragonTailCommandIKComponent>();
}

void DragonAttackControllerComponent::UpdateRuntimeAttacks(float deltaTime_)
{
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);

    for (RuntimeAttack& runtime_ : runtimeAttacks)
    {
        runtime_.lifeTimer -= safeDeltaTime_;
        ApplyFollowTransform(runtime_);
        ApplyDangerAreaTransform(runtime_);

        if (runtime_.requireTailAttackActive)
        {
            const std::shared_ptr<DragonTailCommandIKComponent> tailIK_ = tailIK.lock();
            const std::shared_ptr<AttackEffectHitComponent> hitComponent_ = runtime_.hitComponent.lock();
            if (hitComponent_)
            {
                if (tailIK_ && tailIK_->IsAttackActive())
                {
                    hitComponent_->Play();
                }
                else
                {
                    hitComponent_->Stop();
                }
            }
        }
    }

    runtimeAttacks.erase(
        std::remove_if(
            runtimeAttacks.begin(),
            runtimeAttacks.end(),
            [this](RuntimeAttack& runtime_)
            {
                if (runtime_.lifeTimer > 0.0f) return false;
                StopRuntimeAttack(runtime_);
                return true;
            }),
        runtimeAttacks.end());
}

void DragonAttackControllerComponent::UpdateBreathVFX(float deltaTime_)
{
    if (runtimeAttacks.empty()) return;

    bool hasBreath_ = false;
    for (const RuntimeAttack& runtime_ : runtimeAttacks)
    {
        if (runtime_.followMouth && !runtime_.hitComponent.expired())
        {
            hasBreath_ = true;
            break;
        }
    }
    if (!hasBreath_) return;

    breathVFXTimer -= (std::max)(deltaTime_, 0.0f);
    if (breathVFXTimer > 0.0f) return;

    breathVFXTimer = (std::max)(breathVFXInterval, 0.01f);
    SpawnBreathVFX();
}

std::shared_ptr<AttackEffectHitComponent> DragonAttackControllerComponent::SpawnAttackHit(
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
    float hitInterval_)
{
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return nullptr;

    std::shared_ptr<GameObject> attackObject_ = owner_->GetManager().Create(objectName_);
    if (!attackObject_) return nullptr;

    attackObject_->SetNoSaveObject();
    attackObject_->SetLayer(LayerType::EnemyBullet);

    std::shared_ptr<AttackEffectHitComponent> hitComponent_ =
        attackObject_->AddComponent<AttackEffectHitComponent>();
    if (!hitComponent_) return nullptr;

    AttackEffectHitShapeTemplate shape_{};
    shape_.hitBoxName = objectName_ + "_Shape";
    shape_.shape = ATTACK_HIT_SHAPE::CAPSULE;
    shape_.localOffset = localOffset_;
    shape_.radius = (std::max)(radius_, 0.01f);
    shape_.halfHeight = (std::max)(halfHeight_, 0.0f);
    shape_.activeStartTime = 0.0f;
    shape_.activeEndTime = lifeTime_;
    shape_.hitInterval = hitInterval_;
    shape_.allowMultiHit = hitInterval_ > 0.0f;

    const uint32_t attackInstanceId_ = CreateAttackInstanceId();
    hitComponent_->SetOwner(owner_.get());
    hitComponent_->SetAttackInstanceId(attackInstanceId_);
    hitComponent_->SetHitGroupId(attackInstanceId_);
    hitComponent_->SetEffectType(ATTACK_EFFECT_TYPE::BREATH);
    hitComponent_->SetLifeTime(lifeTime_);
    hitComponent_->SetDestroyOnLifeEnd(false);
    hitComponent_->SetDamage(damage_);
    hitComponent_->SetDamageRate(damageRate_);
    hitComponent_->SetFixedDamage(fixedDamage_);
    hitComponent_->SetDamageApplyType(hitInterval_ > 0.0f
        ? DAMAGE_APPLY_TYPE::DAMAGE_OVER_TIME
        : DAMAGE_APPLY_TYPE::SINGLE_HIT);
    hitComponent_->SetDamageHitInterval(hitInterval_);
    hitComponent_->SetMaxHitCount(hitInterval_ > 0.0f ? 8 : 1);
    hitComponent_->SetImpactPower(impactPower_);
    hitComponent_->SetKnockBackPower(knockBackPower_);
    hitComponent_->SetAllowMultiHit(hitInterval_ > 0.0f);
    hitComponent_->SetHitShapes({ shape_ });

    RuntimeAttack runtime_{};
    runtime_.object = attackObject_;
    runtime_.hitComponent = hitComponent_;
    runtime_.lifeTimer = lifeTime_;
    runtime_.followMouth = followMouth_;
    runtime_.followTail = followTail_;
    runtime_.requireTailAttackActive = requireTailAttackActive_;
    ApplyFollowTransform(runtime_);
    if (showDangerAreaOnAttackStart)
    {
        AttachDangerArea(
            runtime_,
            objectName_ + "_DangerArea",
            localOffset_,
            radius_,
            halfHeight_ + dangerAreaLengthPadding);
        ApplyDangerAreaTransform(runtime_);
    }
    hitComponent_->Play();
    runtimeAttacks.push_back(runtime_);

    return hitComponent_;
}

void DragonAttackControllerComponent::ApplyFollowTransform(RuntimeAttack& runtime_)
{
    if (!runtime_.object || !runtime_.object->transform) return;

    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    bool resolved_ = false;

    if (runtime_.followMouth)
    {
        const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock();
        resolved_ = neckIK_ &&
            neckIK_->GetMouthWorldPosition(position_) &&
            neckIK_->GetMouthWorldForward(forward_);
    }
    else if (runtime_.followTail)
    {
        const std::shared_ptr<DragonTailCommandIKComponent> tailIK_ = tailIK.lock();
        resolved_ = tailIK_ &&
            tailIK_->GetTailTipWorldPosition(position_) &&
            tailIK_->GetTailTipWorldForward(forward_);
    }

    if (!resolved_)
    {
        const std::shared_ptr<GameObject> owner_ = GetGameObject();
        if (!owner_ || !owner_->transform) return;
        position_ = owner_->transform->GetWorldPosition();
        const Vector3 ownerForward_ = owner_->transform->GetFront();
        forward_ = { ownerForward_.x, ownerForward_.y, ownerForward_.z };
    }

    runtime_.object->transform->localPosition = position_;
    runtime_.object->transform->localRotation = ResolveForwardRotation(forward_);
    runtime_.object->transform->MarkDirty();
}

void DragonAttackControllerComponent::ApplyDangerAreaTransform(RuntimeAttack& runtime_)
{
    if (!runtime_.hasDangerArea || !runtime_.dangerAreaObject || !runtime_.dangerAreaObject->transform)
    {
        return;
    }

    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    bool resolved_ = false;

    if (runtime_.followMouth)
    {
        const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock();
        resolved_ = neckIK_ &&
            neckIK_->GetMouthWorldPosition(position_) &&
            neckIK_->GetMouthWorldForward(forward_);
    }
    else if (runtime_.followTail)
    {
        const std::shared_ptr<DragonTailCommandIKComponent> tailIK_ = tailIK.lock();
        resolved_ = tailIK_ &&
            tailIK_->GetTailTipWorldPosition(position_) &&
            tailIK_->GetTailTipWorldForward(forward_);
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!resolved_)
    {
        if (!owner_ || !owner_->transform) return;
        position_ = owner_->transform->GetWorldPosition();
        const Vector3 ownerForward_ = owner_->transform->GetFront();
        forward_ = { ownerForward_.x, ownerForward_.y, ownerForward_.z };
    }

    DirectX::XMFLOAT3 groundPosition_ = position_;
    if (owner_ && owner_->transform)
    {
        groundPosition_.y = owner_->transform->GetWorldPosition().y;
    }

    const DirectX::XMFLOAT3 normalizedForward_ = Normalize({ forward_.x, 0.0f, forward_.z });
    const DirectX::XMFLOAT3 right_ = { normalizedForward_.z, 0.0f, -normalizedForward_.x };
    groundPosition_ = Add(groundPosition_, Scale(right_, runtime_.dangerAreaLocalOffset.x));
    groundPosition_ = Add(groundPosition_, Scale(normalizedForward_, runtime_.dangerAreaLocalOffset.z));
    groundPosition_.y += dangerAreaGroundOffset;

    runtime_.dangerAreaObject->transform->localPosition = groundPosition_;
    runtime_.dangerAreaObject->transform->localRotation = ResolveForwardRotation(normalizedForward_);
    runtime_.dangerAreaObject->transform->localScale = { 1.0f, 1.0f, 1.0f };
    runtime_.dangerAreaObject->transform->MarkDirty();
}

void DragonAttackControllerComponent::SpawnDangerArea(
    const std::string& objectName_,
    float lifeTime_,
    bool followMouth_,
    bool followTail_,
    const DirectX::XMFLOAT3& localOffset_,
    float radius_,
    float halfHeight_)
{
    if (!dangerAreaEnabled) return;

    RuntimeAttack runtime_{};
    runtime_.lifeTimer = lifeTime_;
    runtime_.followMouth = followMouth_;
    runtime_.followTail = followTail_;
    AttachDangerArea(runtime_, objectName_, localOffset_, radius_, halfHeight_);
    ApplyDangerAreaTransform(runtime_);
    if (runtime_.hasDangerArea)
    {
        runtimeAttacks.push_back(runtime_);
    }
}

void DragonAttackControllerComponent::AttachDangerArea(
    RuntimeAttack& runtime_,
    const std::string& objectName_,
    const DirectX::XMFLOAT3& localOffset_,
    float radius_,
    float halfHeight_)
{
    if (!dangerAreaEnabled || dangerAreaVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    std::shared_ptr<GameObject> dangerObject_ = owner_->GetManager().Create(objectName_);
    if (!dangerObject_) return;

    dangerObject_->SetNoSaveObject();
    std::shared_ptr<MeshVFXComponent> meshVFX_ = dangerObject_->AddComponent<MeshVFXComponent>();
    if (!meshVFX_ || !meshVFX_->LoadAsset(dangerAreaVFXAssetPath))
    {
        owner_->GetManager().Destroy(dangerObject_);
        return;
    }

    ConfigureDangerAreaVFX(meshVFX_, radius_, halfHeight_);
    meshVFX_->Reset();
    meshVFX_->Play();

    runtime_.dangerAreaObject = dangerObject_;
    runtime_.dangerAreaVFX = meshVFX_;
    runtime_.dangerAreaLocalOffset = localOffset_;
    runtime_.dangerAreaRadius = radius_;
    runtime_.dangerAreaHalfHeight = halfHeight_;
    runtime_.hasDangerArea = true;
}

void DragonAttackControllerComponent::ConfigureDangerAreaVFX(
    const std::shared_ptr<MeshVFXComponent>& meshVFX_,
    float radius_,
    float halfHeight_) const
{
    if (!meshVFX_) return;

    MeshVFXAsset asset_ = meshVFX_->GetAsset();
    const float safeRadius_ = (std::max)(radius_, 0.01f);
    const float safeLength_ = (std::max)(halfHeight_, 0.0f) * 2.0f;
    for (MeshVFXPart& part_ : asset_.parts)
    {
        // 表示範囲は攻撃判定のカプセル寸法から毎回作り直し、アセット側は見た目の調整に専念させる。
        part_.modelPath = DANGER_AREA_MODEL_PATH;
        part_.meshIndex = 0;
        part_.shapeCylinderRadius = safeRadius_;
        part_.internalMeshSettings.capsuleLength = safeLength_;
    }
    meshVFX_->SetAsset(asset_);
}

void DragonAttackControllerComponent::SpawnBreathVFX()
{
    if (breathVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock();
    if (!owner_ || !neckIK_) return;

    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    if (!neckIK_->GetMouthWorldPosition(position_) ||
        !neckIK_->GetMouthWorldForward(forward_))
    {
        return;
    }

    forward_ = Normalize(forward_);
    MeshVFXSpawnRequest request_{};
    request_.assetPath = breathVFXAssetPath;
    request_.position = Add(position_, Scale(forward_, 0.35f));
    request_.normal = forward_;
    request_.lifeTime = breathVFXLifeTime;
    request_.normalOffset = 0.0f;
    owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
}

void DragonAttackControllerComponent::SpawnBiteVFX()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnBiteVFX");

    if (biteVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock();
    if (!owner_ || !neckIK_) return;

    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    if (!neckIK_->GetMouthWorldPosition(position_) ||
        !neckIK_->GetMouthWorldForward(forward_))
    {
        return;
    }

    forward_ = Normalize(forward_);
    MeshVFXSpawnRequest request_{};
    request_.assetPath = biteVFXAssetPath;
    request_.position = Add(position_, Scale(forward_, biteForwardOffset));
    request_.normal = forward_;
    request_.lifeTime = biteVFXLifeTime;
    request_.normalOffset = 0.0f;
    owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
}

void DragonAttackControllerComponent::SpawnStampHit()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnStampHit");

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const DirectX::XMFLOAT3 ownerPosition_ = owner_->transform->GetWorldPosition();
    const Vector3 ownerForward_ = owner_->transform->GetFront();
    const DirectX::XMFLOAT3 forward_ = Normalize({ ownerForward_.x, 0.0f, ownerForward_.z });
    const DirectX::XMFLOAT3 centerPosition_ = Add(ownerPosition_, Scale(forward_, stampForwardOffset));

    std::shared_ptr<GameObject> attackObject_ = owner_->GetManager().Create("RuntimeDragonStampHit");
    if (!attackObject_) return;
    if (!attackObject_->transform)
    {
        owner_->GetManager().Destroy(attackObject_);
        return;
    }

    attackObject_->SetNoSaveObject();
    attackObject_->SetLayer(LayerType::EnemyBullet);
    attackObject_->transform->localPosition = centerPosition_;
    attackObject_->transform->localRotation = { 0.0f, 0.0f, 0.0f };
    attackObject_->transform->MarkDirty();

    std::shared_ptr<AttackEffectHitComponent> hitComponent_ =
        attackObject_->AddComponent<AttackEffectHitComponent>();
    if (!hitComponent_)
    {
        owner_->GetManager().Destroy(attackObject_);
        return;
    }

    AttackEffectHitShapeTemplate shape_{};
    shape_.hitBoxName = "RuntimeDragonStampHit_Sphere";
    shape_.shape = ATTACK_HIT_SHAPE::SPHERE;
    shape_.radius = (std::max)(stampRadius, 0.01f);
    shape_.activeStartTime = 0.0f;
    shape_.activeEndTime = stampLifeTime;
    shape_.hitInterval = 0.0f;
    shape_.allowMultiHit = false;

    const uint32_t attackInstanceId_ = CreateAttackInstanceId();
    hitComponent_->SetOwner(owner_.get());
    hitComponent_->SetAttackInstanceId(attackInstanceId_);
    hitComponent_->SetHitGroupId(attackInstanceId_);
    hitComponent_->SetEffectType(ATTACK_EFFECT_TYPE::INSTANT_AREA);
    hitComponent_->SetLifeTime(stampLifeTime);
    hitComponent_->SetDestroyOnLifeEnd(false);
    hitComponent_->SetDamage(stampDamage);
    hitComponent_->SetDamageRate(stampDamageRate);
    hitComponent_->SetFixedDamage(stampFixedDamage);
    hitComponent_->SetDamageApplyType(DAMAGE_APPLY_TYPE::SINGLE_HIT);
    hitComponent_->SetDamageHitInterval(0.0f);
    hitComponent_->SetMaxHitCount(1);
    hitComponent_->SetImpactPower(stampImpactPower);
    hitComponent_->SetKnockBackPower(stampKnockBackPower);
    hitComponent_->SetAllowMultiHit(false);
    hitComponent_->SetHitShapes({ shape_ });
    hitComponent_->Play();

    RuntimeAttack runtime_{};
    runtime_.object = attackObject_;
    runtime_.hitComponent = hitComponent_;
    runtime_.lifeTimer = stampLifeTime;
    if (showDangerAreaOnAttackStart)
    {
        AttachDangerArea(
            runtime_,
            "RuntimeDragonStampHit_DangerArea",
            { 0.0f, 0.0f, 0.0f },
            stampRadius,
            0.0f);
        ApplyDangerAreaTransform(runtime_);
    }
    runtimeAttacks.push_back(runtime_);

    SpawnStampRockVFX(centerPosition_);
}

void DragonAttackControllerComponent::SpawnStampRockVFX(const DirectX::XMFLOAT3& centerPosition_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnStampRockVFX");

    if (stampRockVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    const Vector3 ownerForward_ = owner_->transform->GetFront();
    const DirectX::XMFLOAT3 forward_ = Normalize({ ownerForward_.x, 0.0f, ownerForward_.z });
    const float baseYaw_ = std::atan2(forward_.x, forward_.z);
    const int rockCount_ = (std::max)(stampRockCount, 1);
    const float arcRadians_ = std::clamp(stampRockArcDegrees, 0.0f, 360.0f) * 3.1415926535f / 180.0f;
    const float startYaw_ = baseYaw_ - arcRadians_ * 0.5f;
    const float stepYaw_ = rockCount_ > 1 ? arcRadians_ / static_cast<float>(rockCount_ - 1) : 0.0f;

    // 着地点を中心に複数の岩VFXをずらして発生させ、地面から突き上げる攻撃の見た目を作る。
    for (int rockIndex_ = 0; rockIndex_ < rockCount_; ++rockIndex_)
    {
        const float yaw_ = startYaw_ + stepYaw_ * static_cast<float>(rockIndex_);
        const DirectX::XMFLOAT3 direction_ = { std::sin(yaw_), 0.0f, std::cos(yaw_) };
        MeshVFXSpawnRequest request_{};
        request_.assetPath = stampRockVFXAssetPath;
        request_.position = Add(centerPosition_, Scale(direction_, stampRockRadius));
        request_.normal = { 0.0f, 1.0f, 0.0f };
        request_.lifeTime = stampRockLifeTime;
        request_.normalOffset = 0.0f;
        owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
    }
}

void DragonAttackControllerComponent::StopRuntimeAttack(RuntimeAttack& runtime_)
{
    if (const std::shared_ptr<AttackEffectHitComponent> hitComponent_ = runtime_.hitComponent.lock())
    {
        hitComponent_->Stop();
    }
    if (runtime_.object)
    {
        runtime_.object->GetManager().Destroy(runtime_.object);
    }
    if (runtime_.dangerAreaObject)
    {
        runtime_.dangerAreaObject->GetManager().Destroy(runtime_.dangerAreaObject);
    }
    runtime_.object.reset();
    runtime_.dangerAreaObject.reset();
}

DirectX::XMFLOAT3 DragonAttackControllerComponent::ResolveForwardRotation(
    const DirectX::XMFLOAT3& forward_) const
{
    const DirectX::XMFLOAT3 normalized_ = Normalize(forward_);
    const float yaw_ = std::atan2(normalized_.x, normalized_.z) * RAD_TO_DEG;
    const float horizontalLength_ = std::sqrt(
        normalized_.x * normalized_.x +
        normalized_.z * normalized_.z);
    const float pitch_ = -std::atan2(normalized_.y, horizontalLength_) * RAD_TO_DEG;
    return { pitch_, yaw_, 0.0f };
}

uint32_t DragonAttackControllerComponent::CreateAttackInstanceId()
{
    return nextAttackInstanceId++;
}

void DragonAttackControllerComponent::Gui()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::Gui");

    ImGui::InputText("Action Category", &actionCategory);
    ImGui::InputText("Breath VFX", &breathVFXAssetPath);
    ImGui::InputText("Bite VFX", &biteVFXAssetPath);
    ImGui::SeparatorText("Danger Area");
    ImGui::Checkbox("Danger Area Enabled", &dangerAreaEnabled);
    ImGui::Checkbox("Show Danger Area On Attack Start", &showDangerAreaOnAttackStart);
    ImGui::InputText("Danger Area VFX", &dangerAreaVFXAssetPath);
    ImGui::DragFloat("Danger Area Ground Offset", &dangerAreaGroundOffset, 0.001f, -1.0f, 1.0f);
    ImGui::DragFloat("Danger Area Length Padding", &dangerAreaLengthPadding, 0.01f, 0.0f, 20.0f);
    ImGui::DragFloat("Breath Life", &breathLifeTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Breath VFX Life", &breathVFXLifeTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Breath VFX Interval", &breathVFXInterval, 0.01f, 0.01f, 1.0f);
    ImGui::DragFloat("Breath Damage", &breathDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Breath Damage Rate", &breathDamageRate, 0.01f, 0.0f);
    ImGui::DragFloat("Breath Fixed Damage", &breathFixedDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Breath Hit Interval", &breathHitInterval, 0.01f, 0.0f);
    ImGui::DragFloat("Breath Radius", &breathRadius, 0.01f, 0.01f);
    ImGui::DragFloat("Breath Half Height", &breathHalfHeight, 0.01f, 0.0f);
    ImGui::DragFloat("Breath Forward Offset", &breathForwardOffset, 0.01f, 0.0f);

    ImGui::SeparatorText("Bite");
    ImGui::DragFloat("Bite Life", &biteLifeTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Bite VFX Life", &biteVFXLifeTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Bite Damage", &biteDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Bite Damage Rate", &biteDamageRate, 0.01f, 0.0f);
    ImGui::DragFloat("Bite Fixed Damage", &biteFixedDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Bite Impact Power", &biteImpactPower, 0.1f, 0.0f);
    ImGui::DragFloat("Bite KnockBack Power", &biteKnockBackPower, 0.1f, 0.0f);
    ImGui::DragFloat("Bite Radius", &biteRadius, 0.01f, 0.01f);
    ImGui::DragFloat("Bite Half Height", &biteHalfHeight, 0.01f, 0.0f);
    ImGui::DragFloat("Bite Forward Offset", &biteForwardOffset, 0.01f, 0.0f);

    ImGui::SeparatorText("Tail");
    ImGui::DragFloat("Tail Sweep Life", &tailSweepLifeTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Spin Sweep Life", &spinSweepLifeTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Tail Damage", &tailDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Tail Damage Rate", &tailDamageRate, 0.01f, 0.0f);
    ImGui::DragFloat("Spin Damage", &spinDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Spin Damage Rate", &spinDamageRate, 0.01f, 0.0f);
    ImGui::DragFloat("Tail Fixed Damage", &tailFixedDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Tail Hit Interval", &tailHitInterval, 0.01f, 0.0f);
    ImGui::DragFloat("Tail Radius", &tailRadius, 0.01f, 0.01f);
    ImGui::DragFloat("Tail Half Height", &tailHalfHeight, 0.01f, 0.0f);

    ImGui::SeparatorText("Stamp");
    ImGui::InputText("Stamp Rock VFX", &stampRockVFXAssetPath);
    ImGui::DragFloat("Stamp Life", &stampLifeTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Stamp Danger Life", &stampDangerLifeTime, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Stamp Damage", &stampDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Stamp Damage Rate", &stampDamageRate, 0.01f, 0.0f);
    ImGui::DragFloat("Stamp Fixed Damage", &stampFixedDamage, 0.1f, 0.0f);
    ImGui::DragFloat("Stamp Impact Power", &stampImpactPower, 0.1f, 0.0f);
    ImGui::DragFloat("Stamp KnockBack Power", &stampKnockBackPower, 0.1f, 0.0f);
    ImGui::DragFloat("Stamp Radius", &stampRadius, 0.01f, 0.01f);
    ImGui::DragFloat("Stamp Forward Offset", &stampForwardOffset, 0.01f, -20.0f, 20.0f);
    ImGui::DragInt("Stamp Rock Count", &stampRockCount, 1.0f, 1, 32);
    ImGui::DragFloat("Stamp Rock Radius", &stampRockRadius, 0.01f, 0.0f, 30.0f);
    ImGui::DragFloat("Stamp Rock Arc", &stampRockArcDegrees, 1.0f, 0.0f, 360.0f);
    ImGui::DragFloat("Stamp Rock Life", &stampRockLifeTime, 0.01f, 0.0f, 10.0f);
    ImGui::Text("Runtime Attacks: %d", static_cast<int>(runtimeAttacks.size()));

    if (ImGui::Button("Play Breath")) PlayBreath();
    ImGui::SameLine();
    if (ImGui::Button("Bite")) PlayBite();
    ImGui::SameLine();
    if (ImGui::Button("Tail Sweep")) PlayTailSweep();
    ImGui::SameLine();
    if (ImGui::Button("Spin Sweep")) PlaySpinSweep();
    ImGui::SameLine();
    if (ImGui::Button("Stamp")) PlayStamp();
    if (ImGui::Button("Show Breath Danger")) ShowBreathDangerArea();
    ImGui::SameLine();
    if (ImGui::Button("Show Bite Danger")) ShowBiteDangerArea();
    ImGui::SameLine();
    if (ImGui::Button("Show Tail Danger")) ShowTailSweepDangerArea();
    ImGui::SameLine();
    if (ImGui::Button("Show Spin Danger")) ShowSpinSweepDangerArea();
    ImGui::SameLine();
    if (ImGui::Button("Show Stamp Danger")) ShowStampDangerArea();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) StopAttack();
}

void DragonAttackControllerComponent::Serialize(nlohmann::json& json_) const
{
    json_["ActionCategory"] = actionCategory;
    json_["BreathVFXAssetPath"] = breathVFXAssetPath;
    json_["BiteVFXAssetPath"] = biteVFXAssetPath;
    json_["DangerAreaVFXAssetPath"] = dangerAreaVFXAssetPath;
    json_["DangerAreaEnabled"] = dangerAreaEnabled;
    json_["ShowDangerAreaOnAttackStart"] = showDangerAreaOnAttackStart;
    json_["DangerAreaGroundOffset"] = dangerAreaGroundOffset;
    json_["DangerAreaLengthPadding"] = dangerAreaLengthPadding;
    json_["BreathLifeTime"] = breathLifeTime;
    json_["BreathVFXLifeTime"] = breathVFXLifeTime;
    json_["BreathVFXInterval"] = breathVFXInterval;
    json_["BreathDamage"] = breathDamage;
    json_["BreathDamageRate"] = breathDamageRate;
    json_["BreathFixedDamage"] = breathFixedDamage;
    json_["BreathHitInterval"] = breathHitInterval;
    json_["BreathImpactPower"] = breathImpactPower;
    json_["BreathKnockBackPower"] = breathKnockBackPower;
    json_["BreathRadius"] = breathRadius;
    json_["BreathHalfHeight"] = breathHalfHeight;
    json_["BreathForwardOffset"] = breathForwardOffset;
    json_["BiteLifeTime"] = biteLifeTime;
    json_["BiteVFXLifeTime"] = biteVFXLifeTime;
    json_["BiteDamage"] = biteDamage;
    json_["BiteDamageRate"] = biteDamageRate;
    json_["BiteFixedDamage"] = biteFixedDamage;
    json_["BiteImpactPower"] = biteImpactPower;
    json_["BiteKnockBackPower"] = biteKnockBackPower;
    json_["BiteRadius"] = biteRadius;
    json_["BiteHalfHeight"] = biteHalfHeight;
    json_["BiteForwardOffset"] = biteForwardOffset;
    json_["TailSweepLifeTime"] = tailSweepLifeTime;
    json_["SpinSweepLifeTime"] = spinSweepLifeTime;
    json_["TailDamage"] = tailDamage;
    json_["TailDamageRate"] = tailDamageRate;
    json_["SpinDamage"] = spinDamage;
    json_["SpinDamageRate"] = spinDamageRate;
    json_["TailFixedDamage"] = tailFixedDamage;
    json_["TailHitInterval"] = tailHitInterval;
    json_["TailImpactPower"] = tailImpactPower;
    json_["TailKnockBackPower"] = tailKnockBackPower;
    json_["TailRadius"] = tailRadius;
    json_["TailHalfHeight"] = tailHalfHeight;
    json_["StampRockVFXAssetPath"] = stampRockVFXAssetPath;
    json_["StampLifeTime"] = stampLifeTime;
    json_["StampDangerLifeTime"] = stampDangerLifeTime;
    json_["StampDamage"] = stampDamage;
    json_["StampDamageRate"] = stampDamageRate;
    json_["StampFixedDamage"] = stampFixedDamage;
    json_["StampImpactPower"] = stampImpactPower;
    json_["StampKnockBackPower"] = stampKnockBackPower;
    json_["StampRadius"] = stampRadius;
    json_["StampForwardOffset"] = stampForwardOffset;
    json_["StampRockCount"] = stampRockCount;
    json_["StampRockRadius"] = stampRockRadius;
    json_["StampRockArcDegrees"] = stampRockArcDegrees;
    json_["StampRockLifeTime"] = stampRockLifeTime;
}

void DragonAttackControllerComponent::Deserialize(const nlohmann::json& json_)
{
    actionCategory = json_.value("ActionCategory", std::string("DragonAttack"));
    breathVFXAssetPath = json_.value("BreathVFXAssetPath", std::string("Data/VFX/DragonBreath.json"));
    biteVFXAssetPath = json_.value("BiteVFXAssetPath", std::string("Data/VFX/DragonBite.json"));
    dangerAreaVFXAssetPath = json_.value("DangerAreaVFXAssetPath", std::string("Data/VFX/EnemyDangerArea.json"));
    dangerAreaEnabled = json_.value("DangerAreaEnabled", true);
    showDangerAreaOnAttackStart = json_.value("ShowDangerAreaOnAttackStart", true);
    dangerAreaGroundOffset = json_.value("DangerAreaGroundOffset", 0.03f);
    dangerAreaLengthPadding = json_.value("DangerAreaLengthPadding", 0.0f);
    breathLifeTime = json_.value("BreathLifeTime", 2.0f);
    breathVFXLifeTime = json_.value("BreathVFXLifeTime", 0.35f);
    breathVFXInterval = json_.value("BreathVFXInterval", 0.08f);
    breathDamage = json_.value("BreathDamage", 18.0f);
    breathDamageRate = json_.value("BreathDamageRate", 0.18f);
    breathFixedDamage = json_.value("BreathFixedDamage", 0.0f);
    breathHitInterval = json_.value("BreathHitInterval", 0.25f);
    breathImpactPower = json_.value("BreathImpactPower", 12.0f);
    breathKnockBackPower = json_.value("BreathKnockBackPower", 4.0f);
    breathRadius = json_.value("BreathRadius", 1.2f);
    breathHalfHeight = json_.value("BreathHalfHeight", 4.0f);
    breathForwardOffset = json_.value("BreathForwardOffset", 4.0f);
    biteLifeTime = json_.value("BiteLifeTime", 0.25f);
    biteVFXLifeTime = json_.value("BiteVFXLifeTime", 0.35f);
    biteDamage = json_.value("BiteDamage", 20.0f);
    biteDamageRate = json_.value("BiteDamageRate", 0.20f);
    biteFixedDamage = json_.value("BiteFixedDamage", 0.0f);
    biteImpactPower = json_.value("BiteImpactPower", 16.0f);
    biteKnockBackPower = json_.value("BiteKnockBackPower", 6.0f);
    biteRadius = json_.value("BiteRadius", 1.4f);
    biteHalfHeight = json_.value("BiteHalfHeight", 0.7f);
    biteForwardOffset = json_.value("BiteForwardOffset", 0.7f);
    tailSweepLifeTime = json_.value("TailSweepLifeTime", 1.2f);
    spinSweepLifeTime = json_.value("SpinSweepLifeTime", 1.8f);
    tailDamage = json_.value("TailDamage", 22.0f);
    tailDamageRate = json_.value("TailDamageRate", 0.22f);
    spinDamage = json_.value("SpinDamage", 30.0f);
    spinDamageRate = json_.value("SpinDamageRate", 0.30f);
    tailFixedDamage = json_.value("TailFixedDamage", 0.0f);
    tailHitInterval = json_.value("TailHitInterval", 0.35f);
    tailImpactPower = json_.value("TailImpactPower", 18.0f);
    tailKnockBackPower = json_.value("TailKnockBackPower", 8.0f);
    tailRadius = json_.value("TailRadius", 1.1f);
    tailHalfHeight = json_.value("TailHalfHeight", 1.8f);
    stampRockVFXAssetPath = json_.value("StampRockVFXAssetPath", std::string("Data/VFX/DragonStampRock.json"));
    stampLifeTime = json_.value("StampLifeTime", 0.35f);
    stampDangerLifeTime = json_.value("StampDangerLifeTime", 1.0f);
    stampDamage = json_.value("StampDamage", 28.0f);
    stampDamageRate = json_.value("StampDamageRate", 0.28f);
    stampFixedDamage = json_.value("StampFixedDamage", 0.0f);
    stampImpactPower = json_.value("StampImpactPower", 26.0f);
    stampKnockBackPower = json_.value("StampKnockBackPower", 12.0f);
    stampRadius = json_.value("StampRadius", 5.0f);
    stampForwardOffset = json_.value("StampForwardOffset", 1.5f);
    stampRockCount = json_.value("StampRockCount", 7);
    stampRockRadius = json_.value("StampRockRadius", 3.5f);
    stampRockArcDegrees = json_.value("StampRockArcDegrees", 180.0f);
    stampRockLifeTime = json_.value("StampRockLifeTime", 1.2f);

    RegisterActions();
}

REGISTER_COMPONENT(DragonAttackControllerComponent, "DragonAttackControllerComponent")
