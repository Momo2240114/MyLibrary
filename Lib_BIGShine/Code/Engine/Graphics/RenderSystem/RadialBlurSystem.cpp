#include "RadialBlurSystem.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "OutlineSystem.h"

bool RadialBlurSystem::Initialize(ID3D11Device* device, int width, int height)
{
    screenSize = {
        static_cast<float>(width),
        static_cast<float>(height)
    };

    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/FullScreenVS.cso",
        nullptr, 0,
        nullptr,
        fullScreenVS.GetAddressOf());

    CPUResourceLoader::LoadPixelShader(
        device,
        "Shader/RadialBlur_PS.cso",
        radialBlurPS.GetAddressOf());

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(CbRadial);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, constantBuffer.GetAddressOf()));
}

void RadialBlurSystem::Execute(
    const RenderContext& rc,
    ID3D11ShaderResourceView* inputSRV,
    ID3D11RenderTargetView* outputRTV,
    const Parameters& params)
{
    auto* dc = rc.deviceContext;
    auto* rs = rc.renderState;

    if (dc == nullptr || rs == nullptr || inputSRV == nullptr || outputRTV == nullptr) {
        return;
    }

    UpdateConstantBuffer(dc, params);

    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullNone));
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    ID3D11SamplerState* sampler = rs->GetSamplerState(SamplerState::LinearClamp);
    dc->PSSetSamplers(0, 1, &sampler);

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = screenSize.x;
    vp.Height = screenSize.y;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    dc->RSSetViewports(1, &vp);

    dc->OMSetRenderTargets(1, &outputRTV, nullptr);
    dc->VSSetShader(fullScreenVS.Get(), nullptr, 0);
    dc->PSSetShader(radialBlurPS.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(2, 1, constantBuffer.GetAddressOf());
    dc->PSSetShaderResources(0, 1, &inputSRV);

    DrawFullScreenQuad(dc);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    dc->PSSetShaderResources(0, 1, &nullSRV);

    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);
}

void RadialBlurSystem::UpdateConstantBuffer(ID3D11DeviceContext* dc, const Parameters& params)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(dc->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        auto* cb = static_cast<CbRadial*>(mapped.pData);
        cb->blur_radius = params.blurRadius;
        cb->blur_sampling_count = params.blurSamplingCount;
        cb->blur_center = params.blurCenter;
        cb->blur_mask_radius = params.blurMaskRadius;
        dc->Unmap(constantBuffer.Get(), 0);
    }
}

void RadialBlurSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc)
{
    dc->IASetInputLayout(nullptr);
    dc->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc->VSSetShader(fullScreenVS.Get(), nullptr, 0);
    dc->Draw(3, 0);
}
