#pragma once

#include "MeshVFXComponent.h"

#include "Engine/Graphics/Resource/GLTFModel.h"

#include <vector>

// Builds a dynamic MeshVFX ribbon from pairs of world-space positions.
// Derived trail components only need to feed both edges through AddSample.
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

    void AddSample(
        const DirectX::XMFLOAT3& leftPosition_,
        const DirectX::XMFLOAT3& rightPosition_);
    void Clear();
    void SetEmitting(bool emitting_) { emitting = emitting_; }
    bool IsEmitting() const { return emitting; }

    void SetLifeTime(float lifeTime_);
    void SetMaxSampleCount(size_t maxSampleCount_);
    void SetMinimumSampleDistance(float distance_);

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
    bool emitting = false;
    bool meshDirty = true;
};
