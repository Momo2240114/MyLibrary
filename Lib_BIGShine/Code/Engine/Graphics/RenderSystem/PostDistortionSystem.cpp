#include "PrecompiledHeader.h"

#include "PostDistortionSystem.h"

#include <cstring>

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Debug/TracyProfiler.h"

bool PostDistortionSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    // 画面サイズはシェーダーで UV 補正に使うため、初期化時に保持しておく。
    width = width_;
    height = height_;

    // 頂点バッファを使わず、SV_VertexID から画面全体の三角形を生成する VS。
    if (FAILED(CPUResourceLoader::LoadVertexShader(
        device_,
        "Data/Shader/FullScreenVS.cso",
        nullptr,
        0,
        nullptr,
        fullScreenVs.GetAddressOf())))
    {
        return false;
    }

    // 歪み用 SRV を参照して、シーンカラーのサンプリング位置をずらす PS。
    if (FAILED(CPUResourceLoader::LoadPixelShader(
        device_,
        "Data/Shader/PostDistortionPS.cso",
        pixelShader.GetAddressOf())))
    {
        return false;
    }

    // 解像度依存値だけなので、毎フレーム Map できる動的定数バッファとして作る。
    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbPostDistortion);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void PostDistortionSystem::Shutdown()
{
    BS_PROFILE_SCOPE("PostDistortionSystem::Shutdown");

    constantBuffer.Reset();
    pixelShader.Reset();
    fullScreenVs.Reset();
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
        // 必須リソースが 1 つでも無い場合は合成せず、呼び出し側の描画状態を壊さない。
        return;
    }

    UpdateConstantBuffer(rc_);

    dc_->RSSetState(rs_->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

    // 出力先全体を 1 パスで上書きするため、ビューポートもシステム解像度に合わせる。
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

    // t0 に元シーン、t1 に歪みマップを束ねて PS へ渡す。
    ID3D11ShaderResourceView* shaderResources_[] = { sceneSRV_, distortionSRV_ };

    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc_->PSSetSamplers(0, 1, &sampler_);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc_->PSSetShaderResources(0, 2, shaderResources_);
    DrawFullScreenQuad(dc_);

    // outputRTV_ が次のパスで SRV として使われても競合しないよう、参照を明示的に外す。
    ID3D11ShaderResourceView* nullSRVs_[2] = { nullptr, nullptr };
    dc_->PSSetShaderResources(0, 2, nullSRVs_);
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::TestOnly), 0);
}

void PostDistortionSystem::UpdateConstantBuffer(const RenderContext& rc_)
{
    CbPostDistortion cb_{};
    // ピクセル単位の歪み量を UV に変換するため、PS 側へ現在解像度を渡す。
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
    // FullScreenVS が頂点を生成するため、入力レイアウトと頂点バッファは不要。
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->Draw(3, 0);
}
