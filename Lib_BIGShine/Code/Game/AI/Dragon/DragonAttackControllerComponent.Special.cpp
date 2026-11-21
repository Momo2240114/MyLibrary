#include "PrecompiledHeader.h"
#include "DragonAttackControllerComponent.h"
#include "DragonAIComponent.h"
#include "Game/GameMode/GameModeManager.h"
#include "Game/Charcter/Player/PlayerControllerComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "Engine/GamePlay/Core/Component/MotionWarpComponent.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/VFX/DangerAreaComponent.h"
#include "Engine/GamePlay/Camera/CameraManager.h"
#include "Engine/GamePlay/Camera/CameraComponent.h"
#include "Engine/GamePlay/Camera/CameraActionDirectorComponent.h"
#include "Engine/GamePlay/Camera/SpecialAttackCameraComponent.h"
#include "Engine/GamePlay/Camera/CameraShakeComponent.h"
#include "Engine/Graphics/Resource/Animation/AnimationControllerComponent.h"
#include "Engine/Graphics/Particle/ParticleComponent.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Physics/PhysXSystem.h"
#include "Engine/Physics/MeshCollider.h"
#include "Engine/Physics/HurtBox.h"
#include "Engine/Physics/HitBox.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace
{
    constexpr const char* SPECIAL_CAMERA = "DragonSpecialIntro";
    constexpr const char* PHASE_NAMES[] = { "Ascent", "Charge", "Camera return", "Meteor waves", "Glide", "Tracking", "Locked", "Dive", "Landing", "Recovery", "Rock cleanup" };

    // userDataを型変換せず、今回生成した岩だけを遮蔽物として選別する。
    class SpecialQueryFilter : public physx::PxQueryFilterCallback
    {
    public:
        SpecialQueryFilter(const std::vector<const void*>& rocks_, bool cover_) : rocks(rocks_), cover(cover_)
        {
        }

        physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&, const physx::PxShape* shape_,
            const physx::PxRigidActor* actor_, physx::PxHitFlags&) override
        {
            if (!shape_ || !actor_)
            {
                return physx::PxQueryHitType::eNONE;
            }
            const bool rock_ = std::find(rocks.begin(), rocks.end(), actor_->userData) != rocks.end();
            if (cover)
            {
                return rock_ ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
            }
            if (rock_ || shape_->getFlags().isSet(physx::PxShapeFlag::eTRIGGER_SHAPE))
            {
                return physx::PxQueryHitType::eNONE;
            }
            auto filter_ = shape_->getQueryFilterData();
            if (filter_.word0 == 0u)
            {
                filter_ = shape_->getSimulationFilterData();
            }
            constexpr uint32_t GROUND = static_cast<uint32_t>(LayerType::Environment) | static_cast<uint32_t>(LayerType::Terrain);
            return (filter_.word0 & GROUND) != 0u && filter_.word2 == static_cast<uint32_t>(ColliderComponent::CollisionRole::PHYSICAL)
                ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
        }

        physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit& hit_,
            const physx::PxShape*, const physx::PxRigidActor*) override
        {
            return cover || static_cast<const physx::PxRaycastHit&>(hit_).normal.y > 0.45f
                ? physx::PxQueryHitType::eBLOCK : physx::PxQueryHitType::eNONE;
        }

    private:
        const std::vector<const void*>& rocks;
        bool cover;
    };
}

bool DragonAttackControllerComponent::LoadSpecialSettings()
{
    try
    {
        std::ifstream stream_("Data/AI/DragonSpecialActions.json");
        if (!stream_)
        {
            specialStatus = "DragonSpecialActions.json missing";
            return false;
        }
        auto settings_ = nlohmann::json::parse(stream_);
        for (const char* key_ : { "ArenaRadius", "MeteorHoverHeight", "DiveHeight", "AscentTime", "ChargeTime", "CameraReturnTime",
            "IntroCameraDistance", "IntroCameraHeight", "IntroFocusHeight", "IntroFov",
            "MeteorHeight", "MeteorSpeed", "CoverRadiusRatio", "OrbitRadiusRatio", "OrbitSpeed", "OrbitTime", "TrackingTime",
            "TrackingSpeed", "EvadeTime", "DiveSpeed", "DiveRadiusRatio", "RecoveryTime", "RockCleanupInterval", "DecalLifetime",
            "ExplosionScale", "MeteorDamage", "FinalDamage", "DiveDamage", "BlurStrength", "DescentSpeed" })
        {
            if (!settings_.contains(key_) || !settings_[key_].is_number())
            {
                throw std::runtime_error("Missing numeric setting");
            }
            const float value_ = settings_[key_].get<float>();
            if (!std::isfinite(value_) || value_ <= 0.0f)
            {
                throw std::runtime_error("Invalid positive setting");
            }
        }
        if (!settings_.at("StageObject").is_string() || !settings_.at("Waves").is_array() || settings_["Waves"].size() != 5)
        {
            throw std::runtime_error("Invalid stage or waves");
        }
        float previousTime_ = -1.0f;
        for (size_t index_ = 0; index_ < 5; ++index_)
        {
            const auto& wave_ = settings_["Waves"][index_];
            const float time_ = wave_.at("Time").get<float>();
            const float warning_ = wave_.at("Warning").get<float>();
            const float radius_ = wave_.at("RadiusRatio").get<float>();
            if (!std::isfinite(time_) || !std::isfinite(warning_) || !std::isfinite(radius_) || time_ < 0.0f || time_ < previousTime_ || warning_ <= 0.0f || radius_ <= 0.0f)
            {
                throw std::runtime_error("Invalid wave timing/radius");
            }
            previousTime_ = time_;
            const auto& points_ = wave_.at("Positions");
            if (!points_.is_array() || points_.size() != (index_ == 0 || index_ == 4 ? 1u : 2u))
            {
                throw std::runtime_error("Invalid wave count");
            }
            for (const auto& point_ : points_)
            {
                if (!point_.is_array() || point_.size() != 2 || !point_[0].is_number() || !point_[1].is_number() ||
                    !std::isfinite(point_[0].get<float>()) || !std::isfinite(point_[1].get<float>()) ||
                    std::hypot(point_[0].get<float>(), point_[1].get<float>()) > 0.95f)
                {
                    throw std::runtime_error("Invalid wave position");
                }
            }
        }
        specialSettings = std::move(settings_);
        specialStatus = "Settings loaded";
        return true;
    }
    catch (const std::exception& error_)
    {
        specialStatus = std::string("Special settings: ") + error_.what();
        return false;
    }
}

bool DragonAttackControllerComponent::ResolveSpecialGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const
{
    auto* scene_ = PhysXSystem::GetScene();
    if (!scene_)
    {
        return false;
    }
    std::vector<const void*> rocks_;
    for (const auto& meteor_ : specialMeteors)
    {
        if (meteor_.landed && meteor_.body)
        {
            rocks_.push_back(meteor_.body->GetComponent<MeshCollider>().get());
        }
    }
    SpecialQueryFilter callback_(rocks_, false);
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER | physx::PxQueryFlag::ePOSTFILTER;
    physx::PxRaycastBuffer hit_;
    if (!scene_->raycast({position_.x, position_.y + 200.0f, position_.z}, {0,-1,0}, 400.0f, hit_, physx::PxHitFlag::eDEFAULT, filter_, &callback_) || !hit_.hasBlock)
    {
        return false;
    }
    ground_ = { hit_.block.position.x, hit_.block.position.y, hit_.block.position.z };
    return true;
}

void DragonAttackControllerComponent::PlaceSpecialDragon(const DirectX::XMFLOAT3& position_)
{
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->transform)
    {
        return;
    }
    using namespace DirectX;
    owner_->transform->ApplyWorldMatrix(XMMatrixScaling(specialWorldScale.x,specialWorldScale.y,specialWorldScale.z) *
        XMMatrixRotationRollPitchYaw(XMConvertToRadians(specialRotation.x), XMConvertToRadians(specialRotation.y), XMConvertToRadians(specialRotation.z)) *
        XMMatrixTranslation(position_.x,position_.y,position_.z));
}

bool DragonAttackControllerComponent::BeginSpecialAction(bool dive_)
{
    if (!CanSpawnAttack() || IsSpecialActionActive())
    {
        return false;
    }
    if (specialSettings.empty() && !LoadSpecialSettings())
    {
        return false;
    }
    const auto owner_ = GetGameObject();
    const auto player_ = owner_->GetManager().Find(targetObjectName);
    const auto stage_ = owner_->GetManager().Find(specialSettings["StageObject"].get<std::string>());
    const auto animation_ = owner_->GetComponent<AnimationControllerComponent>();
    const auto movement_ = owner_->GetComponent<CharacterMovementComponent>();
    const auto mode_ = owner_->GetManager().GetGameModeManager();
    if (!owner_->transform || !player_ || !player_->transform || !stage_ || !stage_->transform || !animation_ || !movement_ || !mode_)
    {
        specialStatus = "Stage, player, animation, movement or GameMode missing";
        return false;
    }
    const auto health_ = player_->GetComponent<HealthComponent>();
    if (health_ && health_->IsDead())
    {
        return false;
    }
    DirectX::XMFLOAT3 ground_;
    specialStart = owner_->transform->GetWorldPosition();
    if (!ResolveSpecialGround(specialStart, ground_) || !ResolveSpecialGround(stage_->transform->GetWorldPosition(), specialCenter))
    {
        specialStatus = "Stage ground raycast failed";
        return false;
    }
    const auto cameras_ = owner_->GetManager().GetCameraManager();
    const auto camera_ = cameras_ ? cameras_->GetSourceCamera() : nullptr;
    const auto cameraObject_ = camera_ ? camera_->GetGameObject() : nullptr;
    if (!cameraObject_)
    {
        specialStatus = "Battle camera unavailable";
        return false;
    }
    auto director_ = cameraObject_->GetOrAddComponent<CameraActionDirectorComponent>();
    // 独立したStateにし、通常攻撃のHitBox開閉・終了イベントを特殊行動へ持ち込まない。
    const char* NAMES[] = { "DragonSpecialTakeOff", "DragonSpecialHover", "DragonSpecialCharge", "DragonSpecialGlide", "DragonSpecialFalling", "DragonSpecialIdle" };
    const int INDICES[] = { 23, 6, 10, 14, 4, 18 };
    for (int index_ = 0; index_ < 6; ++index_)
    {
        if (!animation_->GetStates().contains(NAMES[index_]))
        {
            animation_->AddState(NAMES[index_], INDICES[index_]);
        }
        auto& state_ = *animation_->GetStates().at(NAMES[index_]);
        state_.rootMotion.enable = false;
        state_.rootMotion.cutVisualWhenDisabled = true;
        state_.events.clear();
        state_.sequencerDataName.clear();
        state_.isLoop = index_ != 0 && index_ != 2;
    }
    StopAttack();
    if (const auto warp_ = owner_->GetComponent<MotionWarpComponent>())
    {
        warp_->Stop();
    }
    specialDive = dive_;
    specialPhase = 0;
    specialTimer = specialElapsed = 0.0f;
    specialWave = 0;
    specialCompleted = specialCameraReturning = false;
    specialGroundOffset = specialStart.y - ground_.y;
    specialRotation = owner_->transform->GetWorldRotation();
    specialWorldScale = owner_->transform->GetWorldScale();
    specialAngle = std::atan2(specialStart.x-specialCenter.x, specialStart.z-specialCenter.z);
    specialAirPosition = specialCenter;
    specialAirPosition.y += specialGroundOffset + specialSettings[dive_ ? "DiveHeight" : "MeteorHoverHeight"].get<float>();
    if (dive_)
    {
        const float radius_ = specialSettings["ArenaRadius"].get<float>() * specialSettings["OrbitRadiusRatio"].get<float>();
        specialAirPosition.x += std::sin(specialAngle)*radius_;
        specialAirPosition.z += std::cos(specialAngle)*radius_;
    }
    specialGameState = static_cast<int>(mode_->GetCurrentGameState());
    specialMovementSuppressed = movement_->IsRuntimeSuppressed();
    movement_->ResetMovementRequests();
    movement_->SetRuntimeSuppressed(true);
    const auto collider_ = movement_->ResolveCollider();
    specialCollider = collider_;
    if (collider_)
    {
        specialGravity = collider_->useGravity;
        specialPhysicsControlsTransform = collider_->physicsControlsTransform;
        collider_->useGravity = false;
        collider_->physicsControlsTransform = false;
        collider_->velocity = {};
    }
    specialAnimationControlled = animation_->IsPresentationControlled();
    animation_->SetPresentationControlled(true);
    animation_->ChangeState("DragonSpecialTakeOff", 0.15f, true);
    specialPreviousPlayerLock = mode_->IsPlayerControlLocked();
    mode_->SetPlayerControlLocked(true);
    specialIntroLocked = true;
    // プレイヤーの既存回避無敵とは別のフラグを保存し、操作不能中の被弾を防ぐ。
    const auto protect_ = [&](auto&& self_, const std::shared_ptr<GameObject>& object_) -> void
    {
        for (const auto& hurt_ : object_->GetComponents<HurtBox>())
        {
            specialPlayerInvincibility.push_back({hurt_,hurt_->IsTemporaryInvincible()});
            hurt_->SetTemporaryInvincible(true);
        }
        for (const auto& component_ : object_->GetComponents<CharacterMovementComponent>())
        {
            specialPlayerSuppression.push_back({component_,component_->IsRuntimeSuppressed()});
            component_->ResetMovementRequests();
            component_->SetRuntimeSuppressed(true);
        }
        for (const auto& component_ : object_->GetComponents<AnimationControllerComponent>())
        {
            specialPlayerSuppression.push_back({component_,component_->IsRuntimeSuppressed()});
            component_->SetRuntimeSuppressed(true);
        }
        for (const auto& component_ : object_->GetComponents<HitBox>())
        {
            specialPlayerSuppression.push_back({component_,component_->IsRuntimeSuppressed()});
            component_->SetRuntimeSuppressed(true);
        }
        for (const auto& child_ : object_->transform->children)
        {
            if (child_)
            {
                if (const auto objectChild_ = child_->gameObject.lock())
                {
                    self_(self_,objectChild_);
                }
            }
        }
    };
    if (const auto controller_ = player_->GetComponent<PlayerControllerComponent>())
    {
        controller_->StopSkill();
    }
    protect_(protect_,player_);
    specialPreviousBlur = GraphicsDeviceManager::Instance().GetRadialBlurParams();
    specialAppliedBlurRadius = specialPreviousBlur.blurRadius;
    specialCamera = director_;
    const auto playerPosition_ = player_->transform->GetWorldPosition();
    const float frontX_ = playerPosition_.x-specialStart.x;
    const float frontZ_ = playerPosition_.z-specialStart.z;
    const float frontLength_ = std::hypot(frontX_,frontZ_);
    DirectX::XMFLOAT3 front_;
    if (frontLength_ > 0.001f)
    {
        front_ = {frontX_/frontLength_,0.0f,frontZ_/frontLength_};
    }
    else
    {
        DirectX::XMStoreFloat3(&front_,DirectX::XMVector3TransformNormal(
            DirectX::XMVectorSet(0.0f,0.0f,1.0f,0.0f),owner_->transform->GetWorldMatrix()));
        front_.y = 0.0f;
        const float length_ = std::hypot(front_.x,front_.z);
        if (length_ > 0.001f)
        {
            front_.x /= length_;
            front_.z /= length_;
        }
        else
        {
            front_ = {0.0f,0.0f,1.0f};
        }
    }
    const float cameraDistance_ = specialSettings["IntroCameraDistance"].get<float>();
    CameraActionPreset preset_;
    preset_.actionName = SPECIAL_CAMERA;
    preset_.targetMode = CAMERA_ACTION_TARGET_MODE::GAME_OBJECT_OFFSET;
    preset_.positionMode = CAMERA_ACTION_POSITION_MODE::WORLD_POSITION;
    // ドラゴンの上昇に合わせて視点も追従させ、正面から胴体を捉える。
    preset_.autoFrameTargets = true;
    preset_.targetObjectName = owner_->GetName();
    preset_.useModelBounds = false;
    preset_.framingRadius = cameraDistance_ * 0.25f;
    preset_.minimumFramingDistance = cameraDistance_;
    preset_.cameraOffset = {front_.x,
        (specialSettings["IntroCameraHeight"].get<float>() - specialSettings["IntroFocusHeight"].get<float>()) / cameraDistance_,
        front_.z};
    preset_.focusOffset = {0.0f,specialSettings["IntroFocusHeight"].get<float>(),0.0f};
    preset_.targetFov = specialSettings["IntroFov"].get<float>();
    preset_.enterDuration = 0.7f;
    preset_.holdDuration = 300.0f;
    preset_.returnDuration = specialSettings["CameraReturnTime"].get<float>();
    preset_.priority = 25;
    preset_.suppressCameraCollision = true;
    preset_.useUnscaledTime = false;
    if (!director_->PlayPresetForTarget(preset_,owner_))
    {
        specialStatus = "Special camera request rejected";
        StopSpecialAction();
        return false;
    }
    // イントロからの復帰先を先に登録し、回避中はドラゴンとプレイヤーを一緒に追う。
    const auto trackingCamera_ = cameraObject_->GetComponent<SpecialAttackCameraComponent>();
    if (trackingCamera_ && trackingCamera_->BeginTracking(player_, owner_))
    {
        specialTrackingCamera = trackingCamera_;
    }
    for (int index_ = 0; index_ < 3; ++index_)
    {
        ParticleSpawnRequest request_;
        request_.assetPath = "Data/Particle/DragonBreath_ChargeGather.json";
        request_.position = specialStart;
        request_.lifeTime = 3600.0f;
        request_.sizeScale = 4.0f;
        specialAura.push_back(owner_->GetManager().GetEffectManager().SpawnParticle(request_));
    }
    specialStatus = dive_ ? "Dive ascent" : "Meteor ascent";
    return true;
}

void DragonAttackControllerComponent::BeginSpecialCameraReturn()
{
    if (specialPhase != 1 || specialCameraReturning)
    {
        return;
    }
    specialCameraReturning = true;
    if (const auto camera_ = specialCamera.lock())
    {
        camera_->StopActionByName(SPECIAL_CAMERA, false);
    }
    specialPhase = 2;
    specialTimer = 0.0f;
}

void DragonAttackControllerComponent::ReleaseSpecialIntro()
{
    if (!specialIntroLocked)
    {
        return;
    }
    specialIntroLocked = false;
    const auto owner_ = GetGameObject();
    if (owner_ && owner_->manager)
    {
        if (const auto mode_ = owner_->GetManager().GetGameModeManager(); mode_ && static_cast<int>(mode_->GetCurrentGameState()) == specialGameState)
        {
            mode_->SetPlayerControlLocked(specialPreviousPlayerLock);
        }
    }
    for (const auto& [weak_, previous_] : specialPlayerSuppression)
    {
        if (const auto component_ = weak_.lock())
        {
            if (const auto movement_ = std::dynamic_pointer_cast<CharacterMovementComponent>(component_))
            {
                movement_->ResetMovementRequests();
            }
            component_->SetRuntimeSuppressed(previous_);
        }
    }
    specialPlayerSuppression.clear();
    for (const auto& [weak_, previous_] : specialPlayerInvincibility)
    {
        if (const auto hurt_ = weak_.lock())
        {
            hurt_->SetTemporaryInvincible(previous_);
        }
    }
    specialPlayerInvincibility.clear();
    auto& blur_ = GraphicsDeviceManager::Instance().GetRadialBlurParams();
    if (std::abs(blur_.blurRadius-specialAppliedBlurRadius) < 0.00001f)
    {
        blur_ = specialPreviousBlur;
    }
}

void DragonAttackControllerComponent::StopSpecialAction()
{
    if (specialPhase < 0)
    {
        return;
    }
    specialPhase = -1;
    ReleaseSpecialIntro();
    if (const auto camera_ = specialCamera.lock())
    {
        camera_->StopActionByName(SPECIAL_CAMERA, true);
    }
    specialCamera.reset();
    if (const auto trackingCamera_ = specialTrackingCamera.lock())
    {
        trackingCamera_->EndTracking();
    }
    specialTrackingCamera.reset();
    const auto owner_ = GetGameObject();
    if (!owner_ || !owner_->manager)
    {
        return;
    }
    auto& manager_ = owner_->GetManager();
    if (specialDanger)
    {
        manager_.Destroy(specialDanger);
    }
    specialDanger.reset();
    for (auto& meteor_ : specialMeteors)
    {
        if (meteor_.body)
        {
            manager_.Destroy(meteor_.body);
        }
        if (meteor_.warning)
        {
            manager_.Destroy(meteor_.warning);
        }
    }
    specialMeteors.clear();
    for (const auto& effect_ : specialAura)
    {
        if (effect_)
        {
            manager_.GetEffectManager().StopEffect(effect_);
        }
    }
    specialAura.clear();
    // 正常終了した焦げ跡は寿命で消す。中断時は戦闘中に生成したものを残さない。
    if (!specialCompleted)
    {
        for (const auto& weak_ : specialDecals)
        {
            if (const auto decal_ = weak_.lock())
            {
                manager_.Destroy(decal_);
            }
        }
        manager_.GetEffectManager().StopAttackEffects(owner_);
        for (auto& attack_ : runtimeAttacks)
        {
            StopRuntimeAttack(attack_);
        }
        runtimeAttacks.clear();
        DirectX::XMFLOAT3 ground_;
        const auto position_ = owner_->transform->GetWorldPosition();
        if (ResolveSpecialGround(position_,ground_))
        {
            ground_.y += specialGroundOffset;
            specialRotation.x = specialRotation.z = 0.0f;
            PlaceSpecialDragon(ground_);
        }
    }
    if (!specialCompleted)
    {
        specialDecals.clear();
    }
    if (const auto collider_ = specialCollider.lock())
    {
        collider_->useGravity = specialGravity;
        collider_->physicsControlsTransform = specialPhysicsControlsTransform;
        collider_->velocity = {};
    }
    specialCollider.reset();
    if (const auto movement_ = owner_->GetComponent<CharacterMovementComponent>())
    {
        movement_->ResetMovementRequests();
        movement_->SetRuntimeSuppressed(specialMovementSuppressed);
    }
    if (const auto animation_ = owner_->GetComponent<AnimationControllerComponent>())
    {
        if (specialCompleted)
        {
            animation_->ChangeState("PlainsDragon_IdleBreathe",0.2f,true);
        }
        animation_->SetPresentationControlled(specialAnimationControlled);
    }
}

bool DragonAttackControllerComponent::IsSpecialTargetSheltered() const
{
    const auto owner_ = gameObject.lock();
    const auto player_ = owner_ ? owner_->GetManager().Find(targetObjectName) : nullptr;
    auto* scene_ = PhysXSystem::GetScene();
    if (!player_ || !player_->transform || !scene_)
    {
        return false;
    }
    std::vector<const void*> rocks_;
    for (const auto& meteor_ : specialMeteors)
    {
        // 爆心地そのものの岩は全方向を遮るため、外周6発の残骸だけを遮蔽に使う。
        if (!meteor_.landed || meteor_.final || !meteor_.body ||
            std::hypot(meteor_.ground.x-specialCenter.x, meteor_.ground.z-specialCenter.z) < meteor_.coverRadius*2.0f)
        {
            continue;
        }
        rocks_.push_back(meteor_.body->GetComponent<MeshCollider>().get());
    }
    const auto point_ = player_->transform->GetWorldPosition();
    const physx::PxVec3 start_(specialCenter.x,specialCenter.y+1.0f,specialCenter.z);
    const physx::PxVec3 delta_(point_.x-start_.x,point_.y+1.0f-start_.y,point_.z-start_.z);
    const float length_ = delta_.magnitude();
    if (length_ < 0.001f)
    {
        return false;
    }
    SpecialQueryFilter callback_(rocks_,true);
    physx::PxQueryFilterData filter_;
    filter_.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
    physx::PxRaycastBuffer hit_;
    return scene_->raycast(start_,delta_/length_,length_,hit_,physx::PxHitFlag::eDEFAULT,filter_,&callback_) && hit_.hasBlock;
}

void DragonAttackControllerComponent::SpawnSpecialImpact(const DirectX::XMFLOAT3& position_, float radius_, bool final_, bool applyDamage_)
{
    const auto owner_ = GetGameObject();
    if (!owner_)
    {
        return;
    }
    const char* effect_ = specialDive ? "DragonDiveImpact" : final_ ? "DragonMeteorFinalImpact" : "DragonMeteorImpact";
    const float damage_ = specialSettings[specialDive ? "DiveDamage" : final_ ? "FinalDamage" : "MeteorDamage"].get<float>();
    if (applyDamage_)
    {
        PlaySpecialImpact(position_,radius_,damage_,effect_);
    }
    else
    {
        owner_->GetManager().GetEffectManager().SpawnImpactEffectAsset(effect_,position_,{0,1,0},owner_);
    }
    auto& effects_ = owner_->GetManager().GetEffectManager();
    for (int layer_ = 0; layer_ < 2; ++layer_)
    {
        DecalSpawnRequest decal_;
        decal_.texturePath = layer_ == 0 ? "Data/Texture/Decal/GroundCrater.png" : "Data/Texture/Decal/ScorchLava.png";
        decal_.groundMaskPath = "Data/Texture/Decal/ScorchRound.png";
        decal_.position = position_;
        decal_.scale = {radius_*1.15f,radius_*1.15f,2.0f};
        decal_.groundSurface = true;
        decal_.emissiveStrength = layer_ == 0 ? 0.0f : 2.5f;
        decal_.color = layer_ == 0 ? DirectX::XMFLOAT4{0.35f,0.18f,0.1f,0.95f} : DirectX::XMFLOAT4{1.0f,0.35f,0.05f,0.9f};
        decal_.lifeTime = specialSettings["DecalLifetime"].get<float>();
        decal_.fadeOutDuration = decal_.lifeTime*0.3f;
        if (const auto object_ = effects_.SpawnDecal(decal_))
        {
            specialDecals.push_back(object_);
        }
    }
    ParticleSpawnRequest dust_;
    dust_.assetPath = "Data/Particle/DragonSpecialExplosion.json";
    dust_.position = position_;
    dust_.lifeTime = 5.0f;
    dust_.sizeScale = radius_ * specialSettings["ExplosionScale"].get<float>();
    const auto explosion_ = effects_.SpawnParticle(dust_);
    effects_.RegisterAttackEffectOwner(explosion_,owner_);
    MeshVFXSpawnRequest shockwave_;
    shockwave_.assetPath = "Data/VFX/DragonSpecialShockwave.json";
    shockwave_.position = position_;
    shockwave_.position.y += 0.12f;
    shockwave_.scale = {radius_,1.0f,radius_};
    shockwave_.lifeTime = 2.0f;
    effects_.RegisterAttackEffectOwner(effects_.SpawnMeshVFX(shockwave_),owner_);
    const auto cameras_ = owner_->GetManager().GetCameraManager();
    const auto camera_ = cameras_ ? cameras_->GetSourceCamera() : nullptr;
    if (const auto cameraObject_ = camera_ ? camera_->GetGameObject() : nullptr)
    {
        cameraObject_->GetOrAddComponent<CameraShakeComponent>()->PlayShake(final_ || specialDive ? 0.8f : 0.25f,0.12f,1.2f,22.0f,20);
    }
}

void DragonAttackControllerComponent::UpdateSpecialAction(float deltaTime_)
{
    if (!IsSpecialActionActive())
    {
        return;
    }

    BS_PROFILE_SCOPE("DragonAttackControllerComponent::UpdateSpecialAction");

    const auto owner_ = GetGameObject();
    const auto mode_ = owner_ && owner_->manager ? owner_->GetManager().GetGameModeManager() : nullptr;
    const auto player_ = owner_ && owner_->manager ? owner_->GetManager().Find(targetObjectName) : nullptr;

    const auto playerHealth_ = player_ ? player_->GetComponent<HealthComponent>() : nullptr;

    if (!CanSpawnAttack() || !mode_ || !player_ || (playerHealth_ && playerHealth_->IsDead()) ||
        static_cast<int>(mode_->GetCurrentGameState()) != specialGameState || mode_->IsEnemyAILocked())
    { 
        specialStatus = "Interrupted";
        StopSpecialAction(); 
        return; 
    }

    const float dt_ = (std::max)(0.0f,deltaTime_);
    specialTimer += dt_;

    const auto animation_ = owner_->GetComponent<AnimationControllerComponent>();
    const float arena_ = specialSettings["ArenaRadius"].get<float>();
    auto position_ = owner_->transform->GetWorldPosition();

    if (specialIntroLocked)
    {
        const float charge_ = specialSettings["ChargeTime"].get<float>();
        const float pulse_ = specialPhase == 1 ? std::sin(std::clamp(specialTimer/charge_,0.0f,1.0f)*DirectX::XM_PI) : 0.0f;
        auto& blur_ = GraphicsDeviceManager::Instance().GetRadialBlurParams();

        if (std::abs(blur_.blurRadius-specialAppliedBlurRadius) < 0.00001f)
        {
            blur_ = specialPreviousBlur;
            blur_.blurRadius += specialSettings["BlurStrength"].get<float>()*pulse_;
            blur_.blurCenter = {0.5f,0.45f};
            specialAppliedBlurRadius = blur_.blurRadius;
        }
    }

    for (size_t index_ = 0; index_ < specialAura.size(); ++index_)
    {
        const auto& effect_ = specialAura[index_];
        if (!effect_)
        {
            continue;
        }
        // 翼の左右と口元。モデルのローカル軸からWorldへ変換して旋回に追従させる。
        const DirectX::XMFLOAT3 offset_ = index_ == 0 ? DirectX::XMFLOAT3{0,4,8} : DirectX::XMFLOAT3{index_ == 1 ? -9.0f : 9.0f,5,0};
        DirectX::XMFLOAT3 world_;
        DirectX::XMStoreFloat3(&world_,DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&offset_),owner_->transform->GetWorldMatrix()));
        effect_->transform->localPosition = world_;
        effect_->transform->MarkDirty();
    }

    if (specialPhase == 0)
    {
        float rate_ = std::clamp(specialTimer/specialSettings["AscentTime"].get<float>(),0.0f,1.0f);
        rate_ = rate_*rate_*(3.0f-2.0f*rate_);
        position_ = 
        {
            specialStart.x+(specialAirPosition.x-specialStart.x)*rate_,
            specialStart.y+(specialAirPosition.y-specialStart.y)*rate_,
            specialStart.z+(specialAirPosition.z-specialStart.z)*rate_
        };

        if (rate_ >= 1.0f)
        {
            specialPhase = 1;
            specialTimer = 0.0f;
            animation_->ChangeState(specialDive ? "DragonSpecialHover" : "DragonSpecialCharge",0.2f,true);
        }
    }
    else if (specialPhase == 1)
    {
        if (specialTimer >= specialSettings["ChargeTime"].get<float>())
        {
            BeginSpecialCameraReturn();
        }
    }
    else if (specialPhase == 2)
    {
        const auto camera_ = specialCamera.lock();
        if (!camera_ || !camera_->IsPlayingAction(SPECIAL_CAMERA))
        {
            ReleaseSpecialIntro();
            specialPhase = specialDive ? 4 : 3;
            specialTimer = specialElapsed = 0.0f;
            animation_->ChangeState(specialDive ? "DragonSpecialGlide" : "DragonSpecialHover",0.2f,true);
            if (specialDive)
            {
                for (auto& effect_ : specialAura)
                {
                    if (effect_)
                    {
                        owner_->GetManager().GetEffectManager().StopEffect(effect_);
                    }
                    ParticleSpawnRequest trail_;
                    trail_.assetPath = "Data/Particle/DragonSpecialMeteorTrail.json";
                    trail_.position = position_;
                    trail_.sizeScale = 4.0f;
                    trail_.lifeTime = 3600.0f;
                    effect_ = owner_->GetManager().GetEffectManager().SpawnParticle(trail_);
                }
            }
        }
        else if (specialTimer > specialSettings["CameraReturnTime"].get<float>()+5.0f)
        {
            specialStatus = "Camera return timed out";
            StopSpecialAction();
            return;
        }
    }
    else if (specialPhase == 3)
    {
        specialElapsed += dt_;
        const auto& waves_ = specialSettings["Waves"];
        if (specialWave < 5 && specialElapsed >= waves_[specialWave]["Time"].get<float>())
        {
            const bool final_ = specialWave == 4;
            const bool previousLanded_ = std::all_of(specialMeteors.begin(),specialMeteors.end(),[](const SpecialMeteor& meteor_)
            {
                return meteor_.landed;
            });
            if (!final_ || previousLanded_)
            {
                for (const auto& point_ : waves_[specialWave]["Positions"])
                {
                    SpecialMeteor meteor_;
                    auto sample_ = specialCenter;
                    sample_.x += point_[0].get<float>()*arena_;
                    sample_.z += point_[1].get<float>()*arena_;
                    if (!ResolveSpecialGround(sample_,meteor_.ground))
                    {
                        specialStatus = "Meteor target has no stage ground";
                        StopSpecialAction();
                        return;
                    }
                    meteor_.radius = waves_[specialWave]["RadiusRatio"].get<float>()*arena_;
                    meteor_.coverRadius = specialSettings["CoverRadiusRatio"].get<float>()*arena_;
                    meteor_.height = specialSettings["MeteorHeight"].get<float>();
                    meteor_.warningRemaining = waves_[specialWave]["Warning"].get<float>();
                    meteor_.final = final_;
                    auto bodyPosition_ = meteor_.ground;
                    bodyPosition_.y += meteor_.height;
                    const float scale_ = final_ ? meteor_.radius*0.65f : meteor_.coverRadius;
                    meteor_.body = owner_->GetManager().Instantiate("DragonSpecialMeteor");
                    if (!meteor_.body)
                    {
                        specialStatus = "Meteor prefab unavailable";
                        StopSpecialAction();
                        return;
                    }
                    meteor_.body->SetNoSaveObject();
                    meteor_.body->transform->localPosition = bodyPosition_;
                    meteor_.body->transform->localScale = {scale_,scale_,scale_};
                    meteor_.body->transform->MarkDirty();
                    if (const auto particles_ = meteor_.body->GetComponent<ParticleComponent>())
                    {
                        particles_->SetRuntimeVisualScale((std::min)(6.0f,1.8f*scale_/meteor_.coverRadius));
                    }
                    const auto collider_ = meteor_.body->GetComponent<MeshCollider>();
                    if (!collider_)
                    {
                        owner_->GetManager().Destroy(meteor_.body);
                        specialStatus = "Meteor collider unavailable";
                        StopSpecialAction();
                        return;
                    }
                    // プレハブの静的判定は、落下演出中だけ無効にする。
                    collider_->SetEnabled(false);
                    DangerAreaTemplate danger_;
                    danger_.shapeType = DangerAreaShapeType::CIRCLE;
                    danger_.radius = meteor_.radius;
                    danger_.lifeTime = meteor_.warningRemaining + meteor_.height/specialSettings["MeteorSpeed"].get<float>()+0.5f;
                    meteor_.warning = owner_->GetOrAddComponent<DangerAreaComponent>()->ShowAt(danger_,meteor_.ground);
                    specialMeteors.push_back(std::move(meteor_));
                    if (!specialMeteors.back().warning)
                    {
                        specialStatus = "Meteor visual or warning unavailable";
                        StopSpecialAction();
                        return;
                    }
                }
                ++specialWave;
            }
        }
        for (auto& meteor_ : specialMeteors)
        {
            if (meteor_.landed)
            {
                continue;
            }
            float fallingTime_ = dt_;
            if (meteor_.warningRemaining > 0.0f)
            {
                fallingTime_ = (std::max)(0.0f, dt_-meteor_.warningRemaining);
                meteor_.warningRemaining = (std::max)(0.0f,meteor_.warningRemaining-dt_);
            }

            meteor_.height = (std::max)(0.0f,meteor_.height-specialSettings["MeteorSpeed"].get<float>()*fallingTime_);
            auto bodyPosition_ = meteor_.ground;
            bodyPosition_.y += meteor_.height;

            if (meteor_.body)
            {
                meteor_.body->transform->localPosition = bodyPosition_;
                meteor_.body->transform->MarkDirty();
            }

            if (meteor_.height > 0.0f)
            {
                continue;
            }

            meteor_.landed = true;

            if (meteor_.warning) 
            {
                owner_->GetManager().Destroy(meteor_.warning);
            }

            meteor_.warning.reset();
            if (const auto particles_ = meteor_.body->GetComponent<ParticleComponent>())
            {
                particles_->Stop();
            }
            const bool sheltered_ = meteor_.final && IsSpecialTargetSheltered();
            SpawnSpecialImpact(meteor_.ground,meteor_.radius,meteor_.final,!sheltered_);
            if (!meteor_.final)
            {
                const auto collider_ = meteor_.body->GetComponent<MeshCollider>();
                collider_->SetEnabled(true);
                if (collider_->IsStarted() && collider_->IsLoadReady())
                {
                    collider_->RecreatePhysXActor();
                }
            }
            else
            {
                owner_->GetManager().Destroy(meteor_.body);
                meteor_.body.reset();
                specialStatus = sheltered_ ? "Final impact blocked by meteor cover" : "Final impact";
                specialPhase = 10;
                specialTimer = 0.0f;
            }
        }
    }
    else if (specialPhase == 4)
    {
        specialAngle += specialSettings["OrbitSpeed"].get<float>()*dt_;
        const float radius_ = arena_*specialSettings["OrbitRadiusRatio"].get<float>();
        position_ = {specialCenter.x+std::sin(specialAngle)*radius_,specialAirPosition.y,specialCenter.z+std::cos(specialAngle)*radius_};
        const float orbitYaw_ = DirectX::XMConvertToDegrees(specialAngle+DirectX::XM_PIDIV2);
        specialRotation.y += std::remainder(orbitYaw_-specialRotation.y,360.0f)*(1.0f-std::exp(-6.0f*dt_));
        specialRotation.z = -12.0f*std::clamp(specialTimer,0.0f,1.0f);
        if (specialTimer >= specialSettings["OrbitTime"].get<float>())
        {
            specialPhase = 5;
            specialTimer = 0.0f;
            animation_->ChangeState("DragonSpecialHover",0.3f,true);
            if (!ResolveSpecialGround(player_->transform->GetWorldPosition(),specialTarget))
            {
                specialStatus = "Dive target ground missing";
                StopSpecialAction(); 
                return;
            }

            DangerAreaTemplate danger_;
            danger_.shapeType = DangerAreaShapeType::CIRCLE;
            danger_.radius = arena_*specialSettings["DiveRadiusRatio"].get<float>();
            danger_.lifeTime = specialSettings["TrackingTime"].get<float>()+specialSettings["EvadeTime"].get<float>()+30.0f;
            specialDanger = owner_->GetOrAddComponent<DangerAreaComponent>()->ShowAt(danger_,specialTarget);
            if (!specialDanger) 
            { 
                specialStatus = "Dive warning unavailable";
                StopSpecialAction();
                return; 
            }
        }
    }
    else if (specialPhase == 5 || specialPhase == 6)
    {
        specialRotation.z *= std::exp(-6.0f*dt_);
        if (specialPhase == 5)
        {
            auto sample_ = player_->transform->GetWorldPosition();
            const float dx_ = sample_.x-specialCenter.x, dz_ = sample_.z-specialCenter.z;
            const float distance_ = std::hypot(dx_,dz_);
            const float limit_ = arena_*0.88f;
            if (distance_ > limit_)
            {
                sample_.x = specialCenter.x+dx_*limit_/distance_;
                sample_.z = specialCenter.z+dz_*limit_/distance_;
            }
            DirectX::XMFLOAT3 ground_;
            if (ResolveSpecialGround(sample_,ground_))
            {
                const float blend_ = 1.0f-std::exp(-specialSettings["TrackingSpeed"].get<float>()*dt_);
                specialTarget.x += (ground_.x-specialTarget.x)*blend_;
                specialTarget.z += (ground_.z-specialTarget.z)*blend_;
                if (!ResolveSpecialGround(specialTarget,specialTarget))
                {
                    specialStatus = "Tracking ground missing";
                    StopSpecialAction();
                    return;
                }
                owner_->GetOrAddComponent<DangerAreaComponent>()->UpdateAt(specialDanger,specialTarget,0.0f);
            }
            if (specialTimer >= specialSettings["TrackingTime"].get<float>())
            {
                specialPhase = 6; 
                specialTimer = 0.0f; 
            }
        }
        const float targetYaw_ = DirectX::XMConvertToDegrees(std::atan2(specialTarget.x-position_.x,specialTarget.z-position_.z));
        specialRotation.y += std::remainder(targetYaw_-specialRotation.y,360.0f)*(1.0f-std::exp(-6.0f*dt_));
        if (specialPhase == 6 && specialTimer >= specialSettings["EvadeTime"].get<float>())
        {
            specialPhase = 7;
            specialTimer = 0.0f;
            specialStart = position_;
            animation_->ChangeState("DragonSpecialFalling",0.1f,true);
        }
    }
    else if (specialPhase == 7 || specialPhase == 8)
    {
        // 確定後はPlayer座標を一切参照せず、保存済みの着地点へ直進する。
        auto destination_ = specialTarget;
        destination_.y += specialGroundOffset;
        const float dx_ = destination_.x-position_.x, dy_ = destination_.y-position_.y, dz_ = destination_.z-position_.z;
        const float distance_ = std::sqrt(dx_*dx_+dy_*dy_+dz_*dz_);
        const float speed_ = specialSettings[specialPhase == 7 ? "DiveSpeed" : "DescentSpeed"].get<float>();
        const float rate_ = distance_ <= 0.001f ? 1.0f : (std::min)(1.0f,speed_*dt_/distance_);
        position_ = {position_.x+dx_*rate_,position_.y+dy_*rate_,position_.z+dz_*rate_};
        if (rate_ >= 1.0f)
        {
            if (specialPhase == 7)
            {
                SpawnSpecialImpact(specialTarget,arena_*specialSettings["DiveRadiusRatio"].get<float>(),true);
            }
            specialPhase = 9;
            specialTimer = 0.0f;
            specialRotation.x = specialRotation.z = 0.0f;
            animation_->ChangeState("DragonSpecialIdle",0.25f,true);
            if (specialDanger) 
            {
                owner_->GetManager().Destroy(specialDanger);
            }
            specialDanger.reset();
        }
    }
    else if (specialPhase == 9 && specialTimer >= specialSettings["RecoveryTime"].get<float>())
    {
        PlaceSpecialDragon(position_);
        specialCompleted = true;
        specialStatus = "Complete";
        StopSpecialAction();
        return;
    }
    else if (specialPhase == 10)
    {
        const float interval_ = specialSettings["RockCleanupInterval"].get<float>();
        bool remaining_ = false;
        for (size_t index_ = 0; index_ < specialMeteors.size(); ++index_)
        {
            auto& meteor_ = specialMeteors[index_];
            if (!meteor_.body || meteor_.final)
            {
                continue;
            }
            const float shrink_ = std::clamp((specialTimer-2.0f-static_cast<float>(index_)*interval_)/interval_,0.0f,1.0f);
            const float scale_ = meteor_.coverRadius*(1.0f-shrink_);
            meteor_.body->transform->localScale = {scale_,scale_,scale_};
            meteor_.body->transform->MarkDirty();
            if (shrink_ >= 1.0f)
            {
                owner_->GetManager().Destroy(meteor_.body);
                meteor_.body.reset();
            }
            else
            {
                remaining_ = true;
            }
        }
        if (!remaining_)
        {
            specialPhase = 8;
            specialTimer = 0.0f;
            if (!ResolveSpecialGround(specialCenter,specialTarget)) 
            {
                specialStatus = "Landing ground missing";
                StopSpecialAction();
                return;
            }
            animation_->ChangeState("DragonSpecialHover",0.2f,true);
        }
    }
    PlaceSpecialDragon(position_);
}

void DragonAttackControllerComponent::DrawSpecialGui()
{
    if (!ImGui::CollapsingHeader("Dragon Special Actions"))
    {
        return;
    }

    ImGui::Text("Action: %s / Phase: %s / Time: %.2f",IsSpecialActionActive() ? (specialDive ? "Dive" : "Meteors") : "None",
        specialPhase >= 0 && specialPhase < 11 ? PHASE_NAMES[specialPhase] : "Idle",specialTimer);

    ImGui::Text("Input locked: %s / Meteors: %d / Aura: %d",specialIntroLocked ? "Yes" : "No",static_cast<int>(specialMeteors.size()),static_cast<int>(specialAura.size()));
    ImGui::Text("Fixed target: %.2f, %.2f, %.2f",specialTarget.x,specialTarget.y,specialTarget.z);
    ImGui::TextWrapped("%s",specialStatus.c_str());

    for (const auto& meteor_ : specialMeteors)
    {
        ImGui::Text("Meteor (%.1f, %.1f, %.1f) r=%.1f height=%.1f %s", meteor_.ground.x, meteor_.ground.y, meteor_.ground.z, meteor_.radius, meteor_.height, meteor_.landed ? "Landed" : "Pending");
    }

    ImGui::BeginDisabled(IsSpecialActionActive());
    if (ImGui::Button("Reload Special Settings")) 
    {
        LoadSpecialSettings();
    }
    ImGui::EndDisabled();
}
