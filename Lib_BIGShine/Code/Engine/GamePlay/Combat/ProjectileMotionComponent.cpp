#include "PrecompiledHeader.h"

#include "ProjectileMotionComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Combat/HealthComponent.h"
#include "Engine/Physics/PhysXSystem.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

const std::string& ProjectileMotionComponent::GetName() const
{
    static const std::string name_ = "ProjectileMotionComponent";
    return name_;
}

void ProjectileMotionComponent::Start()
{
    BS_PROFILE_SCOPE("ProjectileMotionComponent::Start");

    direction = NormalizeDirection(direction);
    if (velocity.x == 0.0f && velocity.y == 0.0f && velocity.z == 0.0f)
    {
        velocity = {
            direction.x * speed,
            direction.y * speed,
            direction.z * speed
        };
    }
}

void ProjectileMotionComponent::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("ProjectileMotionComponent::Update");

    if (homingEnabled) { UpdateHoming(deltaTime_); return; }
    const float safeDeltaTime_ = (std::max)(deltaTime_, 0.0f);
    if (safeDeltaTime_ <= 0.0f) return;

    const DirectX::XMFLOAT3 normalizedDirection_ = NormalizeDirection(direction);
    velocity.x += normalizedDirection_.x * acceleration * safeDeltaTime_;
    velocity.y += normalizedDirection_.y * acceleration * safeDeltaTime_ - gravity * safeDeltaTime_;
    velocity.z += normalizedDirection_.z * acceleration * safeDeltaTime_;

    const float speedSq_ =
        velocity.x * velocity.x +
        velocity.y * velocity.y +
        velocity.z * velocity.z;
    if (maxSpeed > 0.0f && speedSq_ > maxSpeed * maxSpeed)
    {
        const float speedLength_ = std::sqrt(speedSq_);
        const float rate_ = maxSpeed / speedLength_;
        velocity.x *= rate_;
        velocity.y *= rate_;
        velocity.z *= rate_;
    }

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->transform) return;

    // 判定Object自体を動かすことで、子になっているHitBoxと見た目を同じ弾道へ追従させる。
    owner_->transform->AddMoveOffset({
        velocity.x * safeDeltaTime_,
        velocity.y * safeDeltaTime_,
        velocity.z * safeDeltaTime_ });
}

void ProjectileMotionComponent::Gui()
{
    BS_PROFILE_SCOPE("ProjectileMotionComponent::Gui");

    ImGui::Checkbox("Homing", &homingEnabled);
    ImGui::DragFloat("Homing Wait", &homingWaitTime, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Homing Turn Degrees/Sec", &homingTurnSpeed, 0.1f, 0.0f, 180.0f);
    ImGui::DragFloat("Homing Collision Radius", &homingCollisionRadius, 0.01f, 0.01f, 5.0f);
    ImGui::Text("Homing elapsed %.2f / target %s", homingElapsed,
        homingTarget.expired() ? "missing" : "active");
    ImGui::DragFloat3("Direction", &direction.x, 0.01f);
    ImGui::DragFloat3("Velocity", &velocity.x, 0.01f);
    ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f);
    ImGui::DragFloat("Acceleration", &acceleration, 0.1f);
    ImGui::DragFloat("Gravity", &gravity, 0.1f, 0.0f);
    ImGui::DragFloat("Max Speed", &maxSpeed, 0.1f, 0.0f);
}

void ProjectileMotionComponent::Serialize(nlohmann::json& json_) const
{
    json_["HomingEnabled"] = homingEnabled;
    json_["HomingWaitTime"] = homingWaitTime;
    json_["HomingTurnSpeed"] = homingTurnSpeed;
    json_["HomingCollisionRadius"] = homingCollisionRadius;
    json_["HomingTargetName"] = homingTarget.expired() ? std::string() : homingTarget.lock()->GetName();
    json_["HomingAttackerName"] = homingAttacker.expired() ? std::string() : homingAttacker.lock()->GetName();
    json_["ComponentName"] = GetName();
    json_["Direction"] = { direction.x, direction.y, direction.z };
    json_["Velocity"] = { velocity.x, velocity.y, velocity.z };
    json_["Speed"] = speed;
    json_["Acceleration"] = acceleration;
    json_["Gravity"] = gravity;
    json_["MaxSpeed"] = maxSpeed;
}

void ProjectileMotionComponent::Deserialize(const nlohmann::json& json_)
{
    homingEnabled = json_.value("HomingEnabled", false);
    homingWaitTime = (std::max)(0.0f, json_.value("HomingWaitTime", 0.0f));
    homingTurnSpeed = std::clamp(json_.value("HomingTurnSpeed", 25.0f), 0.0f, 180.0f);
    homingCollisionRadius = (std::max)(0.01f, json_.value("HomingCollisionRadius", 0.65f));
    homingElapsed = 0.0f;
    if (const auto owner_ = GetGameObject(); owner_ && owner_->manager)
    {
        homingTarget = owner_->GetManager().Find(json_.value("HomingTargetName", std::string()));
        homingAttacker = owner_->GetManager().Find(json_.value("HomingAttackerName", std::string()));
        hoverPosition = owner_->transform->GetWorldPosition();
    }
    direction = NormalizeDirection(ReadFloat3(json_, "Direction", { 0.0f, 0.0f, 1.0f }));
    velocity = ReadFloat3(json_, "Velocity", { 0.0f, 0.0f, 0.0f });
    speed = (std::max)(json_.value("Speed", 0.0f), 0.0f);
    acceleration = json_.value("Acceleration", 0.0f);
    gravity = (std::max)(json_.value("Gravity", 0.0f), 0.0f);
    maxSpeed = (std::max)(json_.value("MaxSpeed", 0.0f), 0.0f);
}

void ProjectileMotionComponent::Configure(
    const DirectX::XMFLOAT3& direction_,
    float speed_,
    float acceleration_,
    float gravity_,
    float maxSpeed_)
{
    homingEnabled = false;
    homingTarget.reset();
    homingAttacker.reset();
    homingElapsed = 0.0f;
    direction = NormalizeDirection(direction_);
    speed = (std::max)(speed_, 0.0f);
    acceleration = acceleration_;
    gravity = (std::max)(gravity_, 0.0f);
    maxSpeed = (std::max)(maxSpeed_, 0.0f);
    velocity = {
        direction.x * speed,
        direction.y * speed,
        direction.z * speed
    };
}

DirectX::XMFLOAT3 ProjectileMotionComponent::NormalizeDirection(const DirectX::XMFLOAT3& direction_) const
{
    const float lengthSq_ =
        direction_.x * direction_.x +
        direction_.y * direction_.y +
        direction_.z * direction_.z;
    if (lengthSq_ <= 0.000001f)
    {
        return { 0.0f, 0.0f, 1.0f };
    }

    const float invLength_ = 1.0f / std::sqrt(lengthSq_);
    return {
        direction_.x * invLength_,
        direction_.y * invLength_,
        direction_.z * invLength_
    };
}

DirectX::XMFLOAT3 ProjectileMotionComponent::ReadFloat3(
    const nlohmann::json& json_,
    const char* key_,
    const DirectX::XMFLOAT3& fallback_) const
{
    if (!json_.contains(key_) || !json_[key_].is_array() || json_[key_].size() < 3)
    {
        return fallback_;
    }

    return {
        json_[key_][0].get<float>(),
        json_[key_][1].get<float>(),
        json_[key_][2].get<float>()
    };
}

void ProjectileMotionComponent::ConfigureHoming(const std::shared_ptr<GameObject>& target_,
    const std::shared_ptr<GameObject>& attacker_, float waitTime_, float turnDegreesPerSecond_, float collisionRadius_)
{
    homingTarget = target_;
    homingAttacker = attacker_;
    homingEnabled = true;
    homingWaitTime = (std::max)(0.0f, waitTime_);
    homingElapsed = 0.0f;
    homingTurnSpeed = std::clamp(turnDegreesPerSecond_, 0.0f, 180.0f);
    homingCollisionRadius = (std::max)(0.01f, collisionRadius_);
    if (const auto owner_ = GetGameObject()) hoverPosition = owner_->transform->GetWorldPosition();
}

bool ProjectileMotionComponent::UpdateHoming(float deltaTime_)
{
    BS_PROFILE_SCOPE("ProjectileMotionComponent::UpdateHoming");
    const auto owner_ = GetGameObject();
    const auto attacker_ = homingAttacker.lock();
    if (!owner_ || !owner_->manager || !owner_->transform || owner_->IsPendingDestroy()) return false;
    const auto health_ = attacker_ ? attacker_->GetComponent<HealthComponent>() : nullptr;
    if (!attacker_ || attacker_->IsPendingDestroy() || !attacker_->IsCombatReady() || (health_ && health_->IsDead()))
    { owner_->GetManager().Destroy(owner_); return false; }
    const float delta_ = (std::max)(0.0f, deltaTime_);
    const bool wasWaiting_ = homingElapsed < homingWaitTime;
    homingElapsed += delta_;
    if (homingElapsed < homingWaitTime)
    {
        // 待機中の揺れは基準位置から計算し、上下移動を累積しない。
        owner_->transform->ApplyWorldMatrix(DirectX::XMMatrixTranslation(hoverPosition.x,
            hoverPosition.y + std::sin(homingElapsed * 3.0f) * 0.12f, hoverPosition.z));
        return true;
    }
    if (wasWaiting_)
    {
        const auto target_ = homingTarget.lock();
        if (target_ && target_->transform)
        {
            const auto position_ = owner_->transform->GetWorldPosition();
            const auto aim_ = target_->transform->GetWorldPosition();
            direction = NormalizeDirection({ aim_.x - position_.x, aim_.y + 1.0f - position_.y, aim_.z - position_.z });
        }
    }
    const float moveDelta_ = (std::min)(delta_, homingElapsed - homingWaitTime);
    const auto position_ = owner_->transform->GetWorldPosition();
    if (const auto target_ = homingTarget.lock(); target_ && target_->transform)
    {
        using namespace DirectX;
        const auto aim_ = target_->transform->GetWorldPosition();
        const auto desired_ = NormalizeDirection({ aim_.x - position_.x, aim_.y + 1.0f - position_.y, aim_.z - position_.z });
        const auto current_ = XMLoadFloat3(&direction);
        const auto next_ = XMLoadFloat3(&desired_);
        const float angle_ = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(current_, next_)), -1.0f, 1.0f));
        const float limit_ = XMConvertToRadians(homingTurnSpeed) * moveDelta_;
        if (angle_ <= limit_) direction = desired_;
        else if (limit_ > 0.0f)
        {
            auto axis_ = XMVector3Cross(current_, next_);
            if (XMVectorGetX(XMVector3LengthSq(axis_)) < 0.000001f) axis_ = XMVector3Orthogonal(current_);
            XMStoreFloat3(&direction, XMVector3Normalize(XMVector3Rotate(current_, XMQuaternionRotationAxis(axis_, limit_))));
        }
    }
    speed = (std::max)(0.0f, speed + acceleration * moveDelta_);
    if (maxSpeed > 0.0f) speed = (std::min)(speed, maxSpeed);
    const float distance_ = speed * moveDelta_;
    // Sphere sweepで壁の手前に止め、低フレームレートでも障害物を飛び越えない。
    if (auto scene_ = PhysXSystem::GetScene(); scene_ && distance_ > 0.0001f)
    {
        physx::PxSweepBuffer hit_;
        physx::PxQueryFilterData filter_;
        filter_.flags = physx::PxQueryFlag::eSTATIC;
        if (scene_->sweep(physx::PxSphereGeometry(homingCollisionRadius),
            physx::PxTransform(physx::PxVec3(position_.x, position_.y, position_.z)),
            physx::PxVec3(direction.x, direction.y, direction.z), distance_, hit_, physx::PxHitFlag::eDEFAULT, filter_) && hit_.hasBlock)
        { owner_->GetManager().Destroy(owner_); return false; }
    }
    owner_->transform->AddMoveOffset({ direction.x * distance_, direction.y * distance_, direction.z * distance_ });
    return true;
}

REGISTER_COMPONENT(ProjectileMotionComponent, "ProjectileMotionComponent")
