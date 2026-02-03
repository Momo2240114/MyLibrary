#pragma once

// BloomSystem.h

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <DirectXMath.h>

#include "RenderContext.h"
#include "Vector.h"

class BloomSystem 
{
public:
    struct Parameters {
        float threshold = 1.0f;    // どの明るさから光らせるか
        float intensity = 1.0f;    // 光の強さ
        float kawaseScale = 1.0f;  // ぼかしの広がり具合
        int blurPasses = 4;        // ぼかしの回数
    };

    // 【新規】カラーフィルター用（Slot 1想定）
    struct CbColorFilter {
        float hueShift;
        float saturation;
        float brightness;
        float contrast;

        DirectX::XMFLOAT3 colorBalance;
        float filterIntensity;
    };

    bool Initialize(ID3D11Device* device, int width, int height);
    void Execute(
        const RenderContext& rc,
        ID3D11ShaderResourceView* sceneSRV, 
        ID3D11RenderTargetView* finalRTV,
        const Parameters& params);

    void Execute(
        const RenderContext& rc,
        ID3D11ShaderResourceView* sceneSRV, 
        ID3D11RenderTargetView* finalRTV,
        const Parameters& params,
        const CbColorFilter& colorFilterparamas );

private:
    struct RenderTarget {
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        float width, height;
    };

    void CreateRenderTarget(ID3D11Device* device, int w, int h, RenderTarget& outRT);
    void UpdateConstantBuffer(ID3D11DeviceContext* dc, const Parameters& params, float offset, float w, float h);
    // リソース
    RenderTarget brightTarget;           // 高輝度抽出用 (1/2サイズ)
    std::vector<RenderTarget> blurTargets; // 川瀬式用ピンポンバッファ
    Vector2 ScreenSize;

    // 定数バッファ（GUIパラメータ用）
    struct CbBloom {
        float threshold;
        float intensity;
        float kawaseScale;
        float offset; // 川瀬式のパスごとのオフセット
        DirectX::XMFLOAT2 texSize;
        float padding[2];
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> filterBuffer;

    // エラー解消のための追加
    void DrawFullScreenQuad(ID3D11DeviceContext* dc);

    Microsoft::WRL::ComPtr<ID3D11PixelShader>  brightPS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  kawasePS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  compositePS; // 最後に合成する用
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVS; // 全画面用VS
};