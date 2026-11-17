#include "PrecompiledHeader.h"

#include "BloomSystem.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include <algorithm>

bool BloomSystem::Initialize(ID3D11Device* device, int width, int height)
{
    if (!device || width <= 0 || height <= 0) return false;
    blurTargets.clear();

    // 1. 高輝度抽出用テクスチャ (1/2サイズ)
    if (!CreateRenderTarget(device, (std::max)(width / 2, 1), (std::max)(height / 2, 1), brightTarget)) return false;
    
    ScreenSize = {
        static_cast<float>(width),static_cast<float>(height)
    };

    // 2. 川瀬式用ピンポンバッファ (さらに解像度を落とすと効果的)
    for (int i = 0; i < 2; ++i) {
        RenderTarget rt;
        if (!CreateRenderTarget(device, (std::max)(width / 2, 1), (std::max)(height / 2, 1), rt)) return false;
        blurTargets.push_back(rt);
    }

    if (FAILED(CPUResourceLoader::LoadVertexShader(
        device,
        "Data/Shader/FullScreenVS.cso",
        nullptr, 0,
        nullptr, // InputLayoutも不要
        fullScreenVS.GetAddressOf()))) return false;

    // 各ピクセルシェーダー
    if (FAILED(CPUResourceLoader::LoadPixelShader(device, "Data/Shader/BrightPassPS.cso", brightPS.GetAddressOf()))) return false;
    if (FAILED(CPUResourceLoader::LoadPixelShader(device, "Data/Shader/KawaseBlurPS.cso", kawasePS.GetAddressOf()))) return false;
    if (FAILED(CPUResourceLoader::LoadPixelShader(device, "Data/Shader/CompositePS.cso", compositePS.GetAddressOf()))) return false;
    
    // 3. 定数バッファの作成
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(CbBloom);
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, constantBuffer.GetAddressOf()))) return false;

    cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(CbColorFilter);
    if (FAILED(device->CreateBuffer(&cbDesc, nullptr, filterBuffer.GetAddressOf()))) return false;
    
    return true;
}

void BloomSystem::Shutdown()
{
    BS_PROFILE_SCOPE("BloomSystem::Shutdown");

    brightTarget.rtv.Reset();
    brightTarget.srv.Reset();
    brightTarget.width = 0.0f;
    brightTarget.height = 0.0f;
    blurTargets.clear();
    constantBuffer.Reset();
    filterBuffer.Reset();
    brightPS.Reset();
    kawasePS.Reset();
    compositePS.Reset();
    fullScreenVS.Reset();
}

void BloomSystem::Execute(
    const RenderContext& rc , 
    ID3D11ShaderResourceView* sceneSRV, 
    ID3D11RenderTargetView* finalRTV, 
    const Parameters& params)
{
    if (!rc.deviceContext || !rc.renderState || !sceneSRV || !finalRTV ||
        !brightTarget.rtv || !brightTarget.srv || blurTargets.size() != 2 ||
        !constantBuffer || !filterBuffer || !brightPS || !kawasePS || !compositePS || !fullScreenVS) return;
    auto dc = rc.deviceContext;
    auto rs = rc.renderState;

    // 共通ステート設定
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

    // サンプラーを Clamp に固定！
    ID3D11SamplerState* sampler = rs->GetSamplerState(SamplerState::LinearClamp);
    dc->PSSetSamplers(0, 1, &sampler);

    float clearCol[4] = { 0, 0, 0, 1 };

    // 1. Bright Pass
    dc->ClearRenderTargetView(brightTarget.rtv.Get(), clearCol); // 追加

    // ヘルパー：ビューポートをセットする
    auto SetVP = [&](float w, float h) {
        D3D11_VIEWPORT vp = { 0.0f, 0.0f, w, h, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vp);
        };

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };


    // --- 1. Bright Pass (sceneSRV -> brightTarget) ---
    SetVP(brightTarget.width, brightTarget.height);
    dc->OMSetRenderTargets(1, brightTarget.rtv.GetAddressOf(), nullptr);
    UpdateConstantBuffer(dc, params, 0, brightTarget.width, brightTarget.height);

    dc->PSSetShader(brightPS.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf()); // ★これを追加（b0スロット）
    dc->PSSetShaderResources(0, 1, &sceneSRV);
    DrawFullScreenQuad(dc);

    // 次のパスのために t0 を解除
    dc->PSSetShaderResources(0, 1, nullSRVs);

    // --- 2. Kawase Blur Pass ---
    ID3D11ShaderResourceView* currentSRV = brightTarget.srv.Get();

    for (int i = 0; i < params.blurPasses; ++i) {
        int destIdx = i % 2;

        // ★重要：描き込み先のサイズを正確に反映させる
        float targetW = blurTargets[destIdx].width;
        float targetH = blurTargets[destIdx].height;

        SetVP(targetW, targetH); // ここがズレているとチカチカします
        dc->OMSetRenderTargets(1, blurTargets[destIdx].rtv.GetAddressOf(), nullptr);

        // 定数バッファにも同じサイズを渡す（g_texSize の計算のため）
        UpdateConstantBuffer(dc, params, (float)i, targetW, targetH);

        dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

        dc->PSSetShader(kawasePS.Get(), nullptr, 0);
        dc->PSSetShaderResources(0, 1, &currentSRV); // 1回前の結果を入力
        DrawFullScreenQuad(dc);

        // 入力を解除
        dc->PSSetShaderResources(0, 1, nullSRVs);

        // ★重要：今描いた結果を「次の回の入力」に更新する
        currentSRV = blurTargets[destIdx].srv.Get();
    }

    // --- 3. Composite (sceneSRV + currentSRV -> finalRTV) ---
    // ここで元の画面サイズ(width, height)に戻す
    SetVP(ScreenSize.x, ScreenSize.y); // ★ここを実際の画面解像度の変数に！
    dc->OMSetRenderTargets(1, &finalRTV, nullptr);

    dc->PSSetShader(compositePS.Get(), nullptr, 0);
    ID3D11ShaderResourceView* inputs[] = { sceneSRV, currentSRV };
    CbColorFilter neutralFilter_{};
    neutralFilter_.filterIntensity = 0.0f;
    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(dc->Map(filterBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        memcpy(mapped_.pData, &neutralFilter_, sizeof(CbColorFilter));
        dc->Unmap(filterBuffer.Get(), 0);
    }
    ID3D11Buffer* cbs_[] = { constantBuffer.Get(), filterBuffer.Get() };
    dc->PSSetConstantBuffers(0, 2, cbs_);
    dc->PSSetShaderResources(0, 2, inputs);
    DrawFullScreenQuad(dc);

    // 全て解除
    dc->PSSetShaderResources(0, 2, nullSRVs);

    // ★ ここでデプスステートを「テストあり・書き込みなし」等、
    // 次の描画（ナビメッシュ）が期待する状態に戻しておくとより安全です
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // 全て解除
    dc->PSSetShaderResources(0, 2, nullSRVs);

}

void BloomSystem::Execute(
    const RenderContext& rc,
    ID3D11ShaderResourceView* sceneSRV,
    ID3D11RenderTargetView* finalRTV,
    const Parameters& params,
const CbColorFilter& colorFilterParams) // 引数名を修正
{
    if (!rc.deviceContext || !rc.renderState || !sceneSRV || !finalRTV ||
        !brightTarget.rtv || !brightTarget.srv || blurTargets.size() != 2 ||
        !constantBuffer || !filterBuffer || !brightPS || !kawasePS || !compositePS || !fullScreenVS) return;
    auto dc = rc.deviceContext;
    auto rs = rc.renderState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> previousDSV_;
    dc->OMGetRenderTargets(0, nullptr, previousDSV_.GetAddressOf());

    // --- 0. 共通ステート設定 ---
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    ID3D11SamplerState* sampler = rs->GetSamplerState(SamplerState::LinearClamp);
    dc->PSSetSamplers(0, 1, &sampler);

    float clearCol[4] = { 0, 0, 0, 1 };
    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    const bool bloomEnabled_ = params.enabled && params.intensity > 0.0001f && params.blurPasses > 0;
    Parameters safeParams_ = params;
    if (!bloomEnabled_)
    {
        safeParams_.intensity = 0.0f;
        safeParams_.blurPasses = 0;
    }

    auto SetVP = [&](float w, float h) {
        D3D11_VIEWPORT vp = { 0.0f, 0.0f, w, h, 0.0f, 1.0f };
        dc->RSSetViewports(1, &vp);
        };

    ID3D11ShaderResourceView* currentSRV = brightTarget.srv.Get();
    if (bloomEnabled_)
    {
        // --- 1. Bright Pass (sceneSRV -> brightTarget) ---
        dc->ClearRenderTargetView(brightTarget.rtv.Get(), clearCol);
        SetVP(brightTarget.width, brightTarget.height);
        dc->OMSetRenderTargets(1, brightTarget.rtv.GetAddressOf(), nullptr);

        UpdateConstantBuffer(dc, safeParams_, 0, brightTarget.width, brightTarget.height);

        dc->PSSetShader(brightPS.Get(), nullptr, 0);
        dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
        dc->PSSetShaderResources(0, 1, &sceneSRV);
        DrawFullScreenQuad(dc);
        dc->PSSetShaderResources(0, 1, nullSRVs);

        // --- 2. Kawase Blur Pass ---
        for (int i = 0; i < safeParams_.blurPasses; ++i) {
            int destIdx = i % 2;
            float targetW = blurTargets[destIdx].width;
            float targetH = blurTargets[destIdx].height;

            SetVP(targetW, targetH);
            dc->OMSetRenderTargets(1, blurTargets[destIdx].rtv.GetAddressOf(), nullptr);

            UpdateConstantBuffer(dc, safeParams_, (float)i, targetW, targetH);
            dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
            dc->PSSetShader(kawasePS.Get(), nullptr, 0);
            dc->PSSetShaderResources(0, 1, &currentSRV);
            DrawFullScreenQuad(dc);

            dc->PSSetShaderResources(0, 1, nullSRVs);
            currentSRV = blurTargets[destIdx].srv.Get();
        }
    }

    // --- 3. Composite & Color Filter (sceneSRV + Bloom -> finalRTV) ---
    // 解像度をメンバ変数 ScreenSize から正しく取得
    SetVP(ScreenSize.x, ScreenSize.y);

    // デプスバッファを戻して、後の描画（UIやデバッグ形状）が正しく遮蔽されるようにする
    dc->OMSetRenderTargets(1, &finalRTV, previousDSV_.Get());

    // ColorFilter 用定数バッファの更新
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(dc->Map(filterBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &colorFilterParams, sizeof(CbColorFilter));
        dc->Unmap(filterBuffer.Get(), 0);
    }

    dc->PSSetShader(compositePS.Get(), nullptr, 0);

    // b0: BloomParams, b1: ColorFilterParams
    UpdateConstantBuffer(dc, safeParams_, 0.0f, ScreenSize.x, ScreenSize.y);
    ID3D11Buffer* cbs[] = { constantBuffer.Get(), filterBuffer.Get() };
    dc->PSSetConstantBuffers(0, 2, cbs);

    // t0: 原画, t1: ブルーム結果
    ID3D11ShaderResourceView* inputs[] = { sceneSRV, currentSRV };
    dc->PSSetShaderResources(0, 2, inputs);

    DrawFullScreenQuad(dc);

    // --- 4. クリーンアップ ---
    dc->PSSetShaderResources(0, 2, nullSRVs);
    // 深度テストを有効に戻す（スプライトやナビメッシュ描画用）
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);
}


bool BloomSystem::CreateRenderTarget(ID3D11Device* device, int w, int h, RenderTarget& outRT) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR対応必須
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf()))) return false;
    outRT.rtv.Reset();
    outRT.srv.Reset();
    if (FAILED(device->CreateRenderTargetView(tex.Get(), nullptr, outRT.rtv.GetAddressOf()))) return false;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, outRT.srv.GetAddressOf()))) return false;
    outRT.width = (float)w;
    outRT.height = (float)h;
    return true;
}

void BloomSystem::UpdateConstantBuffer(ID3D11DeviceContext* dc, const Parameters& params, float offset, float w, float h) 
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(dc->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        CbBloom* cb = (CbBloom*)mapped.pData;
        cb->threshold = params.threshold;
        cb->intensity = params.intensity;
        cb->kawaseScale = params.kawaseScale;
        cb->offset = offset;
        cb->texSize = { 1.0f / w, 1.0f / h };
        cb->exposure = params.exposure;
        cb->toneMappingEnabled = params.toneMappingEnabled ? 1.0f : 0.0f;
        dc->Unmap(constantBuffer.Get(), 0);
    }
}

void BloomSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc)
{
    // 頂点バッファなしで3頂点描画（シェーダー側で座標計算）
    ID3D11Buffer* emptyVertexBuffer_ = nullptr;
    UINT zero_ = 0;
    dc->IASetInputLayout(nullptr);
    dc->IASetVertexBuffers(0, 1, &emptyVertexBuffer_, &zero_, &zero_);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->VSSetShader(fullScreenVS.Get(), nullptr, 0);
    dc->Draw(3, 0);
}


