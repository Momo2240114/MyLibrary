#include "PrecompiledHeader.h"

#include "ProjectileMotionComponent.h"

#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/Transform/Transform.h"

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

    ImGui::DragFloat3("Direction", &direction.x, 0.01f);
    ImGui::DragFloat3("Velocity", &velocity.x, 0.01f);
    ImGui::DragFloat("Speed", &speed, 0.1f, 0.0f);
    ImGui::DragFloat("Acceleration", &acceleration, 0.1f);
    ImGui::DragFloat("Gravity", &gravity, 0.1f, 0.0f);
    ImGui::DragFloat("Max Speed", &maxSpeed, 0.1f, 0.0f);
}

void ProjectileMotionComponent::Serialize(nlohmann::json& json_) const
{
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

REGISTER_COMPONENT(ProjectileMotionComponent, "ProjectileMotionComponent")
