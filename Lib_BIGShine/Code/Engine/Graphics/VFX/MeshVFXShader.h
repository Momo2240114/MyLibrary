#pragma once

#include "MeshVFXResource.h"

#include <DirectXMath.h>
#include <cstdint>
#include <d3d11.h>
#include <wrl.h>

struct RenderContext;

class MeshVFXShader
{
public:
    bool Initialize(ID3D11Device* device_);
    void Begin(const RenderContext& renderContext_);
    void UpdateScene(const RenderContext& renderContext_);
    void UpdatePart(
        const RenderContext& renderContext_,
        const MeshVFXPart& part_,
        const MeshVFXRuntimePart& runtimePart_,
        ID3D11ShaderResourceView* baseTexture_,
        ID3D11ShaderResourceView* normalTexture_,
        const DirectX::XMMATRIX& world_,
        float partTime_,
        float alpha_);
    void End(const RenderContext& renderContext_);

private:
    struct CbScene
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 lightDirection{};
        DirectX::XMFLOAT4 lightColor{};
        DirectX::XMFLOAT4 cameraPosition{};
        DirectX::XMFLOAT4 zBufferParameters{};
        int numPointLights = 0;
        int numSpotLights = 0;
        DirectX::XMFLOAT2 scenePadding{};
        DirectX::XMFLOAT4X4 invViewProjection;
    };

    struct CbMeshVFX
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT2 baseUVSpeed;
        DirectX::XMFLOAT2 maskUVSpeed;
        DirectX::XMFLOAT2 noiseUVSpeed;
        DirectX::XMFLOAT2 uvRotationCenter;
        float time;
        float alpha;
        float normalStrength;
        float noiseStrength;
        float fresnelPower;
        float fresnelStrength;
        float dissolveThreshold;
        float baseUVRotation;
        float maskUVRotation;
        float noiseUVRotation;
        uint32_t useMask;
        uint32_t useNoise;
        uint32_t useNormal;
        uint32_t useFresnel;
        DirectX::XMUINT2 padding;
    };
    static_assert(sizeof(CbMeshVFX) % 16 == 0);

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> partConstantBuffer;
};
