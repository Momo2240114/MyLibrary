#include "PrecompiledHeader.h"

#include "TrailMeshComponent.h"

#include "Editor/AssetSlotGui.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/GamePlay/Serialize/ComponentRegistry.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/UI/ImGuiColorEditor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include "Engine/GamePlay/Serialize/ActionRegistry.h"

namespace
{
    DirectX::XMFLOAT3 CalculateCenter(
        const DirectX::XMFLOAT3& leftPosition_,
        const DirectX::XMFLOAT3& rightPosition_)
    {
        // 左右端サンプルの中心を、軌跡の移動距離や補間方向の基準にする。
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
        // 生サンプル間をCatmull-Romで補間し、剣筋の帯が角ばらないようにする。
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
        // smoothStepで各サンプル間に追加点を挟むため、描画時の実サンプル数を見積もる。
        if (sampleCount_ < 2) return sampleCount_;
        return (sampleCount_ - 1) * static_cast<size_t>((std::max)(smoothStep_, 1)) + 1;
    }

    float Clamp01(float value_)
    {
        return std::clamp(value_, 0.0f, 1.0f);
    }

    DirectX::XMFLOAT3 LerpPosition(
        const DirectX::XMFLOAT3& firstPosition_,
        const DirectX::XMFLOAT3& secondPosition_,
        float amount_)
    {
        return
        {
            firstPosition_.x + (secondPosition_.x - firstPosition_.x) * amount_,
            firstPosition_.y + (secondPosition_.y - firstPosition_.y) * amount_,
            firstPosition_.z + (secondPosition_.z - firstPosition_.z) * amount_,
        };
    }
}

TrailMeshComponent::TrailMeshComponent()
{
    // TrailMeshはMeshVFX描画経路を使うので、初期状態で加算ブレンドの1パーツを持たせる。
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

    // Trail操作は所有Objectと親Objectの両方から呼べるよう、登録済みActionを整理して再登録する。
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

        parentRegistry_.RegisterAction("Trail", ownerName_ + "_Trail_Start", [weakTrail_ = std::weak_ptr<TrailMeshComponent>(owner_->GetComponent<TrailMeshComponent>())]()
            {
                const auto trail_ = weakTrail_.lock();
                const auto actor_ = trail_ ? trail_->GetGameObject() : nullptr;
                if (actor_ && actor_->manager && !actor_->IsPendingDestroy()) trail_->StartTrail();
            });

        parentRegistry_.RegisterAction("Trail", ownerName_ + "_Trail_Stop", [weakTrail_ = std::weak_ptr<TrailMeshComponent>(owner_->GetComponent<TrailMeshComponent>())]()
            {
                const auto trail_ = weakTrail_.lock();
                const auto actor_ = trail_ ? trail_->GetGameObject() : nullptr;
                if (actor_ && actor_->manager && !actor_->IsPendingDestroy()) trail_->StopTrail();
            });

        parentRegistry_.RegisterAction("Trail", ownerName_ + "_Trail_Clear", [weakTrail_ = std::weak_ptr<TrailMeshComponent>(owner_->GetComponent<TrailMeshComponent>())]()
            {
                const auto trail_ = weakTrail_.lock();
                const auto actor_ = trail_ ? trail_->GetGameObject() : nullptr;
                if (actor_ && actor_->manager && !actor_->IsPendingDestroy()) trail_->ClearTrail();
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
    BS_PROFILE_SCOPE("TrailMeshComponent::Update");
    MeshVFXComponent::Update(deltaTime_);
    if (deltaTime_ <= 0.0f) return;
    UpdateTrailParticleEmission(deltaTime_);
    if (samples.empty()) return;

    // 古いサンプルは寿命で消し、消えた場合はメッシュを作り直す。
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
    BS_PROFILE_SCOPE("TrailMeshComponent::LateUpdate");
    (void)deltaTime_;
    if (!meshDirty) return;
    // サンプル追加/寿命削除があったフレームだけ、CPUメッシュを再構築してGPUへ転送する。
    RebuildMesh();
    UploadMesh();
    meshDirty = false;
}

TRAIL_SAMPLE_RESULT TrailMeshComponent::AddSample(
    const DirectX::XMFLOAT3& leftPosition_,
    const DirectX::XMFLOAT3& rightPosition_)
{
    BS_PROFILE_SCOPE("TrailMeshComponent::AddSample");
    if (!emitting || maxSampleCount < 2) return TRAIL_SAMPLE_RESULT::REJECTED;

    if (!samples.empty())
    {
        // ほぼ同じ位置のサンプルは新規追加せず、最新サンプルを更新して頂点数の増えすぎを防ぐ。
        const TrailSample previousSample_ = samples.front();
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
            return TRAIL_SAMPLE_RESULT::UPDATED_LATEST;
        }

        if (useTrailParticle &&
            trailParticleEmissionMode == TRAIL_PARTICLE_EMISSION_MODE::DISTANCE_INTERVAL)
        {
            // 移動距離で粒子を出すモードでは、新旧サンプル間の移動量から発生位置を補間する。
            const TrailSample currentSample_{ leftPosition_, rightPosition_, 0.0f };
            const float distance_ = std::sqrt(CalculateDistanceSquared(currentCenter_, previousCenter_));
            EmitTrailParticlesAlongMovement(previousSample_, currentSample_, distance_);
        }
    }

    samples.insert(samples.begin(), { leftPosition_, rightPosition_, 0.0f });
    if (samples.size() > maxSampleCount) samples.resize(maxSampleCount);
    meshDirty = true;

    if (useTrailParticle &&
        trailParticleEmissionMode == TRAIL_PARTICLE_EMISSION_MODE::SAMPLE_INTERVAL)
    {
        // サンプル追加数を基準に粒子を出すモード。フレーム時間に左右されにくい。
        ++trailParticleAddedSampleCounter;
        const int interval_ = (std::max)(trailParticleSampleInterval, 1);
        if (trailParticleAddedSampleCounter >= interval_)
        {
            trailParticleAddedSampleCounter = 0;
            EmitTrailParticlesAtSample(samples.front(), trailParticleSpawnCount);
        }
    }
    return TRAIL_SAMPLE_RESULT::ADDED;
}

void TrailMeshComponent::Clear()
{
    BS_PROFILE_SCOPE("TrailMeshComponent::Clear");
    samples.clear();
    vertices.clear();
    indices.clear();
    meshDirty = true;
    ResetParticleEmissionState();
    OnTrailCleared();
}

void TrailMeshComponent::SetLifeTime(float lifeTime_)
{
    lifeTime = (std::max)(lifeTime_, 0.001f);
}

void TrailMeshComponent::SetCrossSectionHeight(float height_)
{
    crossSectionHeight = (std::max)(height_, 0.0f);
    CreateBuffers();
    meshDirty = true;
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
    BS_PROFILE_SCOPE("TrailMeshComponent::CreateBuffers");
    ID3D11Device* device_ = GraphicsDeviceManager::Instance().GetDevice();
    if (!device_) return false;

    const size_t renderSampleCount_ = CalculateRenderSampleCount(maxSampleCount, smoothStep);
    const size_t crossSectionVertexCount_ = crossSectionHeight > 0.0f ? 3 : 2;

    // Trailは毎フレーム形状が変わるため、VB/IBはCPU書き込み可能なDynamicバッファにする。
    D3D11_BUFFER_DESC vertexDescription_{};
    vertexDescription_.ByteWidth = static_cast<UINT>(
        sizeof(GltfModelCommponennt::Vertex) * renderSampleCount_ * crossSectionVertexCount_);
    vertexDescription_.Usage = D3D11_USAGE_DYNAMIC;
    vertexDescription_.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexDescription_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_BUFFER_DESC indexDescription_{};
    indexDescription_.ByteWidth = static_cast<UINT>(
        sizeof(uint32_t) * (renderSampleCount_ - 1) * (crossSectionVertexCount_ - 1) * 6);
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
    BS_PROFILE_SCOPE("TrailMeshComponent::RebuildMesh");
    vertices.clear();
    indices.clear();
    if (samples.size() < 2) return;

    // 生サンプルを補間して描画用サンプルへ増やし、滑らかな帯メッシュにする。
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

    const uint32_t crossSectionVertexCount_ = crossSectionHeight > 0.0f ? 3u : 2u;
    vertices.reserve(renderSamples_.size() * crossSectionVertexCount_);
    indices.reserve((renderSamples_.size() - 1) * (crossSectionVertexCount_ - 1) * 6);
    for (size_t sampleIndex_ = 0; sampleIndex_ < renderSamples_.size(); ++sampleIndex_)
    {
        // 進行方向と左右幅から法線/接線を求め、MeshVFXのマテリアル計算に渡す。
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
        if (crossSectionHeight > 0.0f)
        {
            // 左右端を地面に残し、中央の稜線を持ち上げて泥の断面を立体化する。
            auto ridgeVertex_ = leftVertex_;
            ridgeVertex_.position = CalculateCenter(leftVertex_.position, rightVertex_.position);
            ridgeVertex_.position.y += crossSectionHeight;
            ridgeVertex_.texcoord = {0.5f,v_};
            vertices.push_back(ridgeVertex_);
        }
        vertices.push_back(rightVertex_);
    }

    for (uint32_t sampleIndex_ = 0;
        sampleIndex_ + 1 < static_cast<uint32_t>(renderSamples_.size()); ++sampleIndex_)
    {
        // 平面は1面、厚み付きは稜線の左右2面をつなぐ。頂点数とGPU容量を同じ断面数で揃える。
        for (uint32_t sectionIndex_ = 0; sectionIndex_ + 1 < crossSectionVertexCount_; ++sectionIndex_)
        {
            const uint32_t currentLeft_ = sampleIndex_ * crossSectionVertexCount_ + sectionIndex_;
            const uint32_t currentRight_ = currentLeft_ + 1;
            const uint32_t nextLeft_ = currentLeft_ + crossSectionVertexCount_;
            const uint32_t nextRight_ = nextLeft_ + 1;
            indices.insert(indices.end(),
                {currentLeft_,currentRight_,nextLeft_,currentRight_,nextRight_,nextLeft_});
        }
    }
}

void TrailMeshComponent::UploadMesh()
{
    BS_PROFILE_SCOPE("TrailMeshComponent::UploadMesh");
    if ((!vertexBuffer || !indexBuffer) && !CreateBuffers()) return;
    if (vertices.empty() || indices.empty()) return;

    // RebuildMeshで作ったCPU側頂点/インデックスを、Dynamicバッファへ上書き転送する。
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

void TrailMeshComponent::ResetParticleEmissionState()
{
    BS_PROFILE_SCOPE("TrailMeshComponent::ResetParticleEmissionState");
    trailParticleTimeAccumulator = 0.0f;
    trailParticleDistanceAccumulator = 0.0f;
    trailParticleAddedSampleCounter = 0;
    if (trailParticleClearOnTrailClear)
    {
        // 生成済みのプール粒子はEffectManager側の管理なので、Trail側では今後の発生状態だけをリセットする。
    }
}

void TrailMeshComponent::UpdateTrailParticleEmission(float deltaTime_)
{
    BS_PROFILE_SCOPE("TrailMeshComponent::UpdateTrailParticleEmission");
    if (!useTrailParticle ||
        trailParticleEmissionMode != TRAIL_PARTICLE_EMISSION_MODE::TIME_INTERVAL ||
        samples.empty() ||
        trailParticleAssetPath.empty())
    {
        return;
    }
    if (!emitting && !trailParticleEmitWhileStopping) return;

    // 時間間隔モードでは、最新サンプル上に一定周期で粒子を予約する。
    trailParticleTimeAccumulator += deltaTime_;
    const float interval_ = (std::max)(trailParticleInterval, 0.001f);
    const int maxSpawn_ = (std::max)(trailParticleMaximumSpawnPerFrame, 1);
    int spawned_ = 0;
    while (trailParticleTimeAccumulator >= interval_ && spawned_ < maxSpawn_)
    {
        trailParticleTimeAccumulator -= interval_;
        const int requestCount_ = (std::min)(trailParticleSpawnCount, maxSpawn_ - spawned_);
        EmitTrailParticlesAtSample(samples.front(), requestCount_);
        spawned_ += requestCount_;
    }
    if (spawned_ >= maxSpawn_)
    {
        trailParticleTimeAccumulator = std::fmod(trailParticleTimeAccumulator, interval_);
    }
}

void TrailMeshComponent::EmitTrailParticlesAtSample(
    const TrailSample& sample_,
    int spawnCount_)
{
    BS_PROFILE_SCOPE("TrailMeshComponent::EmitTrailParticlesAtSample");
    if (!useTrailParticle || trailParticleAssetPath.empty()) return;

    const std::shared_ptr<GameObject> owner_ = GetGameObject();
    if (!owner_ || !owner_->manager) return;

    const int clampedSpawnCount_ = (std::max)(spawnCount_, 1);
    const int maxSpawn_ = (std::max)(trailParticleMaximumSpawnPerFrame, 1);
    const int spawnCountThisCall_ = (std::min)(clampedSpawnCount_, maxSpawn_);
    EffectManager& effectManager_ = owner_->GetManager().GetEffectManager();
    // 実際のParticle生成はEffectManagerへキューし、Trail側は左右端と発生幅だけを渡す。
    TrailParticleEmissionRequest request_{};
    request_.assetPath = trailParticleAssetPath;
    request_.emitterName = trailParticleEmitterName;
    request_.leftPosition = sample_.leftPosition;
    request_.rightPosition = sample_.rightPosition;
    request_.lifeTime = trailParticleLifeTime;
    request_.spawnCount = spawnCountThisCall_;
    request_.widthMin = trailParticleWidthMin;
    request_.widthMax = trailParticleWidthMax;
    request_.tipBias = trailParticleTipBias;
    request_.positionJitter = trailParticlePositionJitter;
    effectManager_.QueueTrailParticleEmission(request_);
}

void TrailMeshComponent::EmitTrailParticlesAlongMovement(
    const TrailSample& fromSample_,
    const TrailSample& toSample_,
    float distance_)
{
    BS_PROFILE_SCOPE("TrailMeshComponent::EmitTrailParticlesAlongMovement");
    if (!useTrailParticle || trailParticleAssetPath.empty()) return;

    const float intervalDistance_ = (std::max)(trailParticleDistance, 0.001f);
    trailParticleDistanceAccumulator += (std::max)(distance_, 0.0f);
    int spawned_ = 0;
    const int maxSpawn_ = (std::max)(trailParticleMaximumSpawnPerFrame, 1);
    // 前回からの移動距離が一定量を超えるたび、移動経路上に粒子を補間発生させる。
    while (trailParticleDistanceAccumulator >= intervalDistance_ && spawned_ < maxSpawn_)
    {
        trailParticleDistanceAccumulator -= intervalDistance_;
        const float movementRate_ = distance_ > 0.00001f
            ? Clamp01(1.0f - trailParticleDistanceAccumulator / distance_)
            : 1.0f;
        TrailSample sample_{};
        sample_.leftPosition = LerpPosition(fromSample_.leftPosition, toSample_.leftPosition, movementRate_);
        sample_.rightPosition = LerpPosition(fromSample_.rightPosition, toSample_.rightPosition, movementRate_);
        sample_.age = 0.0f;
        const int requestCount_ = (std::min)(trailParticleSpawnCount, maxSpawn_ - spawned_);
        EmitTrailParticlesAtSample(sample_, requestCount_);
        spawned_ += requestCount_;
    }
    if (spawned_ >= maxSpawn_)
    {
        trailParticleDistanceAccumulator = std::fmod(trailParticleDistanceAccumulator, intervalDistance_);
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
    BS_PROFILE_SCOPE("TrailMeshComponent::Gui");
    MeshVFXComponent::Gui();
    ImGui::SeparatorText("Trail Mesh");
    ImGui::Checkbox("Emitting", &emitting);
    if (ImGui::DragFloat("Cross Section Height", &crossSectionHeight, 0.02f, 0.0f, 5.0f)) SetCrossSectionHeight(crossSectionHeight);
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

    if (ImGui::Button("Register Trail Actions"))
    {
        RegisterActions();
    }

    MeshVFXAsset& asset_ = GetAsset();
    if (!asset_.parts.empty() && ImGui::TreeNode("Trail Color Ramp"))
    {
        MeshVFXPart& part_ = asset_.parts.front();
        ImGui::Checkbox("Use Trail Color Ramp", &part_.useTrailColorRamp);
        ImGui::BeginDisabled(!part_.useTrailColorRamp);
        ImGuiColorEditor::DrawColorEdit4("Old Root Color", &part_.trailOldRootColor.x);
        ImGuiColorEditor::DrawColorEdit4("Old Tip Color", &part_.trailOldTipColor.x);
        ImGuiColorEditor::DrawColorEdit4("New Root Color", &part_.trailNewRootColor.x);
        ImGuiColorEditor::DrawColorEdit4("New Tip Color", &part_.trailNewTipColor.x);
        ImGui::DragFloat("Recency Power", &part_.trailRecencyPower, 0.01f, 0.001f, 16.0f);
        ImGui::DragFloat("Tip Power", &part_.trailTipPower, 0.01f, 0.001f, 16.0f);
        ImGui::SliderFloat("Recency Intensity", &part_.trailRecencyIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Tip Intensity", &part_.trailTipIntensity, 0.0f, 1.0f);
        ImGui::SliderFloat("Old Alpha", &part_.trailOldAlpha, 0.0f, 1.0f);
        ImGui::SliderFloat("New Alpha", &part_.trailNewAlpha, 0.0f, 1.0f);
        if (ImGui::Button("Warm Slash Preset"))
        {
            part_.useTrailColorRamp = true;
            part_.trailOldRootColor = { 0.15f, 0.05f, 0.02f, 0.15f };
            part_.trailOldTipColor = { 0.9f, 0.2f, 0.05f, 0.25f };
            part_.trailNewRootColor = { 0.7f, 0.15f, 0.04f, 0.75f };
            part_.trailNewTipColor = { 1.0f, 0.8f, 0.25f, 1.0f };
        }
        ImGui::SameLine();
        if (ImGui::Button("Cool Slash Preset"))
        {
            part_.useTrailColorRamp = true;
            part_.trailOldRootColor = { 0.02f, 0.08f, 0.18f, 0.12f };
            part_.trailOldTipColor = { 0.05f, 0.35f, 0.9f, 0.25f };
            part_.trailNewRootColor = { 0.15f, 0.65f, 1.0f, 0.7f };
            part_.trailNewTipColor = { 0.8f, 1.0f, 1.0f, 1.0f };
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Trail Particle"))
    {
        ImGui::Checkbox("Use Trail Particle", &useTrailParticle);
        ImGui::BeginDisabled(!useTrailParticle);
        AssetSlotGui::Draw(
            "Particle Asset",
            AssetSlotType::JSON,
            trailParticleAssetPath,
            nullptr,
            "Data/Particle");
        ImGui::InputText("Emitter Name", &trailParticleEmitterName);
        const char* modeItems_[] = { "Time Interval", "Distance Interval", "Sample Interval" };
        int mode_ = static_cast<int>(trailParticleEmissionMode);
        if (ImGui::Combo("Emission Mode", &mode_, modeItems_, IM_ARRAYSIZE(modeItems_)))
        {
            trailParticleEmissionMode = static_cast<TRAIL_PARTICLE_EMISSION_MODE>(
                std::clamp(mode_, 0, 2));
            ResetParticleEmissionState();
        }
        ImGui::BeginDisabled(trailParticleEmissionMode != TRAIL_PARTICLE_EMISSION_MODE::TIME_INTERVAL);
        ImGui::DragFloat("Time Interval", &trailParticleInterval, 0.001f, 0.001f, 10.0f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(trailParticleEmissionMode != TRAIL_PARTICLE_EMISSION_MODE::DISTANCE_INTERVAL);
        ImGui::DragFloat("Distance Interval", &trailParticleDistance, 0.001f, 0.001f, 100.0f);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(trailParticleEmissionMode != TRAIL_PARTICLE_EMISSION_MODE::SAMPLE_INTERVAL);
        ImGui::DragInt("Sample Interval", &trailParticleSampleInterval, 1.0f, 1, 128);
        ImGui::EndDisabled();
        ImGui::DragFloat("Particle Life Time", &trailParticleLifeTime, 0.01f, 0.001f, 10.0f);
        ImGui::DragInt("Spawn Count", &trailParticleSpawnCount, 1.0f, 1, 64);
        ImGui::SliderFloat("Width Min", &trailParticleWidthMin, 0.0f, 1.0f);
        ImGui::SliderFloat("Width Max", &trailParticleWidthMax, 0.0f, 1.0f);
        ImGui::SliderFloat("Tip Bias", &trailParticleTipBias, -1.0f, 1.0f);
        ImGui::DragFloat("Position Jitter", &trailParticlePositionJitter, 0.001f, 0.0f, 10.0f);
        ImGui::DragInt("Max Spawn Per Frame", &trailParticleMaximumSpawnPerFrame, 1.0f, 1, 128);
        ImGui::Checkbox("Emit While Stopping", &trailParticleEmitWhileStopping);
        ImGui::Checkbox("Clear State On Trail Clear", &trailParticleClearOnTrailClear);
        if (ImGui::Button("Preview Particle") && !samples.empty())
        {
            EmitTrailParticlesAtSample(samples.front(), trailParticleSpawnCount);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset Particle Timer")) ResetParticleEmissionState();
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
}

void TrailMeshComponent::Serialize(nlohmann::json& json_) const
{
    BS_PROFILE_SCOPE("TrailMeshComponent::Serialize");
    MeshVFXComponent::Serialize(json_);
    // MeshVFXの見た目設定に加え、Trail固有のサンプル寿命と粒子発生設定を保存する。
    json_["TrailLifeTime"] = lifeTime;
    json_["TrailCrossSectionHeight"] = crossSectionHeight;
    json_["TrailMaxSampleCount"] = maxSampleCount;
    json_["TrailMinimumSampleDistance"] = minimumSampleDistance;
    json_["TrailSmoothStep"] = smoothStep;
    json_["TrailEmitting"] = emitting;
    json_["TrailUseParticle"] = useTrailParticle;
    json_["TrailParticleAssetPath"] = trailParticleAssetPath;
    json_["TrailParticleEmitterName"] = trailParticleEmitterName;
    json_["TrailParticleEmissionMode"] = static_cast<int>(trailParticleEmissionMode);
    json_["TrailParticleInterval"] = trailParticleInterval;
    json_["TrailParticleDistance"] = trailParticleDistance;
    json_["TrailParticleLifeTime"] = trailParticleLifeTime;
    json_["TrailParticleSampleInterval"] = trailParticleSampleInterval;
    json_["TrailParticleSpawnCount"] = trailParticleSpawnCount;
    json_["TrailParticleWidthMin"] = trailParticleWidthMin;
    json_["TrailParticleWidthMax"] = trailParticleWidthMax;
    json_["TrailParticleTipBias"] = trailParticleTipBias;
    json_["TrailParticlePositionJitter"] = trailParticlePositionJitter;
    json_["TrailParticleMaximumSpawnPerFrame"] = trailParticleMaximumSpawnPerFrame;
    json_["TrailParticleEmitWhileStopping"] = trailParticleEmitWhileStopping;
    json_["TrailParticleClearOnTrailClear"] = trailParticleClearOnTrailClear;
}

void TrailMeshComponent::Deserialize(const nlohmann::json& json_)
{
    BS_PROFILE_SCOPE("TrailMeshComponent::Deserialize");
    MeshVFXComponent::Deserialize(json_);
    // 読み込み値は描画バッファ確保に使うため、最低値を丸めてから状態を初期化する。
    lifeTime = (std::max)(json_.value("TrailLifeTime", 0.35f), 0.001f);
    crossSectionHeight = (std::max)(json_.value("TrailCrossSectionHeight", 0.0f), 0.0f);
    maxSampleCount = (std::max)(json_.value("TrailMaxSampleCount", static_cast<size_t>(64)),
        static_cast<size_t>(2));
    minimumSampleDistance = (std::max)(json_.value("TrailMinimumSampleDistance", 0.01f), 0.0f);
    smoothStep = (std::clamp)(json_.value("TrailSmoothStep", 2), 1, 8);
    emitting = false;
    useTrailParticle = json_.value("TrailUseParticle", false);
    trailParticleAssetPath = json_.value("TrailParticleAssetPath", std::string{});
    trailParticleEmitterName = json_.value("TrailParticleEmitterName", std::string{});
    trailParticleEmissionMode = static_cast<TRAIL_PARTICLE_EMISSION_MODE>(
        std::clamp(json_.value("TrailParticleEmissionMode", 1), 0, 2));
    trailParticleInterval = (std::max)(json_.value("TrailParticleInterval", 0.04f), 0.001f);
    trailParticleDistance = (std::max)(json_.value("TrailParticleDistance", 0.15f), 0.001f);
    trailParticleLifeTime = (std::max)(
        json_.value("TrailParticleLifeTime", (std::max)(lifeTime, 0.8f)),
        0.001f);
    trailParticleSampleInterval = (std::max)(json_.value("TrailParticleSampleInterval", 1), 1);
    trailParticleSpawnCount = (std::max)(json_.value("TrailParticleSpawnCount", 1), 1);
    trailParticleWidthMin = std::clamp(json_.value("TrailParticleWidthMin", 0.35f), 0.0f, 1.0f);
    trailParticleWidthMax = std::clamp(json_.value("TrailParticleWidthMax", 1.0f), 0.0f, 1.0f);
    trailParticleTipBias = std::clamp(json_.value("TrailParticleTipBias", 0.0f), -1.0f, 1.0f);
    trailParticlePositionJitter = (std::max)(json_.value("TrailParticlePositionJitter", 0.0f), 0.0f);
    trailParticleMaximumSpawnPerFrame = (std::max)(
        json_.value("TrailParticleMaximumSpawnPerFrame", 8),
        1);
    trailParticleEmitWhileStopping = json_.value("TrailParticleEmitWhileStopping", false);
    trailParticleClearOnTrailClear = json_.value("TrailParticleClearOnTrailClear", false);
    ResetParticleEmissionState();
    Clear();
    CreateBuffers();
}

REGISTER_COMPONENT(TrailMeshComponent, "TrailMeshComponent")
