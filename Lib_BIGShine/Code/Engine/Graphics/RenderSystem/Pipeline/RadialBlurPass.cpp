// RadialBlurPass.cpp
#include "RadialBlurPass.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h" // 貴殿の環境に合わせて調整せよ

void RadialBlurPass::Initialize(ID3D11Device* device, float width, float height) 
{
    // 1. シェーダーの作成 (radial_blur_ps.hlsl を読み込む)
    CPUResourceLoader::CreatePSFromCSO(device, "radial_blur_ps.cso", m_ps.GetAddressOf());

    // 2. 定数バッファの作成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Constants);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, m_cb.GetAddressOf());

    // 3. サンプラーの作成
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&sd, m_sampler.GetAddressOf());
}

void RadialBlurPass::Execute(const RenderContext& rc, ID3D11ShaderResourceView* inputSRV, ID3D11RenderTargetView* outputRTV)
{
    auto dc = rc.deviceContext;

    // 1. レンダリングターゲットとビューポートの更新
    dc->OMSetRenderTargets(1, &outputRTV, nullptr);
    D3D11_VIEWPORT vp{ 0, 0, rc.screenWidth, rc.screenHeight, 0.0f, 1.0f };
    dc->RSSetViewports(1, &vp);

    // 2. 定数バッファの更新
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(dc->Map(m_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &m_constants, sizeof(Constants));
        dc->Unmap(m_cb.Get(), 0);
    }

    // 3. パイプライン設定
    dc->PSSetShader(m_ps.Get(), nullptr, 0);
    dc->PSSetShaderResources(0, 1, &inputSRV); // シーンテクスチャを入力
    dc->PSSetSamplers(0, 1, m_sampler.GetAddressOf());
    dc->PSSetConstantBuffers(2, 1, m_cb.GetAddressOf());
}