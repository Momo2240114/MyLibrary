#include "PrecompiledHeader.h"

#include "PostDistortionSystem.h"

#include <cstring>

#include "Engine/Graphics/Core/CPUResourceLoader.h"

bool PostDistortionSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    width = width_;
    height = height_;

    if (FAILED(CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/FullScreenVS.cso",
        nullptr,
        0,
        nullptr,
        fullScreenVs.GetAddressOf())))
    {
        return false;
    }

    if (FAILED(CPUResourceLoader::LoadPixelShader(
        device_,
        "Shader/PostDistortionPS.cso",
        pixelShader.GetAddressOf())))
    {
        return false;
    }

    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbPostDistortion);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void PostDistortionSystem::Execute(
    const RenderContext& rc_,
    ID3D11ShaderResourceView* sceneSRV_,
    ID3D11ShaderResourceView* distortionSRV_,
    ID3D11RenderTargetView* outputRTV_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    const RenderState* rs_ = rc_.renderState;

    if (dc_ == nullptr || rs_ == nullptr || sceneSRV_ == nullptr || distortionSRV_ == nullptr || outputRTV_ == nullptr)
    {
        return;
    }

    UpdateConstantBuffer(rc_);

    dc_->RSSetState(rs_->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    D3D11_VIEWPORT viewport_{};
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    dc_->RSSetViewports(1, &viewport_);
    dc_->OMSetRenderTargets(1, &outputRTV_, nullptr);

    ID3D11SamplerState* sampler_ = rs_->GetSamplerState(SamplerState::LinearClamp);
    ID3D11ShaderResourceView* shaderResources_[] = { sceneSRV_, distortionSRV_ };

    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc_->PSSetSamplers(0, 1, &sampler_);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc_->PSSetShaderResources(0, 2, shaderResources_);
    DrawFullScreenQuad(dc_);

    ID3D11ShaderResourceView* nullSRVs_[2] = { nullptr, nullptr };
    dc_->PSSetShaderResources(0, 2, nullSRVs_);
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::TestOnly), 0);
}

void PostDistortionSystem::UpdateConstantBuffer(const RenderContext& rc_)
{
    CbPostDistortion cb_{};
    cb_.screenSize = { static_cast<float>(width), static_cast<float>(height) };

    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(rc_.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        std::memcpy(mapped_.pData, &cb_, sizeof(cb_));
        rc_.deviceContext->Unmap(constantBuffer.Get(), 0);
    }
}

void PostDistortionSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc_)
{
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->Draw(3, 0);
}
