#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Graphics/Core/RenderContext.h"

class FxaaSystem
{
public:
    struct Parameters
    {
        bool enabled = true;
        float edgeThreshold = 0.125f;
        float edgeThresholdMin = 0.0312f;
    };

    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();
    void Execute(const RenderContext& rc_, ID3D11ShaderResourceView* sceneSRV_,
        ID3D11RenderTargetView* outputRTV_, const Parameters& parameters_);

private:
    int width = 0;
    int height = 0;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
};
