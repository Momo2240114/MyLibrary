#pragma once

// BloomSystem.h

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Math/Vector.h"

class BloomSystem 
{
public:
    struct Parameters {
        // Bloom 全体の有効切り替え。無効時は元シーンをそのまま合成する。
        bool enabled = true;
        float threshold = 1.0f;    // どの明るさから光らせるか
        float intensity = 1.0f;    // 光の強さ
        float kawaseScale = 1.0f;  // ぼかしの広がり具合
        int blurPasses = 4;        // ぼかしの回数
        bool toneMappingEnabled = true;
        float exposure = 1.0f;
    };

    // Bloom の最終合成時に同時適用するカラーフィルター用定数。
    struct CbColorFilter {
        // 色相、彩度、明度、コントラストを 1 パスで調整する。
        float hueShift = 0.0f;
        float saturation = 1.0f;
        float brightness = 1.0f;
        float contrast = 1.0f;

        // RGB バランスと、フィルター全体をどの程度混ぜるか。
        DirectX::XMFLOAT3 colorBalance = { 1.0f, 1.0f, 1.0f };
        float filterIntensity = 1.0f;
        DirectX::XMFLOAT3 colorOffset = { 0.0f, 0.0f, 0.0f };
        float padding = 0.0f;
    };
    static_assert(sizeof(CbColorFilter) == 48);

    // 高輝度抽出、Kawase Blur、最終合成に必要な RenderTarget と Shader を作る。
    bool Initialize(ID3D11Device* device, int width, int height);
    void Shutdown();

    // Bloom のみを適用して finalRTV へ出力する基本パス。
    void Execute(
        const RenderContext& rc,
        ID3D11ShaderResourceView* sceneSRV, 
        ID3D11RenderTargetView* finalRTV,
        const Parameters& params);

    // Bloom と ColorFilter を同じ最終合成パスで適用する拡張パス。
    void Execute(
        const RenderContext& rc,
        ID3D11ShaderResourceView* sceneSRV, 
        ID3D11RenderTargetView* finalRTV,
        const Parameters& params,
        const CbColorFilter& colorFilterparamas );

    ID3D11ShaderResourceView* GetBrightSRV() const { return brightTarget.srv.Get(); }
    ID3D11ShaderResourceView* GetBlurSRV(size_t index_) const
    {
        if (index_ >= blurTargets.size()) return nullptr;
        return blurTargets[index_].srv.Get();
    }
    size_t GetBlurTargetCount() const { return blurTargets.size(); }

private:
    struct RenderTarget 
    {
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        float width, height;
    };

    // 指定サイズの RTV/SRV ペアを作成し、抽出・ぼかしパスの中間結果として使う。
    void CreateRenderTarget(ID3D11Device* device, int w, int h, RenderTarget& outRT);

    // 閾値、強度、パスごとの Kawase オフセットを GPU へ渡す。
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
        float exposure;
        float toneMappingEnabled;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> filterBuffer;

    // 頂点バッファを使わず FullScreenVS だけで画面全体を描く。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc);

    Microsoft::WRL::ComPtr<ID3D11PixelShader>  brightPS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  kawasePS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  compositePS; // 最後に合成する用
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVS; // 全画面用VS
};
