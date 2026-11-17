#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>

#include "Engine/Graphics/Core/RenderContext.h"

class VignetteSystem
{
public:
    struct Parameters
    {
        // falseならShader側でVignetteを無効化し、元画像に近い結果にする。
        bool enabled = false;
        // 画面端へ乗せる色。
        DirectX::XMFLOAT4 color = { 0.2f, 0.2f, 0.2f, 1.0f };
        // Vignetteの中心。通常は画面中央。
        DirectX::XMFLOAT2 center = { 0.5f, 0.5f };
        // 暗くする強さ。
        float intensity = 0.5f;
        // 中心から端へ変化する境界の柔らかさ。
        float smoothness = 0.2f;
        // 角丸形状へ寄せる量。
        float rounded = 0.0f;
        // 丸みの強さ。
        float roundness = 1.0f;
        // 最終的な合成Alpha。
        float opacity = 1.0f;
        float damageOpacity = 0.0f;
        std::string texturePath;
        DirectX::XMFLOAT2 textureTiling{ 1.0f, 1.0f };
        float textureStrength = 0.0f;
        float hatchStrength = 0.0f;
        float hatchScale = 1.0f;
    };

    // FullScreen ShaderとVignette用ConstantBufferを作成する。
    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();

    // sceneSRVへVignetteを乗せ、outputRTVへ書き込む。
    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

private:
    struct CbVignette
    {
        // HLSL側のcbuffer layoutに合わせた値。paddingも含めて16byte境界を保つ。
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 center;
        DirectX::XMFLOAT4 color;
        int enabled;
        float intensity;
        float smoothness;
        float rounded;
        float roundness;
        float opacity;
        float damageOpacity;
        float padding;
        DirectX::XMFLOAT4 textureParams;
        DirectX::XMFLOAT4 hatchParams;
    };
    static_assert(sizeof(CbVignette) == 96);

    // CPU側ParametersをShader用ConstantBufferへ詰める。
    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);
    // VertexBufferなしの全画面Triangleを描画する。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    // PostEffectを適用するRenderTargetのサイズ。
    int width = 0;
    int height = 0;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vignetteTexture;
    std::string loadedTexturePath;
};
