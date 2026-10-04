#include "PrecompiledHeader.h"

#include "DragonAttackControllerComponent.h"

#include "Engine/GamePlay/Combat/AttackEffectHitComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/VFX/AttackEmitterComponent.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/Graphics/Resource/GlTFModel.h"
#include "Engine/Graphics/Resource/Animation/DragonNeckCommandIKComponent.h"
#include "Engine/Graphics/Resource/Animation/DragonTailCommandIKComponent.h"
#include "Engine/Graphics/VFX/MeshVFXComponent.h"
#include "Engine/Math/RandomHelper.h"
#include "Engine/Physics/ColliderComponent.h"
#include "Engine/Physics/PhysXSystem.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_stdlib.h>

namespace
{
    constexpr float RAD_TO_DEG = 180.0f / 3.1415926535f;
    constexpr float EPSILON = 0.00001f;
    constexpr const char* ACTION_PLAY_BREATH = "DragonAttack_PlayBreath";
    constexpr const char* ACTION_PLAY_BREATH_PROJECTILE_SINGLE = "DragonAttack_PlayBreathProjectileSingle";
    constexpr const char* ACTION_PLAY_BREATH_PROJECTILE_BEAM = "DragonAttack_PlayBreathProjectileBeam";
    constexpr const char* ACTION_PLAY_BREATH_PROJECTILE_FAN = "DragonAttack_PlayBreathProjectileFan";
    constexpr const char* ACTION_PLAY_BITE = "DragonAttack_PlayBite";
    constexpr const char* ACTION_PLAY_TAIL_SWEEP = "DragonAttack_PlayTailSweep";
    constexpr const char* ACTION_PLAY_SPIN_SWEEP = "DragonAttack_PlaySpinSweep";
    constexpr const char* ACTION_PLAY_STAMP = "DragonAttack_PlayStamp";
    constexpr const char* ACTION_SHOW_BREATH_DANGER_AREA = "DragonAttack_ShowBreathDangerArea";
    constexpr const char* ACTION_SHOW_BREATH_PROJECTILE_SINGLE_DANGER_AREA = "DragonAttack_ShowBreathProjectileSingleDangerArea";
    constexpr const char* ACTION_SHOW_BREATH_PROJECTILE_BEAM_DANGER_AREA = "DragonAttack_ShowBreathProjectileBeamDangerArea";
    constexpr const char* ACTION_SHOW_BREATH_PROJECTILE_FAN_DANGER_AREA = "DragonAttack_ShowBreathProjectileFanDangerArea";
    constexpr const char* ACTION_SHOW_BITE_DANGER_AREA = "DragonAttack_ShowBiteDangerArea";
    constexpr const char* ACTION_SHOW_TAIL_SWEEP_DANGER_AREA = "DragonAttack_ShowTailSweepDangerArea";
    constexpr const char* ACTION_SHOW_SPIN_SWEEP_DANGER_AREA = "DragonAttack_ShowSpinSweepDangerArea";
    constexpr const char* ACTION_SHOW_STAMP_DANGER_AREA = "DragonAttack_ShowStampDangerArea";
    constexpr const char* ACTION_STOP = "DragonAttack_Stop";
    constexpr const char* DANGER_AREA_MODEL_PATH = "__InternalMeshDangerArea__";
    constexpr const char* DANGER_FAN_MODEL_PATH = "__InternalMeshDangerFan__";
    constexpr float BREATH_BEAM_TARGET_AIM_BLEND = 1.0f;

    // 地面表示用のRayはステージの物理Colliderだけに当て、攻撃判定やキャラクターを除外する。
    class DangerAreaGroundQueryFilter : public physx::PxQueryFilterCallback
    {
    public:
        explicit DangerAreaGroundQueryFilter(const std::vector<const void*>& ignoredUserData_)
            : ignoredUserData(ignoredUserData_)
        {
            BS_PROFILE_SCOPE("DangerAreaGroundQueryFilter::DangerAreaGroundQueryFilter");
        }

        physx::PxQueryHitType::Enum preFilter(
            const physx::PxFilterData& filterData_,
            const physx::PxShape* shape_,
            const physx::PxRigidActor* actor_,
            physx::PxHitFlags& queryFlags_) override
        {
            BS_PROFILE_SCOPE("DangerAreaGroundQueryFilter::preFilter");
            (void)filterData_;
            (void)queryFlags_;
            if (!shape_ || !actor_ ||
                !shape_->getFlags().isSet(physx::PxShapeFlag::eSCENE_QUERY_SHAPE) ||
                shape_->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE))
            {
                return physx::PxQueryHitType::eNONE;
            }
            physx::PxFilterData shapeFilter_ = shape_->getQueryFilterData();
            if (shapeFilter_.word0 == 0u) shapeFilter_ = shape_->getSimulationFilterData();
            constexpr uint32_t GROUND_LAYERS =
                static_cast<uint32_t>(LayerType::Environment) | static_cast<uint32_t>(LayerType::Terrain);
            if ((shapeFilter_.word0 & GROUND_LAYERS) == 0u ||
                (shapeFilter_.word0 & ~GROUND_LAYERS) != 0u ||
                shapeFilter_.word2 != static_cast<uint32_t>(ColliderComponent::CollisionRole::PHYSICAL))
            {
                return physx::PxQueryHitType::eNONE;
            }
            // userDataはCollider以外を指すActorもあるため、型変換せず自己Colliderと比較する。
            if (std::find(ignoredUserData.begin(), ignoredUserData.end(), actor_->userData) != ignoredUserData.end())
            {
                return physx::PxQueryHitType::eNONE;
            }
            return physx::PxQueryHitType::eBLOCK;
        }

        physx::PxQueryHitType::Enum postFilter(
            const physx::PxFilterData& filterData_,
            const physx::PxQueryHit& hit_,
            const physx::PxShape* shape_,
            const physx::PxRigidActor* actor_) override
        {
            BS_PROFILE_SCOPE("DangerAreaGroundQueryFilter::postFilter");
            (void)filterData_;
            (void)shape_;
            (void)actor_;
            // 裏面やほぼ垂直な壁は地面にしない。
            const auto& rayHit_ = static_cast<const physx::PxRaycastHit&>(hit_);
            return rayHit_.normal.y >= 0.2f ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
        }

    private:
        const std::vector<const void*>& ignoredUserData;
    };

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

    float LerpFloat(float start_, float end_, float rate_)
    {
        return start_ + (end_ - start_) * rate_;
    }

    float StableRandom01(int seed_)
    {
        const float value_ = std::sin(static_cast<float>(seed_) * 12.9898f + 78.233f) * 43758.5453f;
        return value_ - std::floor(value_);
    }

    DirectX::XMFLOAT3 ExtractPosition(const DirectX::XMMATRIX& matrix_)
    {
        DirectX::XMFLOAT3 position_{};
        DirectX::XMStoreFloat3(&position_, matrix_.r[3]);
        return position_;
    }

    DirectX::XMFLOAT3 TransformForward(const DirectX::XMMATRIX& matrix_)
    {
        DirectX::XMFLOAT3 forward_{};
        DirectX::XMStoreFloat3(
            &forward_,
            DirectX::XMVector3TransformNormal(
                DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
                matrix_));
        return Normalize(forward_);
    }

    DirectX::XMFLOAT3 ApplyLocalOffsetToFrame(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMFLOAT3& forward_,
        const DirectX::XMFLOAT3& localOffset_)
    {
        const DirectX::XMFLOAT3 normalizedForward_ = Normalize(forward_);
        const DirectX::XMFLOAT3 horizontalForward_ = Normalize({ normalizedForward_.x, 0.0f, normalizedForward_.z });
        const DirectX::XMFLOAT3 right_{ horizontalForward_.z, 0.0f, -horizontalForward_.x };
        DirectX::XMFLOAT3 result_ = Add(position_, Scale(right_, localOffset_.x));
        result_ = Add(result_, Scale(normalizedForward_, localOffset_.z));
        result_.y += localOffset_.y;
        return result_;
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
    UpdatePendingStampRocks(deltaTime_);
}

void DragonAttackControllerComponent::PlayBreath()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayBreath");

    // 旧Action名はシーケンサー互換の入口として残し、実体はProjectileテンプレート生成へ寄せる。
    PlayBreathProjectileSingle();
}

void DragonAttackControllerComponent::PlayBreathProjectileSingle()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayBreathProjectileSingle");

    SpawnBreathProjectile("DragonBreathLightSingle");
}

void DragonAttackControllerComponent::PlayBreathProjectileBeam()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayBreathProjectileBeam");

    SpawnBreathProjectile("DragonBreathLightBeam", BREATH_BEAM_TARGET_AIM_BLEND);
}

void DragonAttackControllerComponent::PlayBreathProjectileFan()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::PlayBreathProjectileFan");

    SpawnBreathProjectile("DragonBreathLightFan");
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

    ShowBreathProjectileBeamDangerArea();
}

void DragonAttackControllerComponent::ShowBreathProjectileSingleDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowBreathProjectileSingleDangerArea");

    SpawnBreathChargeEffect();

    SpawnDangerArea(
        "RuntimeDragonBreathSingleDangerArea",
        breathDangerLifeTime,
        false,
        true,
        false,
        { 0.0f, 0.0f, breathProjectileImpactDangerRange },
        breathProjectileImpactDangerRadius,
        0.0f);
}

void DragonAttackControllerComponent::ShowBreathProjectileBeamDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowBreathProjectileBeamDangerArea");

    SpawnBreathChargeEffect();

    pendingBeamFrameValid = false;
    EnsureDependencies();
    DirectX::XMFLOAT3 position_{};
    DirectX::XMFLOAT3 forward_{};
    const auto emitter_ = ResolveBreathAttackEmitterComponent(position_, forward_, BREATH_BEAM_TARGET_AIM_BLEND);
    if (!emitter_) return;
    if (!breathProjectileEmitterAssetPath.empty() && !emitter_->LoadAsset(breathProjectileEmitterAssetPath)) return;
    forward_ = Normalize(forward_);
    DirectX::XMFLOAT3 start_{};
    DirectX::XMFLOAT3 end_{};
    float radius_ = 0.0f;
    if (!emitter_->ResolveBeamGeometryAt("DragonBreathLightBeam",
        Add(position_, Scale(forward_, breathProjectileMouthForwardOffset)), forward_, start_, end_, radius_)) return;

    // 判定の両端をXZへ投影する。俯角とTemplateの発射補正も予兆へ反映する。
    const float deltaX_ = end_.x - start_.x;
    const float deltaZ_ = end_.z - start_.z;
    const float horizontalLength_ = std::sqrt(deltaX_ * deltaX_ + deltaZ_ * deltaZ_);
    pendingBeamPosition = position_;
    pendingBeamForward = forward_;
    pendingBeamEmitter = emitter_;
    pendingBeamFrameTimer = (std::max)(breathDangerLifeTime, 0.0f);
    pendingBeamFrameValid = pendingBeamFrameTimer > 0.0f;
    RuntimeAttack runtime_{};
    runtime_.lifeTimer = breathDangerLifeTime;
    runtime_.dangerAreaFixedFrame = true;
    runtime_.dangerAreaFixedPosition = Scale(Add(start_, end_), 0.5f);
    runtime_.dangerAreaFixedForward = Normalize({ deltaX_, 0.0f, deltaZ_ });
    runtimeAttacks.erase(std::remove_if(runtimeAttacks.begin(), runtimeAttacks.end(),
        [this](RuntimeAttack& previous_)
        {
            if (!previous_.dangerAreaFixedFrame) return false;
            StopRuntimeAttack(previous_);
            return true;
        }), runtimeAttacks.end());
    AttachDangerArea(runtime_, "RuntimeDragonBreathDangerArea", { 0.0f, 0.0f, 0.0f },
        radius_, horizontalLength_ * 0.5f);
    ApplyDangerAreaTransform(runtime_);
    if (runtime_.hasDangerArea) runtimeAttacks.push_back(runtime_);
}

void DragonAttackControllerComponent::ShowBreathProjectileFanDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowBreathProjectileFanDangerArea");
    BS_PROFILE_PLOT("DragonAttack::FanDangerAreaObjectCount", 1.0f);

    SpawnBreathChargeEffect();

    // 扇型予兆は分割した円を大量生成せず、単一のFan内部メッシュで表示して生成スパイクを抑える。
    SpawnFanDangerArea(
        "RuntimeDragonBreathFanDangerArea",
        breathDangerLifeTime,
        true,
        breathFanDangerRange,
        breathFanDangerAngleDegrees);
}

void DragonAttackControllerComponent::ShowBiteDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowBiteDangerArea");

    SpawnDangerArea(
        "RuntimeDragonBiteDangerArea",
        biteLifeTime,
        true,
        false,
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
        false,
        true,
        { 0.0f, 0.0f, 0.0f },
        tailRadius,
        tailHalfHeight + dangerAreaLengthPadding);
}

void DragonAttackControllerComponent::ShowStampDangerArea()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ShowStampDangerArea");

    if (!dangerAreaEnabled) return;

    RuntimeAttack runtime_{};
    runtime_.lifeTimer = stampDangerLifeTime;
    runtime_.followStampSource = true;
    AttachDangerArea(
        runtime_,
        "RuntimeDragonStampDangerArea",
        { 0.0f, 0.0f, stampForwardOffset },
        stampRadius,
        0.0f);
    ApplyDangerAreaTransform(runtime_);
    if (runtime_.hasDangerArea)
    {
        runtimeAttacks.push_back(runtime_);
    }
}

void DragonAttackControllerComponent::StopAttack()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::StopAttack");
    pendingBeamFrameValid = false;
    pendingBeamFrameTimer = 0.0f;
    pendingBeamEmitter.reset();

    for (RuntimeAttack& runtime_ : runtimeAttacks)
    {
        StopRuntimeAttack(runtime_);
    }
    runtimeAttacks.clear();
    pendingStampRocks.clear();
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
    registerAction_(ACTION_PLAY_BREATH_PROJECTILE_SINGLE, [this]() { PlayBreathProjectileSingle(); });
    registerAction_(ACTION_PLAY_BREATH_PROJECTILE_BEAM, [this]() { PlayBreathProjectileBeam(); });
    registerAction_(ACTION_PLAY_BREATH_PROJECTILE_FAN, [this]() { PlayBreathProjectileFan(); });
    registerAction_(ACTION_PLAY_BITE, [this]() { PlayBite(); });
    registerAction_(ACTION_PLAY_TAIL_SWEEP, [this]() { PlayTailSweep(); });
    registerAction_(ACTION_PLAY_SPIN_SWEEP, [this]() { PlaySpinSweep(); });
    registerAction_(ACTION_PLAY_STAMP, [this]() { PlayStamp(); });
    registerAction_(ACTION_SHOW_BREATH_DANGER_AREA, [this]() { ShowBreathDangerArea(); });
    registerAction_(ACTION_SHOW_BREATH_PROJECTILE_SINGLE_DANGER_AREA, [this]() { ShowBreathProjectileSingleDangerArea(); });
    registerAction_(ACTION_SHOW_BREATH_PROJECTILE_BEAM_DANGER_AREA, [this]() { ShowBreathProjectileBeamDangerArea(); });
    registerAction_(ACTION_SHOW_BREATH_PROJECTILE_FAN_DANGER_AREA, [this]() { ShowBreathProjectileFanDangerArea(); });
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
    attackEmitter = owner_->GetComponent<AttackEmitterComponent>();
}

void DragonAttackControllerComponent::UpdateRuntimeAttacks(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::UpdateRuntimeAttacks");
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
    pendingBeamFrameTimer = (std::max)(pendingBeamFrameTimer - safeDeltaTime_, 0.0f);
    if (pendingBeamFrameTimer <= 0.0f) pendingBeamFrameValid = false;

    for (RuntimeAttack& runtime_ : runtimeAttacks)
    {
        runtime_.lifeTimer -= safeDeltaTime_;
        runtime_.dangerAreaGroundQueryTimer -= safeDeltaTime_;
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

    if (runtime_.followBreathSource)
    {
        resolved_ = ResolveBreathSourceFrame(position_, forward_);
    }
    else if (runtime_.followMouth)
    {
        resolved_ = ResolveMouthFrame(position_, forward_);
    }
    else if (runtime_.followStampSource)
    {
        resolved_ = ResolveStampSourceFrame(position_, forward_);
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

    runtime_.object->transform->localPosition =
        ApplyLocalOffsetToFrame(position_, forward_, runtime_.sourceLocalOffset);
    runtime_.object->transform->localRotation = ResolveForwardRotation(forward_);
    runtime_.object->transform->MarkDirty();
}

void DragonAttackControllerComponent::ApplyDangerAreaTransform(RuntimeAttack& runtime_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ApplyDangerAreaTransform");
    if (!runtime_.hasDangerArea || !runtime_.dangerAreaObject || !runtime_.dangerAreaObject->transform)
    {
        return;
    }

    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    bool resolved_ = false;

    if (runtime_.dangerAreaFixedFrame)
    {
        if (pendingBeamFrameValid)
        {
            const auto emitter_ = pendingBeamEmitter.lock();
            DirectX::XMFLOAT3 sourcePosition_{};
            DirectX::XMFLOAT3 ignoredForward_{};
            DirectX::XMFLOAT3 start_{};
            DirectX::XMFLOAT3 end_{};
            float radius_ = 0.0f;
            // 予兆中は口の位置だけ追従し、Targetが動いても確定した発射方向は変えない。
            if (emitter_ && ResolveBreathSourceFrame(sourcePosition_, ignoredForward_, BREATH_BEAM_TARGET_AIM_BLEND) &&
                emitter_->ResolveBeamGeometryAt("DragonBreathLightBeam",
                    Add(sourcePosition_, Scale(pendingBeamForward, breathProjectileMouthForwardOffset)),
                    pendingBeamForward, start_, end_, radius_))
            {
                pendingBeamPosition = sourcePosition_;
                runtime_.dangerAreaFixedPosition = Scale(Add(start_, end_), 0.5f);
            }
        }
        position_ = runtime_.dangerAreaFixedPosition;
        forward_ = runtime_.dangerAreaFixedForward;
        resolved_ = true;
    }
    else if (runtime_.followBreathSource)
    {
        resolved_ = ResolveBreathSourceFrame(position_, forward_);
    }
    else if (runtime_.followMouth)
    {
        resolved_ = ResolveMouthFrame(position_, forward_);
    }
    else if (runtime_.followStampSource)
    {
        resolved_ = ResolveStampSourceFrame(position_, forward_);
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
    DirectX::XMFLOAT3 groundNormal_{ 0.0f, 1.0f, 0.0f };
    float groundHeight_ = groundPosition_.y;
    ResolveDangerAreaGroundHeight(runtime_, groundPosition_, normalizedForward_,
        (std::max)(position_.y, groundPosition_.y), groundHeight_, groundNormal_);
    // 内部メッシュはY=0の平面なので厚み補正は不要。法線方向の隙間をYへ換算する。
    groundPosition_.y = groundHeight_ + (std::max)(dangerAreaGroundOffset, 0.001f) / groundNormal_.y;

    runtime_.dangerAreaObject->transform->localPosition = groundPosition_;
    runtime_.dangerAreaObject->transform->localRotation =
        ResolveDangerAreaGroundRotation(normalizedForward_, groundNormal_);
    runtime_.dangerAreaObject->transform->localScale = { 1.0f, 1.0f, 1.0f };
    runtime_.dangerAreaObject->transform->MarkDirty();
}

bool DragonAttackControllerComponent::ResolveDangerAreaGroundHeight(
    RuntimeAttack& runtime_,
    const DirectX::XMFLOAT3& position_,
    const DirectX::XMFLOAT3& forward_,
    float rayReferenceHeight_,
    float& height_,
    DirectX::XMFLOAT3& normal_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveDangerAreaGroundHeight");

    height_ = position_.y;
    normal_ = { 0.0f, 1.0f, 0.0f };
    const float deltaX_ = position_.x - runtime_.dangerAreaGroundSamplePosition.x;
    const float deltaZ_ = position_.z - runtime_.dangerAreaGroundSamplePosition.z;
    const float forwardDot_ = forward_.x * runtime_.dangerAreaGroundForward.x +
        forward_.z * runtime_.dangerAreaGroundForward.z;
    constexpr float CACHE_MOVE_DISTANCE = 0.1f;
    constexpr float CACHE_FORWARD_DOT = 0.9961947f; // 約5度以上の向き変更では即座に再取得する。
    if (runtime_.dangerAreaGroundCached && runtime_.dangerAreaGroundQueryTimer > 0.0f &&
        deltaX_ * deltaX_ + deltaZ_ * deltaZ_ <= CACHE_MOVE_DISTANCE * CACHE_MOVE_DISTANCE &&
        forwardDot_ >= CACHE_FORWARD_DOT)
    {
        if (runtime_.dangerAreaGroundResolved)
        {
            normal_ = runtime_.dangerAreaGroundNormal;
            // 同じ坂を少し移動した場合はキャッシュした平面上の高さを使う。
            height_ = runtime_.dangerAreaGroundHeight -
                (normal_.x * deltaX_ + normal_.z * deltaZ_) / normal_.y;
        }
        return runtime_.dangerAreaGroundResolved;
    }

    physx::PxScene* scene_ = PhysXSystem::GetScene();
    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    std::vector<const void*> ignoredUserData_;
    if (owner_)
    {
        for (const auto& collider_ : owner_->GetComponents<ColliderComponent>())
        {
            if (collider_) ignoredUserData_.push_back(collider_.get());
        }
    }
    DangerAreaGroundQueryFilter queryFilter_(ignoredUserData_);
    const physx::PxQueryFlags queryFlags_ = physx::PxQueryFlag::eSTATIC |
        physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::ePOSTFILTER;
    int rayCount_ = 0;
    auto queryGround_ = [&](const DirectX::XMFLOAT3& sample_, physx::PxRaycastHit& hit_)
        {
            if (!scene_) return false;
            ++rayCount_;
            physx::PxRaycastBuffer buffer_;
            const float rayStartY_ = (std::max)(rayReferenceHeight_, sample_.y) +
                (std::max)(dangerAreaRayStartHeight, 0.01f);
            if (!scene_->raycast(
                physx::PxVec3(sample_.x, rayStartY_, sample_.z),
                physx::PxVec3(0.0f, -1.0f, 0.0f),
                (std::max)(dangerAreaRayDistance, 0.01f), buffer_,
                physx::PxHitFlag::eDEFAULT | physx::PxHitFlag::eMESH_BOTH_SIDES,
                physx::PxQueryFilterData(physx::PxFilterData(), queryFlags_), &queryFilter_) || !buffer_.hasBlock)
            {
                return false;
            }
            hit_ = buffer_.block;
            return true;
        };

    physx::PxRaycastHit centerHit_;
    bool resolved_ = queryGround_(position_, centerHit_);
    if (resolved_)
    {
        height_ = centerHit_.position.y;
        // 直線ブレスは判定のXZ投影寸法を保つため、地面高さだけを補正する。
        if (dangerAreaAlignToGround && !runtime_.dangerAreaFixedFrame)
        {
            normal_ = Normalize({ centerHit_.normal.x, centerHit_.normal.y, centerHit_.normal.z });
        }
    }

    if (dangerAreaMultiSample && scene_)
    {
        const DirectX::XMFLOAT3 rotation_ = ResolveDangerAreaGroundRotation(forward_, normal_);
        const DirectX::XMMATRIX rotationMatrix_ = DirectX::XMMatrixRotationRollPitchYaw(
            DirectX::XMConvertToRadians(rotation_.x), DirectX::XMConvertToRadians(rotation_.y),
            DirectX::XMConvertToRadians(rotation_.z));
        const float baseHeight_ = height_;
        auto sampleOffset_ = [&](float x_, float z_)
            {
                DirectX::XMFLOAT3 offset_{};
                DirectX::XMStoreFloat3(&offset_, DirectX::XMVector3TransformNormal(
                    DirectX::XMVectorSet(x_, 0.0f, z_, 0.0f), rotationMatrix_));
                const DirectX::XMFLOAT3 sample_{
                    position_.x + offset_.x, baseHeight_ + offset_.y, position_.z + offset_.z };
                physx::PxRaycastHit hit_;
                if (!queryGround_(sample_, hit_)) return;
                // 傾斜した表示平面より高い接地点だけを補正し、段差での埋まりを防ぐ。
                const float requiredHeight_ = hit_.position.y - offset_.y;
                height_ = resolved_ ? (std::max)(height_, requiredHeight_) : requiredHeight_;
                resolved_ = true;
            };

        const float radius_ = (std::max)(runtime_.dangerAreaRadius, 0.01f);
        const float halfLength_ = (std::max)(runtime_.dangerAreaHalfHeight, 0.0f);
        if (runtime_.dangerAreaIsFan)
        {
            const float halfAngle_ = DirectX::XMConvertToRadians(runtime_.dangerAreaFanAngleDegrees * 0.5f);
            // 扇は原点に加え、中間半径と外周を左右端を含む5方向で確認する。
            for (int ring_ = 1; ring_ <= 2; ++ring_)
            {
                for (int index_ = 0; index_ < 5; ++index_)
                {
                    const float angle_ = -halfAngle_ + halfAngle_ * static_cast<float>(index_) * 0.5f;
                    const float distance_ = radius_ * static_cast<float>(ring_) * 0.5f;
                    sampleOffset_(std::sin(angle_) * distance_, std::cos(angle_) * distance_);
                }
            }
        }
        else if (halfLength_ > EPSILON)
        {
            // カプセルの始点・中心・終点で幅も確認し、丸い両端まで含める。
            for (int row_ = -1; row_ <= 1; ++row_)
            {
                for (int column_ = -1; column_ <= 1; ++column_)
                {
                    if (row_ == 0 && column_ == 0) continue;
                    sampleOffset_(radius_ * static_cast<float>(column_), halfLength_ * static_cast<float>(row_));
                }
            }
            sampleOffset_(0.0f, halfLength_ + radius_);
            sampleOffset_(0.0f, -halfLength_ - radius_);
        }
        else
        {
            for (int index_ = 0; index_ < 8; ++index_)
            {
                const float angle_ = DirectX::XM_2PI * static_cast<float>(index_) / 8.0f;
                sampleOffset_(std::sin(angle_) * radius_, std::cos(angle_) * radius_);
            }
        }
    }

    // ミスも短時間キャッシュするが、地面が無い場合の本体Yは呼び出しごとに更新する。
    runtime_.dangerAreaGroundCached = true;
    runtime_.dangerAreaGroundResolved = resolved_;
    runtime_.dangerAreaGroundQueryTimer = (std::max)(dangerAreaGroundQueryInterval, 0.0f);
    runtime_.dangerAreaGroundSamplePosition = position_;
    runtime_.dangerAreaGroundForward = forward_;
    runtime_.dangerAreaGroundHeight = height_;
    runtime_.dangerAreaGroundNormal = normal_;
    BS_PROFILE_PLOT("DragonAttack::DangerAreaGroundRayCount", rayCount_);
    return resolved_;
}

DirectX::XMFLOAT3 DragonAttackControllerComponent::ResolveDangerAreaGroundRotation(
    const DirectX::XMFLOAT3& forward_,
    const DirectX::XMFLOAT3& normal_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveDangerAreaGroundRotation");

    // XZ上の向きを維持した接線を作り、右・上・前の直交基底からTransform用回転へ変換する。
    const DirectX::XMFLOAT3 tangent_ = Normalize({ forward_.x,
        -(normal_.x * forward_.x + normal_.z * forward_.z) / normal_.y, forward_.z });
    const DirectX::XMVECTOR up_ = DirectX::XMLoadFloat3(&normal_);
    const DirectX::XMVECTOR front_ = DirectX::XMLoadFloat3(&tangent_);
    const DirectX::XMVECTOR right_ = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up_, front_));
    const DirectX::XMMATRIX basis_(right_, up_, front_, DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
    const DirectX::XMFLOAT3 angles_ = QuaternionToEuler(DirectX::XMQuaternionRotationMatrix(basis_));
    return { angles_.x * RAD_TO_DEG, angles_.y * RAD_TO_DEG, angles_.z * RAD_TO_DEG };
}

void DragonAttackControllerComponent::SpawnDangerArea(
    const std::string& objectName_,
    float lifeTime_,
    bool followMouth_,
    bool followBreathSource_,
    bool followTail_,
    const DirectX::XMFLOAT3& localOffset_,
    float radius_,
    float halfHeight_)
{
    if (!dangerAreaEnabled) return;

    RuntimeAttack runtime_{};
    runtime_.lifeTimer = lifeTime_;
    runtime_.followMouth = followMouth_;
    runtime_.followBreathSource = followBreathSource_;
    runtime_.followTail = followTail_;
    AttachDangerArea(runtime_, objectName_, localOffset_, radius_, halfHeight_);
    ApplyDangerAreaTransform(runtime_);
    if (runtime_.hasDangerArea)
    {
        runtimeAttacks.push_back(runtime_);
    }
}

void DragonAttackControllerComponent::SpawnFanDangerArea(
    const std::string& objectName_,
    float lifeTime_,
    bool followBreathSource_,
    float range_,
    float angleDegrees_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnFanDangerArea");

    if (!dangerAreaEnabled) return;

    RuntimeAttack runtime_{};
    runtime_.lifeTimer = lifeTime_;
    runtime_.followBreathSource = followBreathSource_;
    AttachDangerArea(
        runtime_,
        objectName_,
        { 0.0f, 0.0f, 0.0f },
        (std::max)(range_, 0.01f),
        0.0f,
        true,
        angleDegrees_);
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
    float halfHeight_,
    bool useFanShape_,
    float fanAngleDegrees_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::AttachDangerArea");

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

    ConfigureDangerAreaVFX(meshVFX_, radius_, halfHeight_, useFanShape_, fanAngleDegrees_);
    meshVFX_->Reset();
    meshVFX_->Play();

    runtime_.dangerAreaObject = dangerObject_;
    runtime_.dangerAreaVFX = meshVFX_;
    runtime_.dangerAreaLocalOffset = localOffset_;
    runtime_.dangerAreaRadius = radius_;
    runtime_.dangerAreaHalfHeight = halfHeight_;
    runtime_.dangerAreaIsFan = useFanShape_;
    runtime_.dangerAreaFanAngleDegrees = std::clamp(fanAngleDegrees_, 0.1f, 360.0f);
    runtime_.hasDangerArea = true;
}

void DragonAttackControllerComponent::ConfigureDangerAreaVFX(
    const std::shared_ptr<MeshVFXComponent>& meshVFX_,
    float radius_,
    float halfHeight_,
    bool useFanShape_,
    float fanAngleDegrees_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ConfigureDangerAreaVFX");

    if (!meshVFX_) return;

    MeshVFXAsset asset_ = meshVFX_->GetAsset();
    const float safeRadius_ = (std::max)(radius_, 0.01f);
    const float safeLength_ = (std::max)(halfHeight_, 0.0f) * 2.0f;
    for (MeshVFXPart& part_ : asset_.parts)
    {
        // 表示範囲は攻撃判定のカプセル寸法から毎回作り直し、アセット側は見た目の調整に専念させる。
        part_.modelPath = useFanShape_ ? DANGER_FAN_MODEL_PATH : DANGER_AREA_MODEL_PATH;
        part_.meshIndex = 0;
        part_.shapeCylinderRadius = safeRadius_;
        part_.internalMeshSettings.capsuleLength = useFanShape_ ? 0.0f : safeLength_;
        if (useFanShape_)
        {
            part_.internalMeshSettings.endAngleDegrees =
                std::clamp(fanAngleDegrees_, 0.1f, 360.0f);
        }
    }
    meshVFX_->SetAsset(asset_);
}

void DragonAttackControllerComponent::SpawnBreathVFX()
{
    if (breathVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    if (!ResolveBreathSourceFrame(position_, forward_)) return;

    forward_ = Normalize(forward_);
    MeshVFXSpawnRequest request_{};
    request_.assetPath = breathVFXAssetPath;
    request_.position = Add(position_, Scale(forward_, 0.35f));
    request_.normal = forward_;
    request_.lifeTime = breathVFXLifeTime;
    request_.normalOffset = 0.0f;
    owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
}

void DragonAttackControllerComponent::SpawnBreathChargeEffect()
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnBreathChargeEffect");

    if (!showBreathChargeOnDanger || breathChargeEffectTemplateName.empty()) return;

    EnsureDependencies();
    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    const std::shared_ptr<AttackEmitterComponent> emitter_ =
        ResolveBreathAttackEmitterComponent(position_, forward_);
    if (!emitter_) return;

    if (!breathProjectileEmitterAssetPath.empty())
    {
        emitter_->LoadAsset(breathProjectileEmitterAssetPath);
    }

    // ブレス予告と同時に、実際の口元位置へ溜め炎を発生させる。
    forward_ = Normalize(forward_);
    position_ = Add(position_, Scale(forward_, breathProjectileMouthForwardOffset));
    emitter_->EmitByNameAt(breathChargeEffectTemplateName, position_, forward_);
}

bool DragonAttackControllerComponent::ResolveActualMouthFrame(
    DirectX::XMFLOAT3& position_,
    DirectX::XMFLOAT3& forward_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveActualMouthFrame");

    const std::shared_ptr<DragonNeckCommandIKComponent> neckIK_ = neckIK.lock();
    if (neckIK_ &&
        neckIK_->GetMouthWorldPosition(position_) &&
        neckIK_->GetMouthWorldForward(forward_))
    {
        forward_ = Normalize(forward_);
        return true;
    }

    return false;
}

bool DragonAttackControllerComponent::ResolveMouthFrame(
    DirectX::XMFLOAT3& position_,
    DirectX::XMFLOAT3& forward_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return false;

    if (ResolveActualMouthFrame(position_, forward_))
    {
        return true;
    }

    // Mouthノードが未設定の試作データでも、原点ではなく頭付近から発生して見えるようにする。
    position_ = owner_->transform->GetWorldPosition();
    position_.y += 1.8f;
    const Vector3 ownerForward_ = owner_->transform->GetFront();
    forward_ = Normalize({ ownerForward_.x, ownerForward_.y, ownerForward_.z });
    return true;
}

bool DragonAttackControllerComponent::ResolveBreathSourceFrame(
    DirectX::XMFLOAT3& position_,
    DirectX::XMFLOAT3& forward_,
    float targetAimBlendOverride_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return false;

    DirectX::XMFLOAT3 baseForward_{ 0.0f, 0.0f, 1.0f };
    if (!ResolveBreathBaseForward(baseForward_)) return false;

    DirectX::XMFLOAT3 ignoredNodeForward_{ 0.0f, 0.0f, 1.0f };
    if (ResolveActualMouthFrame(position_, ignoredNodeForward_))
    {
        ResolveBreathAimForward(position_, baseForward_, forward_, targetAimBlendOverride_);
        return true;
    }

    if (ResolveNamedObjectFrame(breathEmitterObjectName, position_, ignoredNodeForward_))
    {
        ResolveBreathAimForward(position_, baseForward_, forward_, targetAimBlendOverride_);
        return true;
    }

    if (!breathSourceNodeName.empty())
    {
        const std::shared_ptr<GltfModelCommponennt> model_ = owner_->GetComponent<GltfModelCommponennt>();
        if (model_ && model_->IsModelReady())
        {
            const int nodeIndex_ = model_->GetNodeIndex(breathSourceNodeName.c_str());
            if (nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(model_->GetNodes().size()))
            {
                // ブレスはモデルノードの回転軸ではなく、本体の水平前方を向きの基準にする。
                const DirectX::XMMATRIX nodeMatrix_ =
                    DirectX::XMLoadFloat4x4(&model_->GetNodes()[nodeIndex_].worldTransform);
                position_ = ExtractPosition(nodeMatrix_);
                ResolveBreathAimForward(position_, baseForward_, forward_, targetAimBlendOverride_);
                return true;
            }
        }
    }

    if (ResolveMouthFrame(position_, ignoredNodeForward_))
    {
        ResolveBreathAimForward(position_, baseForward_, forward_, targetAimBlendOverride_);
        return true;
    }

    return false;
}

bool DragonAttackControllerComponent::ResolveBreathBaseForward(
    DirectX::XMFLOAT3& forward_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveBreathBaseForward");

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return false;

    // ビーム用モデルは本体の水平前方を基準に作られているため、Mouthノードの局所回転は使わない。
    const Vector3 ownerForward_ = owner_->transform->GetFront();
    forward_ = Normalize({ ownerForward_.x, 0.0f, ownerForward_.z });
    return true;
}

bool DragonAttackControllerComponent::ResolveBreathAimForward(
    const DirectX::XMFLOAT3& sourcePosition_,
    const DirectX::XMFLOAT3& fallbackForward_,
    DirectX::XMFLOAT3& forward_,
    float targetAimBlendOverride_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveBreathAimForward");

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_) return false;

    const std::shared_ptr<GameObject> targetObject_ = owner_->GetManager().Find(targetObjectName);
    if (!targetObject_ || !targetObject_->transform)
    {
        forward_ = Normalize(fallbackForward_);
        return true;
    }

    DirectX::XMFLOAT3 targetPosition_ = targetObject_->transform->GetWorldPosition();
    if (sourcePosition_.y > targetPosition_.y + 2.0f)
    {
        targetPosition_.y += breathAirGroundAimHeight;
    }
    else
    {
        targetPosition_.y += 1.0f;
    }

    const DirectX::XMFLOAT3 targetForward_ = Normalize({
        targetPosition_.x - sourcePosition_.x,
        targetPosition_.y - sourcePosition_.y,
        targetPosition_.z - sourcePosition_.z
    });
    // 直線ビームなど、攻撃ごとに本体前方よりターゲット方向を優先したい場合だけ上書きする。
    const float sourceBlend_ = targetAimBlendOverride_ >= 0.0f
        ? targetAimBlendOverride_
        : breathTargetAimBlend;
    const float blend_ = (std::min)((std::max)(sourceBlend_, 0.0f), 1.0f);
    forward_ = Normalize({
        fallbackForward_.x * (1.0f - blend_) + targetForward_.x * blend_,
        fallbackForward_.y * (1.0f - blend_) + targetForward_.y * blend_,
        fallbackForward_.z * (1.0f - blend_) + targetForward_.z * blend_
    });
    return true;
}

bool DragonAttackControllerComponent::ResolveNamedObjectFrame(
    const std::string& objectName_,
    DirectX::XMFLOAT3& position_,
    DirectX::XMFLOAT3& forward_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveNamedObjectFrame");

    if (objectName_.empty()) return false;

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_) return false;

    const std::shared_ptr<GameObject> sourceObject_ = owner_->GetManager().Find(objectName_);
    if (!sourceObject_ || !sourceObject_->transform) return false;

    // 部位ObjectがModelAttachmentで更新された後のTransformを、攻撃発生位置として使う。
    position_ = sourceObject_->transform->GetWorldPosition();
    const Vector3 sourceForward_ = sourceObject_->transform->GetFront();
    forward_ = Normalize({ sourceForward_.x, sourceForward_.y, sourceForward_.z });
    return true;
}

bool DragonAttackControllerComponent::ResolveStampSourceFrame(
    DirectX::XMFLOAT3& position_,
    DirectX::XMFLOAT3& forward_) const
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::ResolveStampSourceFrame");

    if (ResolveNamedObjectFrame(stampSourceObjectName, position_, forward_))
    {
        forward_ = Normalize({ forward_.x, 0.0f, forward_.z });
        return true;
    }

    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return false;

    if (!stampSourceNodeName.empty())
    {
        const std::shared_ptr<GltfModelCommponennt> model_ = owner_->GetComponent<GltfModelCommponennt>();
        if (model_ && model_->IsModelReady())
        {
            const int nodeIndex_ = model_->GetNodeIndex(stampSourceNodeName.c_str());
            if (nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(model_->GetNodes().size()))
            {
                // 明示的にノード名が指定された場合だけ、部位ノードをスタンプ基準にする。
                const DirectX::XMMATRIX nodeMatrix_ =
                    DirectX::XMLoadFloat4x4(&model_->GetNodes()[nodeIndex_].worldTransform);
                position_ = ExtractPosition(nodeMatrix_);
                const DirectX::XMFLOAT3 nodeForward_ = TransformForward(nodeMatrix_);
                forward_ = Normalize({ nodeForward_.x, 0.0f, nodeForward_.z });
                return true;
            }
        }
    }

    position_ = owner_->transform->GetWorldPosition();
    const Vector3 ownerForward_ = owner_->transform->GetFront();
    forward_ = Normalize({ ownerForward_.x, 0.0f, ownerForward_.z });
    return true;
}

std::shared_ptr<AttackEmitterComponent> DragonAttackControllerComponent::ResolveBreathAttackEmitterComponent(
    DirectX::XMFLOAT3& position_,
    DirectX::XMFLOAT3& forward_,
    float targetAimBlendOverride_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_) return nullptr;

    bool hasSourceFrame_ = false;
    std::shared_ptr<AttackEmitterComponent> sourceEmitter_;
    if (!breathEmitterObjectName.empty())
    {
        const std::shared_ptr<GameObject> sourceObject_ = owner_->GetManager().Find(breathEmitterObjectName);
        if (sourceObject_)
        {
            hasSourceFrame_ = ResolveBreathSourceFrame(position_, forward_, targetAimBlendOverride_);
            sourceEmitter_ = sourceObject_->GetComponent<AttackEmitterComponent>();
        }
    }

    if (sourceEmitter_)
    {
        if (hasSourceFrame_ || ResolveBreathSourceFrame(position_, forward_, targetAimBlendOverride_))
        {
            return sourceEmitter_;
        }
    }

    std::shared_ptr<AttackEmitterComponent> ownerEmitter_ = attackEmitter.lock();
    if (ownerEmitter_ && (hasSourceFrame_ || ResolveBreathSourceFrame(position_, forward_, targetAimBlendOverride_)))
    {
        return ownerEmitter_;
    }

    return nullptr;
}

void DragonAttackControllerComponent::SpawnBreathProjectile(
    const std::string& templateName_,
    float targetAimBlendOverride_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnBreathProjectile");

    EnsureDependencies();
    DirectX::XMFLOAT3 position_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 forward_{ 0.0f, 0.0f, 1.0f };
    const bool isBeam_ = templateName_ == "DragonBreathLightBeam";
    std::shared_ptr<AttackEmitterComponent> emitter_;
    if (isBeam_ && pendingBeamFrameValid)
    {
        emitter_ = pendingBeamEmitter.lock();
        position_ = pendingBeamPosition;
        forward_ = pendingBeamForward;
        for (RuntimeAttack& runtime_ : runtimeAttacks)
        {
            if (runtime_.dangerAreaFixedFrame) ApplyDangerAreaTransform(runtime_);
        }
        position_ = pendingBeamPosition;
    }
    if (!emitter_)
    {
        emitter_ = ResolveBreathAttackEmitterComponent(position_, forward_, targetAimBlendOverride_);
    }
    if (!emitter_) return;

    if (!breathProjectileEmitterAssetPath.empty())
    {
        if (!emitter_->LoadAsset(breathProjectileEmitterAssetPath)) return;
    }

    // ブレス本体はAttackEmitter Asset内の判定・モデル・着弾演出をまとめて生成する。
    forward_ = Normalize(forward_);
    position_ = Add(position_, Scale(forward_, breathProjectileMouthForwardOffset));
    emitter_->EmitByNameAt(templateName_, position_, forward_);
    if (isBeam_)
    {
        pendingBeamFrameValid = false;
        pendingBeamEmitter.reset();
        // 発射後も予兆を残す場合は、実際の判定寿命と同じ時刻に消す。
        for (const auto& template_ : emitter_->GetTemplates())
        {
            if (template_.name != templateName_) continue;
            for (RuntimeAttack& runtime_ : runtimeAttacks)
            {
                if (runtime_.dangerAreaFixedFrame) runtime_.lifeTimer = template_.lifeTime;
            }
            break;
        }
    }
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

    DirectX::XMFLOAT3 sourcePosition_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 sourceForward_{ 0.0f, 0.0f, 1.0f };
    if (!ResolveStampSourceFrame(sourcePosition_, sourceForward_)) return;

    const DirectX::XMFLOAT3 forward_ = Normalize({ sourceForward_.x, 0.0f, sourceForward_.z });
    const DirectX::XMFLOAT3 stampLocalOffset_{ 0.0f, 0.0f, stampForwardOffset };
    const DirectX::XMFLOAT3 centerPosition_ =
        ApplyLocalOffsetToFrame(sourcePosition_, forward_, stampLocalOffset_);

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
    runtime_.followStampSource = true;
    runtime_.sourceLocalOffset = stampLocalOffset_;
    if (showDangerAreaOnAttackStart)
    {
        AttachDangerArea(
            runtime_,
            "RuntimeDragonStampHit_DangerArea",
            stampLocalOffset_,
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

    DirectX::XMFLOAT3 sourcePosition_{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 sourceForward_{ 0.0f, 0.0f, 1.0f };
    const DirectX::XMFLOAT3 forward_ = ResolveStampSourceFrame(sourcePosition_, sourceForward_)
        ? Normalize({ sourceForward_.x, 0.0f, sourceForward_.z })
        : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };
    const float baseYaw_ = std::atan2(forward_.x, forward_.z);
    const int rockCount_ = (std::max)(stampRockCount, 1);
    const float arcRadians_ = std::clamp(stampRockArcDegrees, 0.0f, 360.0f) * 3.1415926535f / 180.0f;
    const float startYaw_ = baseYaw_ - arcRadians_ * 0.5f;
    const bool isFullCircle_ = std::abs(arcRadians_ - 3.1415926535f * 2.0f) <= 0.001f;
    const float stepYaw_ = rockCount_ > 1
        ? arcRadians_ / static_cast<float>(isFullCircle_ ? rockCount_ : rockCount_ - 1)
        : 0.0f;

    // 着地点を中心に複数グループへ分けて予約し、塊ごとの突き上げで重さを出す。
    const int groupCount_ = (std::max)(stampRockGroupCount, 1);
    const int rocksPerGroup_ = (std::max)(1, (rockCount_ + groupCount_ - 1) / groupCount_);
    const float safeScaleMin_ = (std::max)(0.01f, (std::min)(stampRockScaleMin, stampRockScaleMax));
    const float safeScaleMax_ = (std::max)(safeScaleMin_, (std::max)(stampRockScaleMin, stampRockScaleMax));
    for (int rockIndex_ = 0; rockIndex_ < rockCount_; ++rockIndex_)
    {
        const float angleJitter_ = (StableRandom01(rockIndex_ * 17 + 3) - 0.5f) * stepYaw_ * 0.35f;
        const float yaw_ = startYaw_ + stepYaw_ * static_cast<float>(rockIndex_) + angleJitter_;
        const int groupIndex_ = rockIndex_ / rocksPerGroup_;
        const float distanceJitter_ =
            (StableRandom01(rockIndex_ * 23 + 11) - 0.5f) * stampRockRadiusJitter;
        PendingStampRock pendingRock_{};
        pendingRock_.centerPosition = centerPosition_;
        pendingRock_.direction = { std::sin(yaw_), 0.0f, std::cos(yaw_) };
        pendingRock_.distance = (std::max)(0.0f, stampRockRadius + distanceJitter_);
        pendingRock_.scale = LerpFloat(
            safeScaleMin_,
            safeScaleMax_,
            StableRandom01(rockIndex_ * 31 + 7));
        pendingRock_.verticalOffset = stampRockRiseStartOffset *
            LerpFloat(0.7f, 1.15f, StableRandom01(rockIndex_ * 43 + 5));
        pendingRock_.timer =
            (std::max)(stampRockGroupInterval, 0.0f) * static_cast<float>(groupIndex_) +
            (std::max)(stampRockSpawnInterval, 0.0f) *
            static_cast<float>(rockIndex_ - groupIndex_ * rocksPerGroup_);
        pendingStampRocks.push_back(pendingRock_);
    }
}

void DragonAttackControllerComponent::SpawnStampRockVFXAt(
    const PendingStampRock& pendingRock_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::SpawnStampRockVFXAt");

    if (stampRockVFXAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_) return;

    MeshVFXSpawnRequest request_{};
    request_.assetPath = stampRockVFXAssetPath;
    request_.position = Add(
        pendingRock_.centerPosition,
        Scale(pendingRock_.direction, pendingRock_.distance));
    request_.position.y += pendingRock_.verticalOffset;
    request_.normal = { 0.0f, 1.0f, 0.0f };
    request_.lifeTime = stampRockLifeTime;
    request_.normalOffset = 0.0f;
    std::shared_ptr<GameObject> rockObject_ =
        owner_->GetManager().GetEffectManager().SpawnMeshVFX(request_);
    if (rockObject_ && rockObject_->transform)
    {
        rockObject_->transform->localScale =
        {
            pendingRock_.scale,
            pendingRock_.scale,
            pendingRock_.scale
        };
        rockObject_->transform->MarkDirty();
    }
}

void DragonAttackControllerComponent::UpdatePendingStampRocks(float deltaTime_)
{
    BS_PROFILE_SCOPE("DragonAttackControllerComponent::UpdatePendingStampRocks");

    for (auto iterator_ = pendingStampRocks.begin(); iterator_ != pendingStampRocks.end();)
    {
        iterator_->timer -= deltaTime_;
        if (iterator_->timer <= 0.0f)
        {
            SpawnStampRockVFXAt(*iterator_);
            iterator_ = pendingStampRocks.erase(iterator_);
            continue;
        }
        ++iterator_;
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
    ImGui::InputText("Breath Projectile AttackEmitter", &breathProjectileEmitterAssetPath);
    ImGui::InputText("Breath Charge Template", &breathChargeEffectTemplateName);
    ImGui::InputText("Breath Target Object", &targetObjectName);
    ImGui::InputText("Breath Emitter Object", &breathEmitterObjectName);
    ImGui::InputText("Breath Source Node Fallback", &breathSourceNodeName);
    ImGui::InputText("Bite VFX", &biteVFXAssetPath);
    ImGui::SeparatorText("Danger Area");
    ImGui::Checkbox("Danger Area Enabled", &dangerAreaEnabled);
    ImGui::Checkbox("Show Danger Area On Attack Start", &showDangerAreaOnAttackStart);
    ImGui::Checkbox("Show Breath Charge On Danger", &showBreathChargeOnDanger);
    ImGui::InputText("Danger Area VFX", &dangerAreaVFXAssetPath);
    ImGui::DragFloat("Danger Area Ground Offset", &dangerAreaGroundOffset, 0.001f, 0.001f, 1.0f);
    ImGui::DragFloat("Danger Area Ray Start Height", &dangerAreaRayStartHeight, 0.1f, 0.01f, 1000.0f);
    ImGui::DragFloat("Danger Area Ray Distance", &dangerAreaRayDistance, 1.0f, 0.01f, 10000.0f);
    ImGui::DragFloat("Danger Area Ground Query Interval", &dangerAreaGroundQueryInterval, 0.001f, 0.0f, 1.0f);
    ImGui::Checkbox("Danger Area Align To Ground", &dangerAreaAlignToGround);
    ImGui::Checkbox("Danger Area Multi Sample", &dangerAreaMultiSample);
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
    ImGui::DragFloat("Breath Projectile Source Offset", &breathProjectileMouthForwardOffset, 0.01f, 0.0f, 5.0f);
    ImGui::DragFloat("Breath Target Aim Blend", &breathTargetAimBlend, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Breath Air Ground Aim Height", &breathAirGroundAimHeight, 0.01f, -5.0f, 5.0f);
    ImGui::DragFloat("Breath Danger Life", &breathDangerLifeTime, 0.01f, 0.0f, 10.0f);
    // 旧予兆寸法は保存互換用として残し、直線ブレスの寸法はEmitter側で編集する。
    ImGui::BeginDisabled();
    ImGui::DragFloat("Legacy Breath Danger Range", &breathDangerRange, 0.01f, 0.01f, 80.0f);
    ImGui::DragFloat("Legacy Breath Danger Width", &breathDangerWidth, 0.01f, 0.01f, 20.0f);
    ImGui::DragFloat("Legacy Breath Danger Start Offset", &breathDangerStartOffset, 0.01f, -10.0f, 10.0f);
    ImGui::EndDisabled();
    ImGui::DragFloat("Breath Single Impact Range", &breathProjectileImpactDangerRange, 0.01f, 0.01f, 80.0f);
    ImGui::DragFloat("Breath Single Impact Radius", &breathProjectileImpactDangerRadius, 0.01f, 0.01f, 20.0f);
    ImGui::DragFloat("Breath Fan Danger Range", &breathFanDangerRange, 0.01f, 0.01f, 80.0f);
    ImGui::DragFloat("Breath Fan Danger Angle", &breathFanDangerAngleDegrees, 0.5f, 0.0f, 180.0f);
    ImGui::DragFloat("Breath Fan Danger Radius", &breathFanDangerRadius, 0.01f, 0.01f, 20.0f);
    ImGui::DragInt("Breath Fan Danger Samples", &breathFanDangerSampleCount, 1.0f, 1, 31);
    ImGui::DragInt("Breath Fan Danger Steps", &breathFanDangerRangeSteps, 1.0f, 1, 16);

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
    ImGui::InputText("Stamp Source Object", &stampSourceObjectName);
    ImGui::InputText("Stamp Source Node", &stampSourceNodeName);
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
    ImGui::DragFloat("Stamp Rock Interval", &stampRockSpawnInterval, 0.001f, 0.0f, 2.0f);
    ImGui::DragFloat("Stamp Rock Life", &stampRockLifeTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragInt("Stamp Rock Groups", &stampRockGroupCount, 1.0f, 1, 16);
    ImGui::DragFloat("Stamp Rock Group Interval", &stampRockGroupInterval, 0.001f, 0.0f, 2.0f);
    ImGui::DragFloat("Stamp Rock Scale Min", &stampRockScaleMin, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat("Stamp Rock Scale Max", &stampRockScaleMax, 0.01f, 0.01f, 5.0f);
    ImGui::DragFloat("Stamp Rock Radius Jitter", &stampRockRadiusJitter, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Stamp Rock Rise Start Offset", &stampRockRiseStartOffset, 0.01f, -5.0f, 1.0f);
    ImGui::Text("Runtime Attacks: %d", static_cast<int>(runtimeAttacks.size()));

    if (ImGui::Button("Play Breath")) PlayBreath();
    ImGui::SameLine();
    if (ImGui::Button("Breath Beam")) PlayBreathProjectileBeam();
    ImGui::SameLine();
    if (ImGui::Button("Breath Fan")) PlayBreathProjectileFan();
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
    if (ImGui::Button("Show Breath Single Danger")) ShowBreathProjectileSingleDangerArea();
    ImGui::SameLine();
    if (ImGui::Button("Show Breath Fan Danger")) ShowBreathProjectileFanDangerArea();
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
    json_["BreathProjectileEmitterAssetPath"] = breathProjectileEmitterAssetPath;
    json_["BreathChargeEffectTemplateName"] = breathChargeEffectTemplateName;
    json_["TargetObjectName"] = targetObjectName;
    json_["BreathEmitterObjectName"] = breathEmitterObjectName;
    json_["BreathSourceNodeName"] = breathSourceNodeName;
    json_["BiteVFXAssetPath"] = biteVFXAssetPath;
    json_["DangerAreaVFXAssetPath"] = dangerAreaVFXAssetPath;
    json_["DangerAreaEnabled"] = dangerAreaEnabled;
    json_["ShowDangerAreaOnAttackStart"] = showDangerAreaOnAttackStart;
    json_["ShowBreathChargeOnDanger"] = showBreathChargeOnDanger;
    json_["DangerAreaGroundOffset"] = dangerAreaGroundOffset;
    json_["DangerAreaRayStartHeight"] = dangerAreaRayStartHeight;
    json_["DangerAreaRayDistance"] = dangerAreaRayDistance;
    json_["DangerAreaGroundQueryInterval"] = dangerAreaGroundQueryInterval;
    json_["DangerAreaAlignToGround"] = dangerAreaAlignToGround;
    json_["DangerAreaMultiSample"] = dangerAreaMultiSample;
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
    json_["BreathProjectileMouthForwardOffset"] = breathProjectileMouthForwardOffset;
    json_["BreathTargetAimBlend"] = breathTargetAimBlend;
    json_["BreathAirGroundAimHeight"] = breathAirGroundAimHeight;
    json_["BreathDangerLifeTime"] = breathDangerLifeTime;
    json_["BreathDangerRange"] = breathDangerRange;
    json_["BreathDangerWidth"] = breathDangerWidth;
    json_["BreathDangerStartOffset"] = breathDangerStartOffset;
    json_["BreathProjectileImpactDangerRange"] = breathProjectileImpactDangerRange;
    json_["BreathProjectileImpactDangerRadius"] = breathProjectileImpactDangerRadius;
    json_["BreathFanDangerRange"] = breathFanDangerRange;
    json_["BreathFanDangerAngleDegrees"] = breathFanDangerAngleDegrees;
    json_["BreathFanDangerRadius"] = breathFanDangerRadius;
    json_["BreathFanDangerSampleCount"] = breathFanDangerSampleCount;
    json_["BreathFanDangerRangeSteps"] = breathFanDangerRangeSteps;
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
    json_["StampSourceObjectName"] = stampSourceObjectName;
    json_["StampSourceNodeName"] = stampSourceNodeName;
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
    json_["StampRockSpawnInterval"] = stampRockSpawnInterval;
    json_["StampRockLifeTime"] = stampRockLifeTime;
    json_["StampRockGroupCount"] = stampRockGroupCount;
    json_["StampRockGroupInterval"] = stampRockGroupInterval;
    json_["StampRockScaleMin"] = stampRockScaleMin;
    json_["StampRockScaleMax"] = stampRockScaleMax;
    json_["StampRockRadiusJitter"] = stampRockRadiusJitter;
    json_["StampRockRiseStartOffset"] = stampRockRiseStartOffset;
}

void DragonAttackControllerComponent::Deserialize(const nlohmann::json& json_)
{
    pendingBeamFrameValid = false;
    pendingBeamFrameTimer = 0.0f;
    pendingBeamEmitter.reset();
    actionCategory = json_.value("ActionCategory", std::string("DragonAttack"));
    breathVFXAssetPath = json_.value("BreathVFXAssetPath", std::string("Data/VFX/DragonBreath.json"));
    breathProjectileEmitterAssetPath = json_.value(
        "BreathProjectileEmitterAssetPath",
        std::string("Data/AttackEmitter/DragonBreathLight.json"));
    breathChargeEffectTemplateName = json_.value(
        "BreathChargeEffectTemplateName",
        std::string("DragonBreathCharge"));
    targetObjectName = json_.value("TargetObjectName", std::string("PlayerModel"));
    breathEmitterObjectName = json_.value("BreathEmitterObjectName", std::string());
    breathSourceNodeName = json_.value("BreathSourceNodeName", std::string("Mouth"));
    biteVFXAssetPath = json_.value("BiteVFXAssetPath", std::string("Data/VFX/DragonBite.json"));
    dangerAreaVFXAssetPath = json_.value("DangerAreaVFXAssetPath", std::string("Data/VFX/EnemyDangerArea.json"));
    dangerAreaEnabled = json_.value("DangerAreaEnabled", true);
    showDangerAreaOnAttackStart = json_.value("ShowDangerAreaOnAttackStart", true);
    showBreathChargeOnDanger = json_.value("ShowBreathChargeOnDanger", true);
    dangerAreaGroundOffset = json_.value("DangerAreaGroundOffset", 0.03f);
    dangerAreaRayStartHeight = (std::max)(json_.value("DangerAreaRayStartHeight", 5.0f), 0.01f);
    dangerAreaRayDistance = (std::max)(json_.value("DangerAreaRayDistance", 250.0f), 0.01f);
    dangerAreaGroundQueryInterval = (std::max)(json_.value("DangerAreaGroundQueryInterval", 0.05f), 0.0f);
    dangerAreaAlignToGround = json_.value("DangerAreaAlignToGround", true);
    dangerAreaMultiSample = json_.value("DangerAreaMultiSample", true);
    dangerAreaLengthPadding = json_.value("DangerAreaLengthPadding", 0.0f);
    breathLifeTime = json_.value("BreathLifeTime", 3.2f);
    breathVFXLifeTime = json_.value("BreathVFXLifeTime", 0.6f);
    breathVFXInterval = json_.value("BreathVFXInterval", 0.08f);
    breathDamage = json_.value("BreathDamage", 18.0f);
    breathDamageRate = json_.value("BreathDamageRate", 0.18f);
    breathFixedDamage = json_.value("BreathFixedDamage", 0.0f);
    breathHitInterval = json_.value("BreathHitInterval", 0.25f);
    breathImpactPower = json_.value("BreathImpactPower", 12.0f);
    breathKnockBackPower = json_.value("BreathKnockBackPower", 4.0f);
    breathRadius = json_.value("BreathRadius", 2.8f);
    breathHalfHeight = json_.value("BreathHalfHeight", 8.0f);
    breathForwardOffset = json_.value("BreathForwardOffset", 4.0f);
    breathProjectileMouthForwardOffset = json_.value("BreathProjectileMouthForwardOffset", 0.35f);
    breathTargetAimBlend = json_.value("BreathTargetAimBlend", 0.35f);
    breathAirGroundAimHeight = json_.value("BreathAirGroundAimHeight", 0.35f);
    breathDangerLifeTime = json_.value("BreathDangerLifeTime", 1.9f);
    breathDangerRange = json_.value("BreathDangerRange", 52.0f);
    breathDangerWidth = json_.value("BreathDangerWidth", 6.0f);
    breathDangerStartOffset = json_.value("BreathDangerStartOffset", 0.25f);
    breathProjectileImpactDangerRange = json_.value("BreathProjectileImpactDangerRange", 44.0f);
    breathProjectileImpactDangerRadius = json_.value("BreathProjectileImpactDangerRadius", 4.2f);
    breathFanDangerRange = json_.value("BreathFanDangerRange", 14.5f);
    breathFanDangerAngleDegrees = json_.value("BreathFanDangerAngleDegrees", 95.0f);
    breathFanDangerRadius = json_.value("BreathFanDangerRadius", 3.4f);
    breathFanDangerSampleCount = json_.value("BreathFanDangerSampleCount", 9);
    breathFanDangerRangeSteps = json_.value("BreathFanDangerRangeSteps", 3);
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
    stampSourceObjectName = json_.value("StampSourceObjectName", std::string());
    stampSourceNodeName = json_.value("StampSourceNodeName", std::string());
    stampLifeTime = json_.value("StampLifeTime", 0.5f);
    stampDangerLifeTime = json_.value("StampDangerLifeTime", 1.8f);
    stampDamage = json_.value("StampDamage", 28.0f);
    stampDamageRate = json_.value("StampDamageRate", 0.28f);
    stampFixedDamage = json_.value("StampFixedDamage", 0.0f);
    stampImpactPower = json_.value("StampImpactPower", 26.0f);
    stampKnockBackPower = json_.value("StampKnockBackPower", 12.0f);
    stampRadius = json_.value("StampRadius", 9.0f);
    stampForwardOffset = json_.value("StampForwardOffset", 2.6f);
    stampRockCount = json_.value("StampRockCount", 20);
    stampRockRadius = json_.value("StampRockRadius", 7.5f);
    stampRockArcDegrees = json_.value("StampRockArcDegrees", 360.0f);
    stampRockSpawnInterval = (std::max)(json_.value("StampRockSpawnInterval", 0.04f), 0.0f);
    stampRockLifeTime = json_.value("StampRockLifeTime", 1.8f);
    stampRockGroupCount = (std::max)(json_.value("StampRockGroupCount", 5), 1);
    stampRockGroupInterval = (std::max)(json_.value("StampRockGroupInterval", 0.10f), 0.0f);
    stampRockScaleMin = (std::max)(json_.value("StampRockScaleMin", 0.75f), 0.01f);
    stampRockScaleMax = (std::max)(json_.value("StampRockScaleMax", 1.35f), 0.01f);
    stampRockRadiusJitter = (std::max)(json_.value("StampRockRadiusJitter", 1.2f), 0.0f);
    stampRockRiseStartOffset = json_.value("StampRockRiseStartOffset", -0.65f);

    RegisterActions();
}

REGISTER_COMPONENT(DragonAttackControllerComponent, "DragonAttackControllerComponent")
