#include "PrecompiledHeader.h"

#include "FxaaSystem.h"

#include <cstring>
#include <DirectXMath.h>

#include "Engine/Graphics/Core/CPUResourceLoader.h"

bool FxaaSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    if (!device_ || width_ <= 0 || height_ <= 0)
    {
        return false;
    }
    width = width_;
    height = height_;
    if (FAILED(CPUResourceLoader::LoadVertexShader(device_, "Data/Shader/FullScreenVS.cso",
        nullptr, 0, nullptr, fullScreenVs.GetAddressOf())))
    {
        return false;
    }
    if (FAILED(CPUResourceLoader::LoadPixelShader(device_, "Data/Shader/FxaaPS.cso",
        pixelShader.GetAddressOf())))
    {
        return false;
    }

    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(DirectX::XMFLOAT4);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void FxaaSystem::Shutdown()
{
    fullScreenVs.Reset();
    pixelShader.Reset();
    constantBuffer.Reset();
}

void FxaaSystem::Execute(const RenderContext& rc_, ID3D11ShaderResourceView* sceneSRV_,
    ID3D11RenderTargetView* outputRTV_, const Parameters& parameters_)
{
    if (!rc_.deviceContext || !rc_.renderState || !sceneSRV_ || !outputRTV_ ||
        !pixelShader || !constantBuffer)
    {
        return;
    }

    const DirectX::XMFLOAT4 constants_ {
        1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height),
        parameters_.edgeThreshold, parameters_.edgeThresholdMin };
    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (FAILED(rc_.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        return;
    }
    std::memcpy(mapped_.pData, &constants_, sizeof(constants_));
    rc_.deviceContext->Unmap(constantBuffer.Get(), 0);

    D3D11_VIEWPORT viewport_{};
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MaxDepth = 1.0f;
    auto* dc_ = rc_.deviceContext;
    dc_->RSSetViewports(1, &viewport_);
    dc_->RSSetState(rc_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rc_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc_->OMSetRenderTargets(1, &outputRTV_, nullptr);
    ID3D11SamplerState* sampler_ = rc_.renderState->GetSamplerState(SamplerState::LinearClamp);
    dc_->IASetInputLayout(nullptr);
    ID3D11Buffer* emptyVertexBuffer_ = nullptr;
    UINT zero_ = 0;
    dc_->IASetVertexBuffers(0, 1, &emptyVertexBuffer_, &zero_, &zero_);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc_->PSSetSamplers(0, 1, &sampler_);
    dc_->PSSetShaderResources(0, 1, &sceneSRV_);
    dc_->Draw(3, 0);
    ID3D11ShaderResourceView* emptyResource_ = nullptr;
    dc_->PSSetShaderResources(0, 1, &emptyResource_);
    dc_->OMSetDepthStencilState(rc_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
}
