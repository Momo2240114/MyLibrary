#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Graphics/Core/RenderContext.h" // 既存のものを使用

class RadialBlurSystem
{
public:
    struct Parameters {
        float blurRadius = 0.00f;
        int blurSamplingCount = 16;
        DirectX::XMFLOAT2 blurCenter = { 0.5f, 0.5f };
        float blurMaskRadius = 0.5f;
    };

    bool Initialize(ID3D11Device* device, int width, int height);

    void Execute(
        const RenderContext& rc,
        ID3D11ShaderResourceView* inputSRV,
        ID3D11RenderTargetView* outputRTV,
        const Parameters& params);

private:
    struct CbRadial {
        float blur_radius;
        int blur_sampling_count;
        DirectX::XMFLOAT2 blur_center;
        float blur_mask_radius;
        float padding[3];
    };

    void UpdateConstantBuffer(ID3D11DeviceContext* dc, const Parameters& params);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> radialBlurPS;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVS;

    DirectX::XMFLOAT2 screenSize = { 0.0f, 0.0f };
};
