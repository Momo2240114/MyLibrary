#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include "ParticleRessiource.h"

// 1つのEmitterをGPU Compute Shaderで更新・描画する低レベルクラス。
// ParticleComponentは複数のParticleEmitterを束ね、こちらはGPUリソースと発生計算を担当する。
class ParticleEmitter
{
public:
    // Compute Shaderの1Dispatch単位。GPU側のスレッド数と合わせる。
    static constexpr UINT NumParticleThread = 1024;
    // Bitonic Sortで使うB2/C2各シェーダーのスレッド数。
    static constexpr UINT BitonicSortB2Thread = 256;
    static constexpr UINT BitonicSortC2Thread = 512;

    ParticleEmitter(ID3D11Device* device, const ParticleSystemConstants& initialSettings, int maxParticles = 1000);

    void Update(ID3D11DeviceContext* context, float dt);
    void Render(ID3D11DeviceContext* context);
    void PrepareRender(ID3D11DeviceContext* context);

    void ApplySettings(const ParticleSystemConstants& nextSettings);
    void SetEmissionEnabled(bool enabled_) { emissionEnabled = enabled_; }
    void SetDepthSortEnabled(bool enabled_) { depthSortEnabled = enabled_; }
    void SetDepthSortInterval(UINT interval_) { depthSortInterval = interval_ > 0 ? interval_ : 1; }
    ParticleSystemConstants& GetSettings() { return settings; }
    const ParticleSystemConstants& GetSettings() const { return settings; }

    void SetTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { textureSRV = srv; }
    void SetMaskTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { maskTextureSRV = srv; settings.useMaskTexture = srv ? 1u : 0u; }
    void SetGradientTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { gradientTextureSRV = srv; settings.useGradientTexture = srv ? 1u : 0u; }
    void SetMeshSpawnPositions(const std::vector<DirectX::XMFLOAT3>& positions_) { meshSpawnPositions = positions_; }
    std::vector<DirectX::XMFLOAT3>& GetMeshSpawnPositions() { return meshSpawnPositions; }
    ID3D11ShaderResourceView* GetSRV() const { return particleDataSRV.Get(); }
    ID3D11ShaderResourceView* GetHeaderSRV() const { return particleHeaderSRV.Get(); }
    ID3D11ShaderResourceView* GetTextureSRV() const { return textureSRV.Get(); }
    ID3D11ShaderResourceView* GetMaskTextureSRV() const { return maskTextureSRV.Get(); }
    ID3D11ShaderResourceView* GetGradientTextureSRV() const { return gradientTextureSRV.Get(); }
    ID3D11Buffer* GetConstantBuffer() const { return emitterConstantBuffer.Get(); }
    UINT GetMaxParticles() const { return numParticles; }

    static void InitializeSharedShaders(ID3D11Device* device);
    static void BindSharedRenderShaders(ID3D11DeviceContext* context);

private:
    using dispatch_indirect = DirectX::XMUINT3;

    // DrawInstancedIndirectへ渡す描画引数。
    struct DrawIndirect
    {
        UINT vertexCountPerInstance;  // 1インスタンスあたりの頂点数。
        UINT instanceCount;           // インスタンス数。
        UINT startVertexLocation;     // 頂点開始位置。
        UINT startInstanceLocation;   // インスタンス開始位置。
    };

    // フレームごとにCompute Shaderへ渡す共通値。
    struct CommonConstants
    {
        float elapsedTime;                    // 今フレームの経過時間。
        DirectX::XMUINT2 textureSplitCount;   // スプライトシートの列/行。
        UINT systemNumParticles;              // GPU上のParticle総数。
        UINT totalEmitCount;                  // 今フレームに生成するParticle数。
        UINT commonDummy[3];                  // 16byteアライン用。
    };

    // GPUソート用の定数。半透明描画の深度順を整える。
    struct SortConstants
    {
        UINT increment;   // 比較する要素間隔。
        UINT direction;   // ソート方向。
        UINT dummy[2];    // 16byteアライン用。
    };

    static constexpr UINT NumCurrentParticleOffset = 0;
    static constexpr UINT NumPreviousParticleOffset = NumCurrentParticleOffset + sizeof(UINT);
    static constexpr UINT NumDeadParticleOffset = NumPreviousParticleOffset + sizeof(UINT);
    static constexpr UINT EmitDispatchIndirectOffset = NumDeadParticleOffset + sizeof(UINT);
    static constexpr UINT UpdateDispatchIndirectOffset = EmitDispatchIndirectOffset + sizeof(dispatch_indirect);
    static constexpr UINT NumEmitParticleIndexOffset = UpdateDispatchIndirectOffset + sizeof(dispatch_indirect);
    static constexpr UINT DrawIndirectOffset = NumEmitParticleIndexOffset + sizeof(UINT);
    static constexpr UINT DrawIndirectSize = DrawIndirectOffset + sizeof(DrawIndirect);

    void CreateGPUResources(ID3D11Device* device);
    void Emit(const EmitParticleData& data);
    void BuildEmitList(float dt);
    EmitParticleData MakeEmitData(UINT seed) const;
    void RunBitonicSort(ID3D11DeviceContext* context);

    static UINT RoundParticleCount(UINT count);
    static float Random01(UINT seed);
    static float Lerp(float a, float b, float t);

private:
    ParticleSystemConstants settings;          // このEmitterの編集・実行パラメータ。
    UINT numParticles = 0;                     // 実際に確保したParticle数。GPU都合で2の累乗寄りに丸める。
    UINT numEmitParticles = 0;                 // 1フレームで送れる発生データの最大数。
    bool initialized = false;                  // 初回Update時にGPUバッファを初期化済みか。
    bool emissionEnabled = true;               // falseなら新規発生だけ止め、既存Particleは更新する。
    bool depthSortEnabled = true;              // Alpha合成時だけ深度ソートする。Additiveでは省略して負荷を下げる。
    UINT depthSortInterval = 2;                // Alpha Particleの深度ソート間隔。1なら毎フレーム。
    UINT depthSortFrame = 0;                   // ソート間隔管理用カウンタ。
    float continuousAccumulator = 0.0f;        // 連続発生の小数分を次フレームへ持ち越す。
    DirectX::XMUINT2 textureSplitCount = { 1, 1 }; // 旧互換用。現在はsettings側を主に使う。

    std::vector<EmitParticleData> emitParticles;           // 今フレームGPUへ送る生成予定Particle。
    std::vector<DirectX::XMFLOAT3> meshSpawnPositions;     // Mesh Vertices発生時に使うワールド座標サンプル。

    Microsoft::WRL::ComPtr<ID3D11Buffer> emitterConstantBuffer; // Emitter設定用CB。
    Microsoft::WRL::ComPtr<ID3D11Buffer> commonConstantBuffer;  // フレーム共通値用CB。
    Microsoft::WRL::ComPtr<ID3D11Buffer> sortConstantBuffer;    // GPUソート用CB。

    Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;    // Particle本体のStructuredBuffer。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV; // 描画用読み取りビュー。
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleDataUAV;// Compute更新用書き込みビュー。

    Microsoft::WRL::ComPtr<ID3D11Buffer> unusedIndexBuffer;     // 未使用ParticleインデックスをAppend管理するバッファ。
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unusedIndexUAV; // 未使用インデックスのAppend/Consumeビュー。

    Microsoft::WRL::ComPtr<ID3D11Buffer> emitBuffer;            // CPUで作ったEmitParticleDataをGPUへ渡すバッファ。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitSRV;   // Emit_CSから読むビュー。

    Microsoft::WRL::ComPtr<ID3D11Buffer> particleHeaderBuffer;  // 生存フラグ・深度・Particle参照を持つ管理バッファ。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleHeaderSRV; // 描画・ソート読み取り用ビュー。
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleHeaderUAV;// 更新・ソート書き込み用ビュー。

    Microsoft::WRL::ComPtr<ID3D11Buffer> indirectDataBuffer;    // Dispatch/DrawIndirectの引数をまとめるバッファ。
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> indirectDataUAV;  // Compute Shaderから引数を書き換えるビュー。

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;         // Particle本体の色Texture。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTextureSRV;     // Alpha/形状制御用Mask Texture。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientTextureSRV; // Color Over Life用Gradient Texture。

    // 全Emitterで共有するShader。Emitterごとに作ると重いのでstaticで共有する。
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> initShader;
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> beginFrameShader;
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> emitShader;
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> updateShader;
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> endFrameShader;
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> sortB2Shader;
    static Microsoft::WRL::ComPtr<ID3D11ComputeShader> sortC2Shader;
    static Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    static Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
};

