#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Graphics/Core/RenderContext.h" // 既存のものを使用

class RadialBlurSystem
{
public:
    struct Parameters {
        // Blurの強さ。0ならほぼ元画像、値を上げるほど中心から放射状に伸びる。
        float blurRadius = 0.00f;
        // Pixel Shader側で何回Sampleするか。多いほど滑らかだが重くなる。
        int blurSamplingCount = 16;
        // Screen UV上のBlur中心。{0.5, 0.5}で画面中央。
        DirectX::XMFLOAT2 blurCenter = { 0.5f, 0.5f };
        // 中心からどの範囲へBlurを効かせるかのMask半径。
        float blurMaskRadius = 0.5f;
    };

    // FullScreen ShaderとConstantBufferを作成する。
    bool Initialize(ID3D11Device* device, int width, int height);
    void Shutdown();

    // inputSRVを読み、RadialBlurをかけた結果をoutputRTVへ描画する。
    void Execute(
        const RenderContext& rc,
        ID3D11ShaderResourceView* inputSRV,
        ID3D11RenderTargetView* outputRTV,
        const Parameters& params);

private:
    struct CbRadial {
        // Shader側のcbuffer layoutに合わせるため、命名とpaddingを固定している。
        float blur_radius;
        int blur_sampling_count;
        DirectX::XMFLOAT2 blur_center;
        float blur_mask_radius;
        float padding[3];
    };

    // CPU側ParametersをPixel Shader用ConstantBufferへ反映する。
    void UpdateConstantBuffer(ID3D11DeviceContext* dc, const Parameters& params);
    // VertexBufferなしの全画面Triangleを描画する。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc);

private:
    // RadialBlur用のGPU Resource。
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> radialBlurPS;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVS;

    DirectX::XMFLOAT2 screenSize = { 0.0f, 0.0f };
};
