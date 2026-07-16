#include "PrecompiledHeader.h"

#include "TrailMeshComponent.h"

#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include "Engine/GamePlay/Serialize/ActionRegistry.h"

namespace
{
    DirectX::XMFLOAT3 CalculateCenter(
        const DirectX::XMFLOAT3& leftPosition_,
        const DirectX::XMFLOAT3& rightPosition_)
    {
        return
        {
            (leftPosition_.x + rightPosition_.x) * 0.5f,
            (leftPosition_.y + rightPosition_.y) * 0.5f,
            (leftPosition_.z + rightPosition_.z) * 0.5f,
        };
    }

    float CalculateDistanceSquared(
        const DirectX::XMFLOAT3& firstPosition_,
        const DirectX::XMFLOAT3& secondPosition_)
    {
        const float x_ = firstPosition_.x - secondPosition_.x;
        const float y_ = firstPosition_.y - secondPosition_.y;
        const float z_ = firstPosition_.z - secondPosition_.z;
        return x_ * x_ + y_ * y_ + z_ * z_;
    }

    DirectX::XMFLOAT3 CatmullRom(
        const DirectX::XMFLOAT3& point0_,
        const DirectX::XMFLOAT3& point1_,
        const DirectX::XMFLOAT3& point2_,
        const DirectX::XMFLOAT3& point3_,
        float amount_)
    {
        DirectX::XMFLOAT3 result_{};
        DirectX::XMStoreFloat3(
            &result_,
            DirectX::XMVectorCatmullRom(
                DirectX::XMLoadFloat3(&point0_),
                DirectX::XMLoadFloat3(&point1_),
                DirectX::XMLoadFloat3(&point2_),
                DirectX::XMLoadFloat3(&point3_),
                amount_));
        return result_;
    }

    size_t CalculateRenderSampleCount(size_t sampleCount_, int smoothStep_)
    {
        if (sampleCount_ < 2) return sampleCount_;
        return (sampleCount_ - 1) * static_cast<size_t>((std::max)(smoothStep_, 1)) + 1;
    }
}

TrailMeshComponent::TrailMeshComponent()
{
    MeshVFXPart part_{};
    part_.renderEnabled = true;
    part_.useMaterialBaseTexture = false;
    part_.useMaterialNormalTexture = false;
    part_.blendMode = MeshVFXBlendMode::ADDITIVE;
    MeshVFXAsset asset_{};
    asset_.parts.push_back(part_);
    SetAsset(asset_);
}

void TrailMeshComponent::RegisterActions()
{
    auto owner_ = GetGameObject();
    if (!owner_) return;

    auto unregisterActions_ = [&](ActionRegistry& registry_)
        {
            for (const std::string& actionName_ : registeredActionNames)
            {
                registry_.RemoveAction("Trail", actionName_);
            }
        };

    unregisterActions_(owner_->GetLocalRegistry());

    if (!owner_->transform) return;

    auto parentTransform_ = owner_->transform->parent.lock();
    if (!parentTransform_) return;

    auto parentObject_ = parentTransform_->gameObject.lock();
    if (!parentObject_) return;


   unregisterActions_(parentObject_->GetLocalRegistry());

    registeredActionNames.clear();

    auto registerToRegistry_ = [&](ActionRegistry& registry_, const std::string& actionName_)
        {
            if (actionName_ == "Trail_Start")
            {
                registry_.RegisterAction("Trail", actionName_, [this]()
                    {
                        StartTrail();
                    });
            }
            else if (actionName_ == "Trail_Stop")
            {
                registry_.RegisterAction("Trail", actionName_, [this]()
                    {
                        StopTrail();
                    });
            }
            else if (actionName_ == "Trail_Clear")
            {
                registry_.RegisterAction("Trail", actionName_, [this]()
                    {
                        ClearTrail();
                    });
            }

            registeredActionNames.push_back(actionName_);
        };

    registerToRegistry_(owner_->GetLocalRegistry(), "Trail_Start");
    registerToRegistry_(owner_->GetLocalRegistry(), "Trail_Stop");
    registerToRegistry_(owner_->GetLocalRegistry(), "Trail_Clear");

    if (parentObject_)
    {
        const std::string ownerName_ = owner_->GetName();

        auto& parentRegistry_ = parentObject_->GetLocalRegistry();

        parentRegistry_.RegisterAction("Trail", ownerName_ + "_Trail_Start", [this]()
            {
                StartTrail();
            });

        parentRegistry_.RegisterAction("Trail", ownerName_ + "_Trail_Stop", [this]()
            {
                StopTrail();
            });

        parentRegistry_.RegisterAction("Trail", ownerName_ + "_Trail_Clear", [this]()
            {
                ClearTrail();
            });

        registeredActionNames.push_back(ownerName_ + "_Trail_Start");
        registeredActionNames.push_back(ownerName_ + "_Trail_Stop");
        registeredActionNames.push_back(ownerName_ + "_Trail_Clear");
    }
}


void TrailMeshComponent::Start()
{
    MeshVFXComponent::Start();
    CreateBuffers();
    RegisterActions();
}

void TrailMeshComponent::Update(float deltaTime_)
{
    MeshVFXComponent::Update(deltaTime_);
    if (deltaTime_ <= 0.0f || samples.empty()) return;

    for (TrailSample& sample_ : samples) sample_.age += deltaTime_;
    const auto removeBegin_ = std::remove_if(
        samples.begin(), samples.end(),
        [this](const TrailSample& sample_) { return sample_.age >= lifeTime; });
    if (removeBegin_ != samples.end())
    {
        samples.erase(removeBegin_, samples.end());
        meshDirty = true;
    }
}

void TrailMeshComponent::LateUpdate(float deltaTime_)
{
    (void)deltaTime_;
    if (!meshDirty) return;
    RebuildMesh();
    UploadMesh();
    meshDirty = false;
}

void TrailMeshComponent::AddSample(
    const DirectX::XMFLOAT3& leftPosition_,
    const DirectX::XMFLOAT3& rightPosition_)
{
    if (!emitting || maxSampleCount < 2) return;

    if (!samples.empty())
    {
        const DirectX::XMFLOAT3 currentCenter_ = CalculateCenter(leftPosition_, rightPosition_);
        const DirectX::XMFLOAT3 previousCenter_ =
            CalculateCenter(samples.front().leftPosition, samples.front().rightPosition);
        if (CalculateDistanceSquared(currentCenter_, previousCenter_) <
            minimumSampleDistance * minimumSampleDistance)
        {
            samples.front().leftPosition = leftPosition_;
            samples.front().rightPosition = rightPosition_;
            samples.front().age = 0.0f;
            meshDirty = true;
            return;
        }
    }

    samples.insert(samples.begin(), { leftPosition_, rightPosition_, 0.0f });
    if (samples.size() > maxSampleCount) samples.resize(maxSampleCount);
    meshDirty = true;
}

void TrailMeshComponent::Clear()
{
    samples.clear();
    vertices.clear();
    indices.clear();
    meshDirty = true;
    OnTrailCleared();
}

void TrailMeshComponent::SetLifeTime(float lifeTime_)
{
    lifeTime = (std::max)(lifeTime_, 0.001f);
}

void TrailMeshComponent::SetMaxSampleCount(size_t maxSampleCount_)
{
    maxSampleCount = (std::max)(maxSampleCount_, static_cast<size_t>(2));
    if (samples.size() > maxSampleCount) samples.resize(maxSampleCount);
    CreateBuffers();
    meshDirty = true;
}

void TrailMeshComponent::SetMinimumSampleDistance(float distance_)
{
    minimumSampleDistance = (std::max)(distance_, 0.0f);
}

void TrailMeshComponent::SetSmoothStep(int smoothStep_)
{
    smoothStep = (std::clamp)(smoothStep_, 1, 8);
    CreateBuffers();
    meshDirty = true;
}

void TrailMeshComponent::StartTrail()
{
    SetEmitting(true);
}

void TrailMeshComponent::StopTrail()
{
    SetEmitting(false);
}

void TrailMeshComponent::ClearTrail()
{
    Clear();
}

bool TrailMeshComponent::CreateBuffers()
{
    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return false;

    const size_t renderSampleCount_ = CalculateRenderSampleCount(maxSampleCount, smoothStep);

    D3D11_BUFFER_DESC vertexDescription_{};
    vertexDescription_.ByteWidth = static_cast<UINT>(
        sizeof(GltfModelCommponennt::Vertex) * renderSampleCount_ * 2);
    vertexDescription_.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription_.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_BUFFER_DESC indexDescription_{};
    indexDescription_.ByteWidth = static_cast<UINT>(
        sizeof(uint32_t) * (renderSampleCount_ - 1) * 6);
    indexDescription_.Usage = D3D11_USAGE_DYNAMIC;
    indexDescription_.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexDescription_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    vertexBuffer.Reset();
    indexBuffer.Reset();
    if (FAILED(device_->CreateBuffer(
        &vertexDescription_, nullptr, vertexBuffer.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(device_->CreateBuffer(
        &indexDescription_, nullptr, indexBuffer.ReleaseAndGetAddressOf())))
    {
        vertexBuffer.Reset();
        return false;
    }
    return true;
}

void TrailMeshComponent::RebuildMesh()
{
    vertices.clear();
    indices.clear();
    if (samples.size() < 2) return;

    std::vector<TrailSample> renderSamples_;
    const int clampedSmoothStep_ = (std::max)(smoothStep, 1);
    renderSamples_.reserve(CalculateRenderSampleCount(samples.size(), clampedSmoothStep_));

    for (size_t sampleIndex_ = 0; sampleIndex_ + 1 < samples.size(); ++sampleIndex_)
    {
        const size_t point0Index_ = sampleIndex_ > 0 ? sampleIndex_ - 1 : sampleIndex_;
        const size_t point1Index_ = sampleIndex_;
        const size_t point2Index_ = sampleIndex_ + 1;
        const size_t point3Index_ = sampleIndex_ + 2 < samples.size()
            ? sampleIndex_ + 2
            : sampleIndex_ + 1;

        for (int stepIndex_ = 0; stepIndex_ < clampedSmoothStep_; ++stepIndex_)
        {
            const float amount_ = static_cast<float>(stepIndex_) /
                static_cast<float>(clampedSmoothStep_);
            TrailSample sample_{};
            sample_.leftPosition = CatmullRom(
                samples[point0Index_].leftPosition,
                samples[point1Index_].leftPosition,
                samples[point2Index_].leftPosition,
                samples[point3Index_].leftPosition,
                amount_);
            sample_.rightPosition = CatmullRom(
                samples[point0Index_].rightPosition,
                samples[point1Index_].rightPosition,
                samples[point2Index_].rightPosition,
                samples[point3Index_].rightPosition,
                amount_);
            sample_.age = samples[point1Index_].age +
                (samples[point2Index_].age - samples[point1Index_].age) * amount_;
            renderSamples_.push_back(sample_);
        }
    }
    renderSamples_.push_back(samples.back());

    vertices.reserve(renderSamples_.size() * 2);
    indices.reserve((renderSamples_.size() - 1) * 6);
    for (size_t sampleIndex_ = 0; sampleIndex_ < renderSamples_.size(); ++sampleIndex_)
    {
        const size_t previousIndex_ = sampleIndex_ > 0 ? sampleIndex_ - 1 : sampleIndex_;
        const size_t nextIndex_ = sampleIndex_ + 1 < renderSamples_.size()
            ? sampleIndex_ + 1
            : sampleIndex_;

        const DirectX::XMFLOAT3 previousCenter_ = CalculateCenter(
            renderSamples_[previousIndex_].leftPosition,
            renderSamples_[previousIndex_].rightPosition);

        const DirectX::XMFLOAT3 nextCenter_ = CalculateCenter(
            renderSamples_[nextIndex_].leftPosition,
            renderSamples_[nextIndex_].rightPosition);

        const DirectX::XMVECTOR direction_ = DirectX::XMVector3Normalize(
            DirectX::XMVectorSubtract(
                DirectX::XMLoadFloat3(&nextCenter_),
                DirectX::XMLoadFloat3(&previousCenter_)));

        const DirectX::XMVECTOR width_ = DirectX::XMVector3Normalize(
            DirectX::XMVectorSubtract(
                DirectX::XMLoadFloat3(&renderSamples_[sampleIndex_].rightPosition),
            DirectX::XMLoadFloat3(&renderSamples_[sampleIndex_].leftPosition)));

        DirectX::XMVECTOR normal_ = DirectX::XMVector3Cross(width_, direction_);
        if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normal_)) < 0.000001f)
        {
            normal_ = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
        else
        {
            normal_ = DirectX::XMVector3Normalize(normal_);
        }

        DirectX::XMFLOAT3 normalValue_{};
        DirectX::XMFLOAT3 tangentValue_{};
        DirectX::XMStoreFloat3(&normalValue_, normal_);
        DirectX::XMStoreFloat3(&tangentValue_, direction_);
        const float v_ = static_cast<float>(sampleIndex_) /
            static_cast<float>(renderSamples_.size() - 1);

        GltfModelCommponennt::Vertex leftVertex_{};
        leftVertex_.position = renderSamples_[sampleIndex_].leftPosition;
        leftVertex_.normal = normalValue_;
        leftVertex_.tangent = { tangentValue_.x, tangentValue_.y, tangentValue_.z, 1.0f };
        leftVertex_.texcoord = { 0.0f, v_ };
        GltfModelCommponennt::Vertex rightVertex_ = leftVertex_;
        rightVertex_.position = renderSamples_[sampleIndex_].rightPosition;
        rightVertex_.texcoord = { 1.0f, v_ };
        vertices.push_back(leftVertex_);
        vertices.push_back(rightVertex_);
    }

    for (uint32_t sampleIndex_ = 0;
        sampleIndex_ + 1 < static_cast<uint32_t>(renderSamples_.size()); ++sampleIndex_)
    {
        const uint32_t currentLeft_ = sampleIndex_ * 2;
        const uint32_t currentRight_ = currentLeft_ + 1;
        const uint32_t nextLeft_ = currentLeft_ + 2;
        const uint32_t nextRight_ = currentLeft_ + 3;
        indices.insert(indices.end(),
            { currentLeft_, currentRight_, nextLeft_, currentRight_, nextRight_, nextLeft_ });
    }
}

void TrailMeshComponent::UploadMesh()
{
    if ((!vertexBuffer || !indexBuffer) && !CreateBuffers()) return;
    if (vertices.empty() || indices.empty()) return;

    ID3D11DeviceContext* context_ = GraphicsDeviceManager::Instance().GetContext();
    if (!context_) return;

    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(context_->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        std::memcpy(mapped_.pData, vertices.data(), vertices.size() * sizeof(vertices.front()));
        context_->Unmap(vertexBuffer.Get(), 0);
    }
    if (SUCCEEDED(context_->Map(indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        std::memcpy(mapped_.pData, indices.data(), indices.size() * sizeof(indices.front()));
        context_->Unmap(indexBuffer.Get(), 0);
    }
}

bool TrailMeshComponent::GetDynamicMesh(MeshVFXDynamicMesh& mesh_) const
{
    mesh_.vertexBuffer = vertexBuffer.Get();
    mesh_.indexBuffer = indexBuffer.Get();
    mesh_.indexCount = static_cast<UINT>(indices.size());
    mesh_.vertexStride = sizeof(GltfModelCommponennt::Vertex);
    return mesh_.vertexBuffer && mesh_.indexBuffer && mesh_.indexCount > 0;
}

void TrailMeshComponent::Gui()
{
    MeshVFXComponent::Gui();
    ImGui::SeparatorText("Trail Mesh");
    ImGui::Checkbox("Emitting", &emitting);
    if (ImGui::DragFloat("Life Time", &lifeTime, 0.01f, 0.001f, 10.0f)) SetLifeTime(lifeTime);
    if (ImGui::DragFloat("Minimum Sample Distance", &minimumSampleDistance, 0.001f, 0.0f, 10.0f))
    {
        SetMinimumSampleDistance(minimumSampleDistance);
    }
    int maxSampleCount_ = static_cast<int>(maxSampleCount);
    if (ImGui::DragInt("Max Sample Count", &maxSampleCount_, 1.0f, 2, 1024))
    {
        SetMaxSampleCount(static_cast<size_t>(maxSampleCount_));
    }
    int smoothStep_ = smoothStep;
    if (ImGui::DragInt("Smooth Step", &smoothStep_, 1.0f, 1, 8))
    {
        SetSmoothStep(smoothStep_);
    }
    ImGui::Text("Current Samples: %zu", samples.size());
    if (ImGui::Button("Clear Trail")) Clear();

    if (ImGui::Button("ActionRegistory"))
    {
        RegisterActions();
    }
}

void TrailMeshComponent::Serialize(nlohmann::json& json_) const
{
    MeshVFXComponent::Serialize(json_);
    json_["TrailLifeTime"] = lifeTime;
    json_["TrailMaxSampleCount"] = maxSampleCount;
    json_["TrailMinimumSampleDistance"] = minimumSampleDistance;
    json_["TrailSmoothStep"] = smoothStep;
    json_["TrailEmitting"] = emitting;
}

void TrailMeshComponent::Deserialize(const nlohmann::json& json_)
{
    MeshVFXComponent::Deserialize(json_);
    lifeTime = (std::max)(json_.value("TrailLifeTime", 0.35f), 0.001f);
    maxSampleCount = (std::max)(json_.value("TrailMaxSampleCount", static_cast<size_t>(64)),
        static_cast<size_t>(2));
    minimumSampleDistance = (std::max)(json_.value("TrailMinimumSampleDistance", 0.01f), 0.0f);
    smoothStep = (std::clamp)(json_.value("TrailSmoothStep", 2), 1, 8);
    emitting = false;
    Clear();
    CreateBuffers();
}

REGISTER_COMPONENT(TrailMeshComponent, "TrailMeshComponent")
