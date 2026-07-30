#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class VignetteSystem
{
public:
    struct Parameters
    {
        bool enabled = false;
        DirectX::XMFLOAT4 color = { 0.2f, 0.2f, 0.2f, 1.0f };
        DirectX::XMFLOAT2 center = { 0.5f, 0.5f };
        float intensity = 0.5f;
        float smoothness = 0.2f;
        float rounded = 0.0f;
        float roundness = 1.0f;
        float opacity = 1.0f;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

private:
    struct CbVignette
    {
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 center;
        DirectX::XMFLOAT4 color;
        int enabled;
        float intensity;
        float smoothness;
        float rounded;
        float roundness;
        float opacity;
        DirectX::XMFLOAT2 padding;
    };

    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    int width = 0;
    int height = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
