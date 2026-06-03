//#include "RenderPipeline.h"
//
//void RenderPipeline::Initialize(ID3D11Device* device, int width, int height) {
//    D3D11_TEXTURE2D_DESC desc{};
//    desc.Width = width; desc.Height = height;
//    desc.MipLevels = 1; desc.ArraySize = 1;
//    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
//    desc.SampleDesc.Count = 1;
//    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
//
//    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
//    device->CreateTexture2D(&desc, nullptr, tex.GetAddressOf());
//    device->CreateRenderTargetView(tex.Get(), nullptr, m_intermediateRTV.GetAddressOf());
//    device->CreateShaderResourceView(tex.Get(), nullptr, m_intermediateSRV.GetAddressOf());
//}
//
//void RenderPipeline::Execute(const RenderContext& rc)
//{
//    ID3D11ShaderResourceView* currentInput = rc.baseColorSRV;
//
//    for (size_t i = 0; i < m_passes.size(); ++i) {
//        // 最後の一手ならバックバッファへ、そうでなければ中間テクスチャへ出力
//        ID3D11RenderTargetView* currentOutput = (i == m_passes.size() - 1)
//            ? rc.backBufferRTV
//            : m_intermediateRTV.Get();
//
//        m_passes[i]->Execute(rc, currentInput, currentOutput);
//
//        // 次のパスへバトンタッチ
//        currentInput = m_intermediateSRV.Get();
//    }
//}