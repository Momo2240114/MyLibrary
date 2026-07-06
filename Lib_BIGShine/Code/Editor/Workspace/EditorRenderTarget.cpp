#include "EditorRenderTarget.h"

#include <algorithm>

bool EditorRenderTarget::Initialize(ID3D11Device* device_, int width_, int height_)
{
    width = (std::max)(1, width_);
    height = (std::max)(1, height_);
    Reset();

    return CreateColorTarget(device_) && CreateNormalTarget(device_) && CreateDepthTarget(device_);
}


bool EditorRenderTarget::Resize(ID3D11Device* device_, int width_, int height_)
{
    const int newWidth = (std::max)(1, width_);
    const int newHeight = (std::max)(1, height_);
    if (IsValid() && width == newWidth && height == newHeight)
    {
        return true;
    }
    return Initialize(device_, newWidth, newHeight);
}

void EditorRenderTarget::Begin(ID3D11DeviceContext* context_, const float clearColor_[4])
{
    if (!context_ || !IsValid()) return;

    ID3D11ShaderResourceView* nullSrv_ = nullptr;
    context_->PSSetShaderResources(0, 1, &nullSrv_);

    context_->ClearRenderTargetView(renderTargetView.Get(), clearColor_);
    context_->ClearRenderTargetView(normalRenderTargetView.Get(), clearColor_); // ★ノーマルもクリア

    context_->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ★ MRTで2枚セット
    ID3D11RenderTargetView* rtvs_[] = {
        renderTargetView.Get(),
        normalRenderTargetView.Get()
    };
    context_->OMSetRenderTargets(2, rtvs_, depthStencilView.Get());

    D3D11_VIEWPORT viewport_{};
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport_);
}

void EditorRenderTarget::End(ID3D11DeviceContext* context_)
{
    if (!context_) return;

    ID3D11RenderTargetView* nullRtv_ = nullptr;
    context_->OMSetRenderTargets(1, &nullRtv_, nullptr);
}

bool EditorRenderTarget::CreateNormalTarget(ID3D11Device* device_)
{
    if (!device_) return false;
    D3D11_TEXTURE2D_DESC desc_{};
    desc_.Width = static_cast<UINT>(width);
    desc_.Height = static_cast<UINT>(height);
    desc_.MipLevels = 1;
    desc_.ArraySize = 1;
    desc_.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // カラーと同じフォーマット
    desc_.SampleDesc.Count = 1;
    desc_.Usage = D3D11_USAGE_DEFAULT;
    desc_.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device_->CreateTexture2D(&desc_, nullptr, normalTexture.GetAddressOf()))) return false;
    if (FAILED(device_->CreateRenderTargetView(normalTexture.Get(), nullptr, normalRenderTargetView.GetAddressOf()))) return false;
    if (FAILED(device_->CreateShaderResourceView(normalTexture.Get(), nullptr, normalShaderResourceView.GetAddressOf()))) return false;
    return true;
}

void EditorRenderTarget::Reset()
{
    colorTexture.Reset();
    renderTargetView.Reset();
    shaderResourceView.Reset();
    normalTexture.Reset();
    normalRenderTargetView.Reset();
    normalShaderResourceView.Reset();
    depthTexture.Reset();
    depthStencilView.Reset();
    depthShaderResourceView.Reset();
}

bool EditorRenderTarget::CreateColorTarget(ID3D11Device* device_)
{
    if (!device_) return false;

    D3D11_TEXTURE2D_DESC desc_{};
    desc_.Width = static_cast<UINT>(width);
    desc_.Height = static_cast<UINT>(height);
    desc_.MipLevels = 1;
    desc_.ArraySize = 1;
    desc_.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc_.SampleDesc.Count = 1;
    desc_.Usage = D3D11_USAGE_DEFAULT;
    desc_.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device_->CreateTexture2D(&desc_, nullptr, colorTexture.GetAddressOf()))) return false;
    if (FAILED(device_->CreateRenderTargetView(colorTexture.Get(), nullptr, renderTargetView.GetAddressOf()))) return false;
    if (FAILED(device_->CreateShaderResourceView(colorTexture.Get(), nullptr, shaderResourceView.GetAddressOf()))) return false;

    return true;
}

bool EditorRenderTarget::CreateDepthTarget(ID3D11Device* device_)
{
    if (!device_) return false;

    D3D11_TEXTURE2D_DESC desc_{};
    desc_.Width = static_cast<UINT>(width);
    desc_.Height = static_cast<UINT>(height);
    desc_.MipLevels = 1;
    desc_.ArraySize = 1;
    desc_.Format = DXGI_FORMAT_R24G8_TYPELESS;
    desc_.SampleDesc.Count = 1;
    desc_.Usage = D3D11_USAGE_DEFAULT;
    desc_.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(device_->CreateTexture2D(&desc_, nullptr, depthTexture.GetAddressOf()))) return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc_{};
    dsvDesc_.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc_.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(device_->CreateDepthStencilView(depthTexture.Get(), &dsvDesc_, depthStencilView.GetAddressOf()))) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc_{};
    srvDesc_.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    srvDesc_.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc_.Texture2D.MostDetailedMip = 0;
    srvDesc_.Texture2D.MipLevels = 1;
    if (FAILED(device_->CreateShaderResourceView(depthTexture.Get(), &srvDesc_, depthShaderResourceView.GetAddressOf()))) return false;

    return true;
}
