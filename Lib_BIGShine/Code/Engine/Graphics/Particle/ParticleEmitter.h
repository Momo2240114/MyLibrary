#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include "ParticleRessiource.h"

class ParticleEmitter
{
public:
    static constexpr UINT NumParticleThread = 1024;
    static constexpr UINT BitonicSortB2Thread = 256;
    static constexpr UINT BitonicSortC2Thread = 512;

    ParticleEmitter(ID3D11Device* device, const ParticleSystemConstants& initialSettings, int maxParticles = 1000);

    void Update(ID3D11DeviceContext* context, float dt);
    void Render(ID3D11DeviceContext* context);
    void PrepareRender(ID3D11DeviceContext* context);

    void ApplySettings(const ParticleSystemConstants& nextSettings);
    ParticleSystemConstants& GetSettings() { return settings; }
    const ParticleSystemConstants& GetSettings() const { return settings; }

    void SetTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { textureSRV = srv; }
    void SetMaskTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { maskTextureSRV = srv; settings.useMaskTexture = srv ? 1u : 0u; }
    void SetGradientTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { gradientTextureSRV = srv; settings.useGradientTexture = srv ? 1u : 0u; }
    void SetMeshSpawnPositions(const std::vector<DirectX::XMFLOAT3>& positions_) { meshSpawnPositions = positions_; }
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

    struct DrawIndirect
    {
        UINT vertexCountPerInstance;
        UINT instanceCount;
        UINT startVertexLocation;
        UINT startInstanceLocation;
    };

    struct CommonConstants
    {
        float elapsedTime;
        DirectX::XMUINT2 textureSplitCount;
        UINT systemNumParticles;
        UINT totalEmitCount;
        UINT commonDummy[3];
    };

    struct SortConstants
    {
        UINT increment;
        UINT direction;
        UINT dummy[2];
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
    ParticleSystemConstants settings;
    UINT numParticles = 0;
    UINT numEmitParticles = 0;
    bool initialized = false;
    float continuousAccumulator = 0.0f;
    DirectX::XMUINT2 textureSplitCount = { 1, 1 };

    std::vector<EmitParticleData> emitParticles;
    std::vector<DirectX::XMFLOAT3> meshSpawnPositions;

    Microsoft::WRL::ComPtr<ID3D11Buffer> emitterConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> commonConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sortConstantBuffer;

    Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleDataUAV;

    Microsoft::WRL::ComPtr<ID3D11Buffer> unusedIndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unusedIndexUAV;

    Microsoft::WRL::ComPtr<ID3D11Buffer> emitBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitSRV;

    Microsoft::WRL::ComPtr<ID3D11Buffer> particleHeaderBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleHeaderSRV;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particleHeaderUAV;

    Microsoft::WRL::ComPtr<ID3D11Buffer> indirectDataBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> indirectDataUAV;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTextureSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gradientTextureSRV;

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

