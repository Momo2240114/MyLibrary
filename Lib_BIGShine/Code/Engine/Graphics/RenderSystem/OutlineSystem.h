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
        // 輪郭線として合成する色。
        DirectX::XMFLOAT4 outlineColor = { 0.02f, 0.02f, 0.025f, 1.0f };
        // 近傍Sampleの距離。大きいほど太いOutlineになる。
        float outlineThickness = 1.0f;
        // Depth差で輪郭とみなすしきい値。
        float depthThreshold = 0.002f;
        // Normal差で輪郭とみなすしきい値。
        float normalThreshold = 0.25f;
        // Outline合成の強さ。
        float intensity = 1.0f;
    };

    // FullScreen ShaderとOutline用ConstantBufferを初期化する。
    bool Initialize(ID3D11Device* device_, int width_, int height_);

    // Scene/Normal/Depthを参照して輪郭線を検出し、outputRTVへ合成する。
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
        // HLSL側のcbuffer layoutに合わせたOutline設定。
        DirectX::XMFLOAT4 outlineColor;
        DirectX::XMFLOAT2 texelSize;
        float outlineThickness;
        float depthThreshold;
        float normalThreshold;
        float intensity;
        DirectX::XMFLOAT2 padding;
    };

    // CPU側ParametersをPixel Shader用ConstantBufferへ反映する。
    void UpdateConstantBuffer(ID3D11DeviceContext* dc_, const Parameters& params_);
    // VertexBufferなしの全画面Triangleを描画する。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

private:
    // Outline Passで使うGPU Resource。
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> outlinePS;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVS;

    // PostEffect出力先の画面Size。
    DirectX::XMFLOAT2 screenSize = { 0.0f, 0.0f };
};
