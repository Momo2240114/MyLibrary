#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class MotionBlurSystem
{
public:
    struct Parameters
    {
        bool enabled = false;
        int sampleCount = 6;
        float strength = 0.6f;
        float maxVelocity = 0.035f;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

private:
    struct CbMotionBlur
    {
        DirectX::XMINT2 screenSize;
        DirectX::XMFLOAT2 screenPixelSize;
        DirectX::XMFLOAT4X4 invViewProjection;
        DirectX::XMFLOAT4X4 previousViewProjection;
        int enabled;
        int sampleCount;
        float strength;
        float maxVelocity;
    };

    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);
    void UpdatePreviousViewProjection(const RenderContext& rc_);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    int width = 0;
    int height = 0;
    bool hasPreviousViewProjection = false;
    DirectX::XMFLOAT4X4 previousViewProjection{};

    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
