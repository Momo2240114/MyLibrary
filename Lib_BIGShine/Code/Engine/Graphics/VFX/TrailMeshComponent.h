#pragma once

#include "MeshVFXComponent.h"

#include "Engine/Graphics/Resource/GLTFModel.h"

#include <cstdint>
#include <string>
#include <vector>

enum class TRAIL_SAMPLE_RESULT
{
    // 最小距離に満たず、サンプルを追加しなかった。
    REJECTED,

    // 直近サンプルを更新し、無駄な頂点増加を避けた。
    UPDATED_LATEST,

    // 新しい帯の区間としてサンプルを追加した。
    ADDED,
};

// ワールド空間の左右端点ペアから、動的な MeshVFX リボンを生成する基底コンポーネント。
// 派生クラスは武器先端や身体位置などから端点を作り、AddSample へ渡すだけで軌跡化できる。
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

    // 現在のサンプル列を三角形リストへ変換し、MeshVFXShader が描ける動的メッシュとして返す。
    bool GetDynamicMesh(MeshVFXDynamicMesh& mesh_) const override;

    // 新しい左右端点を追加する。距離が近い場合は最新点の更新だけで済ませる。
    TRAIL_SAMPLE_RESULT AddSample(
        const DirectX::XMFLOAT3& leftPosition_,
        const DirectX::XMFLOAT3& rightPosition_);

    // サンプルと GPU バッファ状態を破棄し、次回追加時にメッシュを作り直す。
    void Clear();
    void SetEmitting(bool emitting_) { emitting = emitting_; }
    bool IsEmitting() const { return emitting; }

    void SetLifeTime(float lifeTime_);
    // 泥の帯を盛り上げる。0なら従来の平面Trailを使用する。
    void SetCrossSectionHeight(float height_);
    void SetMaxSampleCount(size_t maxSampleCount_);
    void SetMinimumSampleDistance(float distance_);

    // 端点列をなめらかに補間する回数。大きいほど滑らかだが頂点数が増える。
    void SetSmoothStep(int smoothStep_);

    // ActionRegistry から呼ばれる再生制御用の薄いラッパー。
    void StartTrail();
    void StopTrail();
    void ClearTrail();

protected:
    struct TrailSample
    {
        // 1 断面ぶんの左右端点。隣の Sample とつないで帯状ポリゴンにする。
        DirectX::XMFLOAT3 leftPosition{};
        DirectX::XMFLOAT3 rightPosition{};

        // 古いサンプルを lifeTime で消すための経過時間。
        float age = 0.0f;
    };

    // 派生クラスが追加状態をまとめて破棄したい時のフック。
    virtual void OnTrailCleared() {}

private:
    // 現在の頂点数・インデックス数に合わせて D3D11Buffer を作成または更新する。
    bool CreateBuffers();

    // samples から vertices/indices を再構築する。meshDirty が立った時だけ呼ぶ。
    void RebuildMesh();

    // 再構築済みの頂点・インデックスを GPU バッファへ転送する。
    void UploadMesh();

    // Trail 連動 Particle のタイマーや距離蓄積を初期状態に戻す。
    void ResetParticleEmissionState();

    // Sample 追加や移動距離に応じて、軌跡上へ Particle を発生させる。
    void UpdateTrailParticleEmission(float deltaTime_);
    void EmitTrailParticlesAtSample(const TrailSample& sample_, int spawnCount_);
    void EmitTrailParticlesAlongMovement(
        const TrailSample& fromSample_,
        const TrailSample& toSample_,
        float distance_);

private:
    // 古い順に保持する軌跡サンプル。
    std::vector<TrailSample> samples;

    // RebuildMesh で生成された描画用メッシュデータ。
    std::vector<GltfModelCommponennt::Vertex> vertices;
    std::vector<uint32_t> indices;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

    // ActionRegistry へ登録した名前。再登録時の重複や解除に使う。
    std::vector<std::string> registeredActionNames;
    size_t maxSampleCount = 64;

    // サンプルの寿命と追加判定。寿命切れや近すぎる点はメッシュから外す。
    float lifeTime = 0.35f;
    float crossSectionHeight = 0.0f; // 世界座標のY方向へ加える断面の厚み。
    float minimumSampleDistance = 0.01f;
    int smoothStep = 2;

    // emitting=false でも既存サンプルは寿命まで残る。meshDirty は再構築要求。
    bool emitting = false;
    bool meshDirty = true;

    // 軌跡に沿って Particle を発生させるためのオプション群。
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
};
