#include "PostRenderSystem.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

bool PostRenderSystem::Initialize(ID3D11Device* device, int width, int height) {
    // それぞれのシステムを初期化
    if (!bloom.Initialize(device, width, height)) return false;
    if (!radialBlur.Initialize(device, width, height))return false;
    if (!CreatePostBuffer(device, width, height, workBuffers[0])) return false;
    if (!CreatePostBuffer(device, width, height, workBuffers[1])) return false;
    return true;
}

void PostRenderSystem::Shutdown() {
    //bloom.Shutdown();
    //radialBlur.Shutdown();
}

void PostRenderSystem::Execute(const RenderContext& rc, ID3D11ShaderResourceView* sceneSRV, ID3D11RenderTargetView* finalRTV) 
{
    // 1. まず Bloom を実行
    bloom.Execute(
        rc,
        sceneSRV,
        workBuffers[0].rtv.Get(),
        GraphicsDeviceManager::Instance().GetBloomParams(),
        GraphicsDeviceManager::Instance().GetFilterParams());

    radialBlur.Execute(
        rc,
        workBuffers[0].srv.Get(),
        finalRTV,
        GraphicsDeviceManager::Instance().GetRadialBlurParams());

}
bool PostRenderSystem::CreatePostBuffer(ID3D11Device* device, int width, int height, PostBuffer& outBuffer)
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = device->CreateTexture2D(&desc, nullptr, outBuffer.texture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateRenderTargetView(outBuffer.texture.Get(), nullptr, outBuffer.rtv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->CreateShaderResourceView(outBuffer.texture.Get(), nullptr, outBuffer.srv.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

