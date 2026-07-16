#include "PrecompiledHeader.h"

#include "AfterimageTrailComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace
{
    DirectX::XMFLOAT3 TransformPosition(
        const DirectX::XMFLOAT3& position_,
        const DirectX::XMMATRIX& transform_)
    {
        DirectX::XMFLOAT3 result_{};
        DirectX::XMStoreFloat3(
            &result_,
            DirectX::XMVector3TransformCoord(
                DirectX::XMLoadFloat3(&position_), transform_));
        return result_;
    }
}

AfterimageTrailComponent::AfterimageTrailComponent()
{
    MeshVFXPart part_{};
    part_.renderEnabled = true;
    part_.useMaterialBaseTexture = false;
    part_.useMaterialNormalTexture = false;
    part_.color = { 0.18f, 0.85f, 1.0f, 1.0f };
    part_.alpha = 0.65f;
    part_.useFresnel = true;
    part_.fresnelPower = 1.8f;
    part_.fresnelStrength = 0.9f;
    part_.useVertexNoise = true;
    part_.vertexNoiseHorizontalStrength = 0.12f;
    part_.vertexNoiseVerticalStrength = 0.08f;
    part_.vertexNoiseHorizontalScale = 1.8f;
    part_.vertexNoiseVerticalScale = 1.2f;
    part_.vertexNoiseSpeed = { 0.0f, 2.5f, 0.0f };
    part_.blendMode = MeshVFXBlendMode::ADDITIVE;

    MeshVFXAsset asset_{};
    asset_.parts.push_back(part_);
    SetAsset(asset_);

    SetEmitting(false);
    SetLifeTime(0.28f);
    SetMaxSampleCount(48);
    SetMinimumSampleDistance(0.05f);
}

void AfterimageTrailComponent::Start()
{
    TrailMeshComponent::Start();
    RegisterActions();
}

void AfterimageTrailComponent::LateUpdate(float deltaTime_)
{
    DirectX::XMFLOAT3 leftPosition_{};
    DirectX::XMFLOAT3 rightPosition_{};
    if (IsEmitting() && GetTrailSegment(leftPosition_, rightPosition_))
    {
        AddSample(leftPosition_, rightPosition_);
    }

    TrailMeshComponent::LateUpdate(deltaTime_);
}

void AfterimageTrailComponent::Gui()
{
    TrailMeshComponent::Gui();
    ImGui::SeparatorText("Afterimage Trail");
    ImGui::DragFloat("Trail Width", &trailWidth, 0.01f, 0.001f, 100.0f);
    ImGui::DragFloat("Vertical Offset", &verticalOffset, 0.01f, -100.0f, 100.0f);
    ImGui::DragFloat("Wobble Amplitude", &wobbleAmplitude, 0.01f, 0.0f, 100.0f);
    ImGui::DragFloat("Wobble Speed", &wobbleSpeed, 0.1f, 0.0f, 100.0f);
}

void AfterimageTrailComponent::Serialize(nlohmann::json& json_) const
{
    TrailMeshComponent::Serialize(json_);
    json_["AfterimageTrailWidth"] = trailWidth;
    json_["AfterimageTrailVerticalOffset"] = verticalOffset;
    json_["AfterimageTrailWobbleAmplitude"] = wobbleAmplitude;
    json_["AfterimageTrailWobbleSpeed"] = wobbleSpeed;
}

void AfterimageTrailComponent::Deserialize(const nlohmann::json& json_)
{
    TrailMeshComponent::Deserialize(json_);
    if (!json_.contains("TrailEmitting")) SetEmitting(false);
    trailWidth = (std::max)(json_.value("AfterimageTrailWidth", 1.8f), 0.001f);
    verticalOffset = json_.value("AfterimageTrailVerticalOffset", 0.8f);
    wobbleAmplitude = (std::max)(json_.value("AfterimageTrailWobbleAmplitude", 0.35f), 0.0f);
    wobbleSpeed = (std::max)(json_.value("AfterimageTrailWobbleSpeed", 18.0f), 0.0f);
}

void AfterimageTrailComponent::RegisterActions()
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_) return;

    auto& registry_ = owner_->GetLocalRegistry();
    registry_.RegisterAction("VFX", "AfterimageTrailStart", [this]() { StartTrail(); });
    registry_.RegisterAction("VFX", "AfterimageTrailStop", [this]() { StopTrail(); });
    registry_.RegisterAction("VFX", "AfterimageTrailClear", [this]() { ClearTrail(); });
}

bool AfterimageTrailComponent::GetTrailSegment(
    DirectX::XMFLOAT3& leftPosition_,
    DirectX::XMFLOAT3& rightPosition_) const
{
    const std::shared_ptr<GameObject> owner_ = gameObject.lock();
    if (!owner_ || !owner_->transform) return false;

    const DirectX::XMMATRIX world_ = owner_->transform->GetWorldMatrix();
    const DirectX::XMFLOAT3 center_ = TransformPosition(
        { 0.0f, verticalOffset, 0.0f },
        world_);

    DirectX::XMVECTOR right_ = DirectX::XMVectorSetW(world_.r[0], 0.0f);
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(right_)) < 0.000001f)
    {
        right_ = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else
    {
        right_ = DirectX::XMVector3Normalize(right_);
    }

    const float wobble_ = std::sin(GetTime() * wobbleSpeed) * wobbleAmplitude;
    const float width_ = (std::max)(trailWidth + wobble_, 0.001f);
    const DirectX::XMVECTOR centerVector_ = DirectX::XMLoadFloat3(&center_);
    const DirectX::XMVECTOR halfWidth_ = DirectX::XMVectorScale(right_, width_ * 0.5f);
    DirectX::XMStoreFloat3(&leftPosition_, DirectX::XMVectorSubtract(centerVector_, halfWidth_));
    DirectX::XMStoreFloat3(&rightPosition_, DirectX::XMVectorAdd(centerVector_, halfWidth_));
    return true;
}

REGISTER_COMPONENT(AfterimageTrailComponent, "AfterimageTrailComponent")
