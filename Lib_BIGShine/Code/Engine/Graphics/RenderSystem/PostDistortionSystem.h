#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class PostDistortionSystem
{
public:
    // フルスクリーン歪み合成に必要なシェーダーと定数バッファを準備する。
    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();

    // sceneSRV_ を distortionSRV_ のオフセット情報で揺らし、outputRTV_ へ書き戻す。
    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* distortionSRV_,
        ID3D11RenderTargetView* outputRTV_);

private:
    struct CbPostDistortion
    {
        // ピクセルシェーダーで UV オフセットをピクセル単位から正規化するための画面サイズ。
        DirectX::XMFLOAT2 screenSize;
        DirectX::XMFLOAT2 padding;
    };

    // 画面サイズなど、解像度依存のパラメータを GPU へ送る。
    void UpdateConstantBuffer(const RenderContext& rc_);

    // 頂点バッファなしの大きな三角形で画面全体を覆う。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

    // 初期化時の描画解像度。リサイズ時は再初期化で更新する想定。
    int width = 0;
    int height = 0;

    // 歪み合成専用の GPU リソース。
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
