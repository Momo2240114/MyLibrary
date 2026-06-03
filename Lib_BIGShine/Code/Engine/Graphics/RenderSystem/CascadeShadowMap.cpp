#include "CascadeShadowMap.h"

bool CascadeShadowMap::Initialize(ID3D11Device* device)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = ShadowmapSize;
    desc.Height = ShadowmapSize;
    desc.MipLevels = 1;
    desc.ArraySize = 1; // å¬ï Ç…ä«óùÇ∑ÇÈèÍçá
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    for (int i = 0; i < CascadeCount; ++i)
    {
        device->CreateTexture2D(&desc, nullptr, &shadowTextures[i]);
        device->CreateDepthStencilView(shadowTextures[i].Get(), nullptr, &shadowDSVs[i]);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(shadowTextures[i].Get(), &srvDesc, &shadowSRVs[i]);
    }
    return true;
}

void CascadeShadowMap::SetViewport(ID3D11DeviceContext* dc) const
{
}
