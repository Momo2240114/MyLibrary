#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class PostDistortionSystem
{
public:
    bool Initialize(ID3D11Device* device_, int width_, int height_);

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* distortionSRV_,
        ID3D11RenderTargetView* outputRTV_);

private:
    struct CbPostDistortion
    {
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 padding;
    };

    void UpdateConstantBuffer(const RenderContext& rc_);
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    int width = 0;
    int height = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
