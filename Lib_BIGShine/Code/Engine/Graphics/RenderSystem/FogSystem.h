#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>

#include "Engine/Graphics/Core/RenderContext.h"

class FogSystem
{
public:
    struct Parameters
    {
        bool enabled = false;
        DirectX::XMFLOAT3 color{ 0.42f, 0.48f, 0.58f };
        float startDistance = 35.0f;
        float endDistance = 160.0f;
        float density = 0.35f;
        std::string texturePath;
        DirectX::XMFLOAT2 textureTiling{ 1.0f, 1.0f };
        float textureStrength = 0.0f;
        float hatchStrength = 0.0f;
        float hatchScale = 1.0f;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();
    void Execute(const RenderContext& rc_, ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_, ID3D11RenderTargetView* outputRTV_, const Parameters& parameters_);

private:
    struct CbFog
    {
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT4 distances;
        DirectX::XMFLOAT4 controls;
        DirectX::XMFLOAT4 texture;
    };

    static_assert(sizeof(CbFog) == 64);

    int width = 0;
    int height = 0;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fogTexture;
    std::string loadedTexturePath;
};
