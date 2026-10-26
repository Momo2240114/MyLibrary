#pragma once
#include "Engine/Graphics/Core/RenderContext.h"
#include <wrl/client.h>

class LightingShader
{
public:
    LightingShader(ID3D11Device* device);
    ~LightingShader() = default;

    // 全画面に対して光を計算し、描画する
    void Execute(const RenderContext& rc,
        ID3D11ShaderResourceView* color,
        ID3D11ShaderResourceView* normal,
        ID3D11ShaderResourceView* material,
        ID3D11ShaderResourceView* emissive,
        ID3D11ShaderResourceView* depth,
        ID3D11ShaderResourceView* styleSRV_,
        ID3D11ShaderResourceView* pointLightSRV,  // 追加
        ID3D11ShaderResourceView* spotLightSRV,
        ID3D11Buffer* sceneConstantBuffer,
        ID3D11Buffer* cascadeShadowConstantBuffer,
        const DirectX::XMFLOAT4& toonShadeSteps_,
        const DirectX::XMFLOAT4& toonShadeFeathers_,
        const DirectX::XMFLOAT4& toonPbrShadeSteps_,
        const DirectX::XMFLOAT4& toonPbrShadeFeathers_);

private:
    struct CbDeferredSettings
    {
        DirectX::XMFLOAT4 toonShadeSteps;
        DirectX::XMFLOAT4 toonShadeFeathers;
        DirectX::XMFLOAT4 toonPbrShadeSteps;
        DirectX::XMFLOAT4 toonPbrShadeFeathers;
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        lightConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        deferredSettingsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samplerState;
};
