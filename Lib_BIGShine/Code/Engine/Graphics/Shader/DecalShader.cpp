#include "PrecompiledHeader.h"

#include "DecalShader.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"

DecalShader::DecalShader(ID3D11Device* device)
{
    // 1. 頂点シェーダー & 入力レイアウト
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/Decal_VS.cso",
        layout,
        _countof(layout),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // 2. ピクセルシェーダー
    CPUResourceLoader::LoadPixelShader(
        device,
        "Shader/Decal_PS.cso",
        pixelShader.GetAddressOf());

    // 3. 深度サンプリング用 (Point)
    D3D11_SAMPLER_DESC pDesc = {};
    pDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    pDesc.AddressU = pDesc.AddressV = pDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&pDesc, pointSampler.GetAddressOf());

    // 4. デカール投影用 (Linear + Border Black)
    // 投影ボックスの外側が黒（透明）になるように Border を使用
    D3D11_SAMPLER_DESC lDesc = {};
    lDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    lDesc.AddressU = lDesc.AddressV = lDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    lDesc.BorderColor[0] = lDesc.BorderColor[1] = lDesc.BorderColor[2] = lDesc.BorderColor[3] = 0.0f;
    device->CreateSamplerState(&lDesc, linearBorderSampler.GetAddressOf());


}

void DecalShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(usePixelShader ? pixelShader.Get() : nullptr, nullptr, 0);

    // サンプラー設定 (s0: Point, s1: LinearBorder)
    ID3D11SamplerState* samplers[] = { pointSampler.Get(), linearBorderSampler.Get() };
    dc->PSSetSamplers(0, _countof(samplers), samplers);
}

void DecalShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    // このクラスでは ModelRenderSystem 側で直接定数バッファを Update するため、
    // mesh 単位の Update 処理は空で問題ありません。
}

void DecalShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);

    // SRV解除 (t4: Depth, t10: DecalTexture)
    ID3D11ShaderResourceView* nullSRVs[11] = { nullptr };
    dc->PSSetShaderResources(0, 11, nullSRVs);
}