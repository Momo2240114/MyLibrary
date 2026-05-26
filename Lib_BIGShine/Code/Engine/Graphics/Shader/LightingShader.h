#pragma once
#include "RenderContext.h"
#include <wrl/client.h>

class LightingShader
{
public:
    LightingShader(ID3D11Device* device);
    ~LightingShader() = default;

    // ‘S‰æ–Ê‚É‘Î‚µ‚ÄŒõ‚ğŒvZ‚µA•`‰æ‚·‚é
    void Execute(const RenderContext& rc,
        ID3D11ShaderResourceView* color,
        ID3D11ShaderResourceView* normal,
        ID3D11ShaderResourceView* material,
        ID3D11ShaderResourceView* pointLightSRV,  // ’Ç‰Á
        ID3D11ShaderResourceView* spotLightSRV,
        ID3D11Buffer* sceneConstantBuffer); // b7 ‚ğó‚¯æ‚é‚½‚ß‚É’Ç‰Á   // ’Ç‰Á

private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        lightConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samplerState;
};