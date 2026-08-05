#include "PrecompiledHeader.h"

#include "OutlineSystem.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"

bool OutlineSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    // ViewportとtexelSize計算に使う画面Sizeを保持する。
    screenSize = {
        static_cast<float>(width_),
        static_cast<float>(height_)
    };

    // PostEffect共通のFullScreen Vertex Shaderを使う。
    CPUResourceLoader::LoadVertexShader(
        device_,
        "Shader/FullScreenVS.cso",
        nullptr,
        0,
        nullptr,
        fullScreenVS.GetAddressOf());

    // Normal/Depth差分からOutlineを検出するPixel Shaderを読み込む。
    CPUResourceLoader::LoadPixelShader(
        device_,
        "Shader/PostOutlinePS.cso",
        outlinePS.GetAddressOf());

    // Outline設定はEditorや演出で変わるためDynamic Bufferにする。
    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbOutline);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void OutlineSystem::Execute(
    const RenderContext& rc_,
    ID3D11ShaderResourceView* sceneSRV_,
    ID3D11ShaderResourceView* normalSRV_,
    ID3D11ShaderResourceView* depthSRV_,
    ID3D11RenderTargetView* outputRTV_,
    const Parameters& params_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    auto* rs_ = rc_.renderState;

    // 必須Resourceが揃わないFrameではPostEffectをSkipする。
    if (dc_ == nullptr || rs_ == nullptr || sceneSRV_ == nullptr || outputRTV_ == nullptr)
    {
        return;
    }

    UpdateConstantBuffer(dc_, params_);

    // FullScreen PassなのでDepthは無効化し、Cullもしない。
    dc_->RSSetState(rs_->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    D3D11_VIEWPORT vp_{};
    vp_.TopLeftX = 0.0f;
    vp_.TopLeftY = 0.0f;
    vp_.Width = screenSize.x;
    vp_.Height = screenSize.y;
    vp_.MinDepth = 0.0f;
    vp_.MaxDepth = 1.0f;
    dc_->RSSetViewports(1, &vp_);

    ID3D11SamplerState* sampler_ = rs_->GetSamplerState(SamplerState::LinearClamp);
    dc_->PSSetSamplers(0, 1, &sampler_);

    dc_->OMSetRenderTargets(1, &outputRTV_, nullptr);

    dc_->VSSetShader(fullScreenVS.Get(), nullptr, 0);
    dc_->PSSetShader(outlinePS.Get(), nullptr, 0);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

    ID3D11ShaderResourceView* srvs_[] =
    {
        // 0:元Scene、1:Normal Buffer、2:Depth Buffer。
        sceneSRV_,
        normalSRV_,
        depthSRV_
    };
    dc_->PSSetShaderResources(0, _countof(srvs_), srvs_);

    DrawFullScreenQuad(dc_);

    // 後続Passで同じTextureをRTVとして使えるよう、SRV/Sampler参照を解除する。
    ID3D11ShaderResourceView* nullSRVs_[] = { nullptr, nullptr, nullptr };
    dc_->PSSetShaderResources(0, _countof(nullSRVs_), nullSRVs_);

    ID3D11SamplerState* nullSampler_ = nullptr;
    dc_->PSSetSamplers(0, 1, &nullSampler_);

    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::TestOnly), 0);
}

void OutlineSystem::UpdateConstantBuffer(ID3D11DeviceContext* dc_, const Parameters& params_)
{
    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(dc_->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        // texelSizeは隣接PixelをSampleするためのUV Offsetとして使う。
        CbOutline* cb_ = static_cast<CbOutline*>(mapped_.pData);
        cb_->outlineColor = params_.outlineColor;
        cb_->texelSize = {
            1.0f / screenSize.x,
            1.0f / screenSize.y
        };
        cb_->outlineThickness = params_.outlineThickness;
        cb_->depthThreshold = params_.depthThreshold;
        cb_->normalThreshold = params_.normalThreshold;
        cb_->intensity = params_.intensity;

        dc_->Unmap(constantBuffer.Get(), 0);
    }
}

void OutlineSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc_)
{
    // FullScreenVSがSV_VertexIDから三角形を作るため、頂点入力は空にする。
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->VSSetShader(fullScreenVS.Get(), nullptr, 0);
    dc_->Draw(3, 0);
}
