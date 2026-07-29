#pragma once

#include "MeshVFXComponent.h"

#include "Engine/Graphics/Resource/GLTFModel.h"

#include <cstdint>
#include <string>
#include <vector>

enum class TRAIL_SAMPLE_RESULT
{
    REJECTED,
    UPDATED_LATEST,
    ADDED,
};

// Builds a dynamic MeshVFX ribbon from pairs of world-space positions.
// Derived trail components only need to feed both edge positions through AddSample.
class TrailMeshComponent : public MeshVFXComponent
{
public:
    TrailMeshComponent();

    const std::string& GetName() const override
    {
        static std::string name_ = "TrailMeshComponent";
        return name_;
    }

    void RegisterActions();
    void Start() override;
    void Update(float deltaTime_) override;
    void LateUpdate(float deltaTime_) override;
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    bool GetDynamicMesh(MeshVFXDynamicMesh& mesh_) const override;

    TRAIL_SAMPLE_RESULT AddSample(
        const DirectX::XMFLOAT3& leftPosition_,
        const DirectX::XMFLOAT3& rightPosition_);
    void Clear();
    void SetEmitting(bool emitting_) { emitting = emitting_; }
    bool IsEmitting() const { return emitting; }

    void SetLifeTime(float lifeTime_);
    void SetMaxSampleCount(size_t maxSampleCount_);
    void SetMinimumSampleDistance(float distance_);
    void SetSmoothStep(int smoothStep_);

    void StartTrail();
    void StopTrail();
    void ClearTrail();

protected:
    struct TrailSample
    {
        DirectX::XMFLOAT3 leftPosition{};
        DirectX::XMFLOAT3 rightPosition{};
        float age = 0.0f;
    };

    virtual void OnTrailCleared() {}

private:
    bool CreateBuffers();
    void RebuildMesh();
    void UploadMesh();
    void ResetParticleEmissionState();
    void UpdateTrailParticleEmission(float deltaTime_);
    void EmitTrailParticlesAtSample(const TrailSample& sample_, int spawnCount_);
    void EmitTrailParticlesAlongMovement(
        const TrailSample& fromSample_,
        const TrailSample& toSample_,
        float distance_);
    DirectX::XMFLOAT3 BuildTrailParticlePosition(const TrailSample& sample_);
    DirectX::XMFLOAT3 BuildTrailParticleNormal(const TrailSample& sample_) const;
    float GenerateTrailParticleRandom01();

private:
    std::vector<TrailSample> samples;
    std::vector<GltfModelCommponennt::Vertex> vertices;
    std::vector<uint32_t> indices;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
    std::vector<std::string> registeredActionNames;
    size_t maxSampleCount = 64;
    float lifeTime = 0.35f;
    float minimumSampleDistance = 0.01f;
    int smoothStep = 2;
    bool emitting = false;
    bool meshDirty = true;
    bool useTrailParticle = false;
    std::string trailParticleAssetPath;
    std::string trailParticleEmitterName;
    TRAIL_PARTICLE_EMISSION_MODE trailParticleEmissionMode =
        TRAIL_PARTICLE_EMISSION_MODE::DISTANCE_INTERVAL;
    float trailParticleInterval = 0.04f;
    float trailParticleDistance = 0.15f;
    float trailParticleLifeTime = 0.8f;
    int trailParticleSampleInterval = 1;
    int trailParticleSpawnCount = 1;
    float trailParticleWidthMin = 0.35f;
    float trailParticleWidthMax = 1.0f;
    float trailParticleTipBias = 0.0f;
    float trailParticlePositionJitter = 0.0f;
    int trailParticleMaximumSpawnPerFrame = 8;
    bool trailParticleEmitWhileStopping = false;
    bool trailParticleClearOnTrailClear = false;
    float trailParticleTimeAccumulator = 0.0f;
    float trailParticleDistanceAccumulator = 0.0f;
    int trailParticleAddedSampleCounter = 0;
    uint32_t trailParticleRandomState = 0x6D2B79F5u;
};
