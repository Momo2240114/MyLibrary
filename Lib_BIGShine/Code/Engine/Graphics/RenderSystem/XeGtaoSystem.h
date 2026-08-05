#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/RenderContext.h"

class XeGtaoSystem
{
public:
    struct Parameters
    {
        // GTAO全体の有効切り替え。
        bool enabled = false;

        // trueならAOだけを表示し、調整時に見やすくする。
        bool showOnly = false;

        // AO探索半径と最終合成強度。
        float radius = 1.0f;
        float intensity = 1.0f;
        float power = 1.5f;
        float sampleRadius = 16.0f;

        // 1フレームで使うサンプル数。多いほど滑らかだが重い。
        int sampleCount = 4;

        // 何フレームごとにAOを更新するか。
        int updateInterval = 3;
    };

    // AO計算用のTexture、CS、合成PSを準備する。
    bool Initialize(ID3D11Device* device_, int width_, int height_);
    void Shutdown();

    void Execute(
        const RenderContext& rc_,
        ID3D11ShaderResourceView* sceneSRV_,
        ID3D11ShaderResourceView* depthSRV_,
        ID3D11ShaderResourceView* normalSRV_,
        ID3D11RenderTargetView* outputRTV_,
        const Parameters& parameters_);

    // Debug表示や後続Passで参照するAO結果。
    ID3D11ShaderResourceView* GetAoSRV() const { return aoSrv.Get(); }

private:
    struct CbXeGtao
    {
        // AO計算対象のViewport情報。
        DirectX::XMINT2 viewportSize;
        DirectX::XMFLOAT2 viewportPixelSize;
        DirectX::XMFLOAT4X4 invViewProjection;

        // ParametersをGPUへ渡す値。
        float radius;
        float intensity;
        float power;
        float sampleRadius;
        int sampleCount;
        int frameIndex;
        int enabled;
        int showOnly;
    };

    // AOを書き込む低解像度または同解像度のTextureを作成する。
    bool CreateAoTarget(ID3D11Device* device_, int width_, int height_);

    // DepthからWorld位置を復元するための行列とAO設定をCBへ送る。
    void UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_);

    // 合成Pass用の全画面三角形を描く。
    void DrawFullScreenQuad(ID3D11DeviceContext* dc_);

private:
    // 入力Sceneの解像度と、実際にAOを書き込む解像度。
    int width = 0;
    int height = 0;
    int aoWidth = 0;
    int aoHeight = 0;
    int frameIndex = 0;
    bool aoReady = false;

    // AO計算結果と読み書きView。
    Microsoft::WRL::ComPtr<ID3D11Texture2D> aoTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> aoSrv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> aoUav;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

    // AO計算CS、Sceneへ合成するPS、全画面VS。
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> xeGtaoCs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> compositePs;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> fullScreenVs;
};
