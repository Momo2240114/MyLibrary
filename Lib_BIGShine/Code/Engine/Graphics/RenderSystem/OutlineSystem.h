#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class OutlineSystem
{
public:
    struct Parameters
    {
        DirectX::XMFLOAT4 outlineColor = { 0.02f, 0.02f, 0.025f, 1.0f };
        float outlineThickness = 1.0f;
        float depthThreshold = 0.002f;
        float normalThreshold = 0.25f;
        float intensity = 1.0f;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* normalSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& params_);

private:
    struct CbOutline
    {
        DirectX::XMFLOAT4 outlineColor;
        DirectX::XMFLOAT2 texelSize;
        float outlineThickness;
        float depthThreshold;
        float normalThreshold;
        float intensity;
        DirectX::XMFLOAT2 padding;
    };

    void UpdateConstantBuffer(ID3D11DeviceContext* dc_, const Parameters& params_);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> outlinePS;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVS;

    DirectX::XMFLOAT2 screenSize = { 0.0f, 0.0f };
};