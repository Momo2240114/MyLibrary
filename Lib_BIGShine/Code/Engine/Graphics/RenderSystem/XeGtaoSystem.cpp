#include "XeGtaoSystem.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"

bool XeGtaoSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    width = width_;
    height = height_;
    aoWidth = width_ > 1 ? width_ / 2 : 1;
    aoHeight = height_ > 1 ? height_ / 2 : 1;
    frameIndex = 0;
    aoReady = false;

    if (!CreateAoTarget(device_, aoWidth, aoHeight))
    {
        return false;
    }

    if (FAILED(CPUResourceLoader::LoadComputeShader(device_, "Shader/XeGTAO_Post_CS.cso", xeGtaoCs.GetAddressOf())))
    {
        return false;
    }

    if (FAILED(CPUResourceLoader::LoadPixelShader(device_, "Shader/XeGTAO_CompositePS.cso", compositePs.GetAddressOf())))
    {
        return false;
    }

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

    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbXeGtao);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void XeGtaoSystem::Execute(
    const RenderContext& rc_,
    ID3D11ShaderResourceView* sceneSRV_,
    ID3D11ShaderResourceView* depthSRV_,
    ID3D11ShaderResourceView* normalSRV_,
    ID3D11RenderTargetView* outputRTV_,
    const Parameters& parameters_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    const RenderState* rs_ = rc_.renderState;

    if (dc_ == nullptr || rs_ == nullptr || sceneSRV_ == nullptr || outputRTV_ == nullptr)
    {
        return;
    }

    const bool aoActive_ = parameters_.enabled && parameters_.intensity > 0.0f && depthSRV_ != nullptr && normalSRV_ != nullptr;
    const int updateInterval_ = parameters_.updateInterval > 0 ? parameters_.updateInterval : 1;
    const bool shouldUpdateAo_ = aoActive_ && (!aoReady || updateInterval_ <= 1 || (frameIndex % updateInterval_) == 0);

    if (shouldUpdateAo_)
    {
        UpdateConstantBuffer(rc_, parameters_);

        ID3D11ShaderResourceView* shaderResources_[] = { depthSRV_, normalSRV_ };
        ID3D11UnorderedAccessView* unorderedAccessViews_[] = { aoUav.Get() };
        ID3D11SamplerState* sampler_ = rs_->GetSamplerState(SamplerState::PointClamp);

        dc_->CSSetShader(xeGtaoCs.Get(), nullptr, 0);
        dc_->CSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
        dc_->CSSetShaderResources(0, 2, shaderResources_);
        dc_->CSSetUnorderedAccessViews(0, 1, unorderedAccessViews_, nullptr);
        dc_->CSSetSamplers(0, 1, &sampler_);

        const UINT groupCountX_ = static_cast<UINT>((aoWidth + 7) / 8);
        const UINT groupCountY_ = static_cast<UINT>((aoHeight + 7) / 8);
        dc_->Dispatch(groupCountX_, groupCountY_, 1);

        ID3D11ShaderResourceView* nullSRVs_[2] = { nullptr, nullptr };
        ID3D11UnorderedAccessView* nullUAV_ = nullptr;
        ID3D11SamplerState* nullSampler_ = nullptr;
        dc_->CSSetShader(nullptr, nullptr, 0);
        dc_->CSSetShaderResources(0, 2, nullSRVs_);
        dc_->CSSetUnorderedAccessViews(0, 1, &nullUAV_, nullptr);
        dc_->CSSetSamplers(0, 1, &nullSampler_);
        aoReady = true;
    }

    Parameters compositeParameters_ = parameters_;
    if (!aoActive_ || !aoReady)
    {
        compositeParameters_.enabled = false;
        compositeParameters_.showOnly = false;
    }
    UpdateConstantBuffer(rc_, compositeParameters_);

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
    ID3D11ShaderResourceView* shaderResources_[] = { sceneSRV_, (aoActive_ && aoReady) ? aoSrv.Get() : nullptr };

    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetShader(compositePs.Get(), nullptr, 0);
    dc_->PSSetSamplers(0, 1, &sampler_);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc_->PSSetShaderResources(0, 2, shaderResources_);
    DrawFullScreenQuad(dc_);

    ID3D11ShaderResourceView* nullSRVs_[2] = { nullptr, nullptr };
    dc_->PSSetShaderResources(0, 2, nullSRVs_);
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::TestOnly), 0);

    ++frameIndex;
}

bool XeGtaoSystem::CreateAoTarget(ID3D11Device* device_, int width_, int height_)
{
    D3D11_TEXTURE2D_DESC desc_{};
    desc_.Width = width_;
    desc_.Height = height_;
    desc_.MipLevels = 1;
    desc_.ArraySize = 1;
    desc_.Format = DXGI_FORMAT_R32_FLOAT;
    desc_.SampleDesc.Count = 1;
    desc_.Usage = D3D11_USAGE_DEFAULT;
    desc_.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    if (FAILED(device_->CreateTexture2D(&desc_, nullptr, aoTexture.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    if (FAILED(device_->CreateShaderResourceView(aoTexture.Get(), nullptr, aoSrv.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    if (FAILED(device_->CreateUnorderedAccessView(aoTexture.Get(), nullptr, aoUav.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    return true;
}

void XeGtaoSystem::UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_)
{
    if (rc_.deviceContext == nullptr || constantBuffer == nullptr)
    {
        return;
    }

    CbXeGtao cb_{};
    cb_.viewportSize = { aoWidth, aoHeight };
    cb_.viewportPixelSize = { 1.0f / static_cast<float>(aoWidth), 1.0f / static_cast<float>(aoHeight) };
    cb_.radius = parameters_.radius;
    cb_.intensity = parameters_.intensity;
    cb_.power = parameters_.power;
    cb_.sampleRadius = parameters_.sampleRadius;
    cb_.sampleCount = parameters_.sampleCount;
    if (cb_.sampleCount < 1)
    {
        cb_.sampleCount = 1;
    }
    if (cb_.sampleCount > 8)
    {
        cb_.sampleCount = 8;
    }
    cb_.frameIndex = frameIndex;
    cb_.enabled = parameters_.enabled ? 1 : 0;
    cb_.showOnly = parameters_.showOnly ? 1 : 0;

    DirectX::XMStoreFloat4x4(&cb_.invViewProjection, DirectX::XMMatrixIdentity());
    if (rc_.camera != nullptr)
    {
        const DirectX::XMMATRIX view_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetView());
        const DirectX::XMMATRIX projection_ = DirectX::XMLoadFloat4x4(&rc_.camera->GetProjection());
        const DirectX::XMMATRIX viewProjection_ = view_ * projection_;
        DirectX::XMStoreFloat4x4(&cb_.invViewProjection, DirectX::XMMatrixInverse(nullptr, viewProjection_));
    }

    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(rc_.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        memcpy(mapped_.pData, &cb_, sizeof(cb_));
        rc_.deviceContext->Unmap(constantBuffer.Get(), 0);
    }
}

void XeGtaoSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc_)
{
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->Draw(3, 0);
}
