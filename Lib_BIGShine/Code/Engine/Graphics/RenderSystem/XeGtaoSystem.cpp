#include "PrecompiledHeader.h"

#include "XeGtaoSystem.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Debug/TracyProfiler.h"

bool XeGtaoSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    // AOは半解像度で計算し、最後にSceneへ合成して負荷を抑える。
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

    // Depth/NormalからAOを書き出すCompute Shader。
    if (FAILED(CPUResourceLoader::LoadComputeShader(device_, "Shader/XeGTAO_Post_CS.cso", xeGtaoCs.GetAddressOf())))
    {
        return false;
    }

    // AO TextureをScene Colorへ合成するPixel Shader。
    if (FAILED(CPUResourceLoader::LoadPixelShader(device_, "Shader/XeGTAO_CompositePS.cso", compositePs.GetAddressOf())))
    {
        return false;
    }

    // 合成Pass用の全画面三角形VS。
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

void XeGtaoSystem::Shutdown()
{
    BS_PROFILE_SCOPE("XeGtaoSystem::Shutdown");

    aoUav.Reset();
    aoSrv.Reset();
    aoTexture.Reset();
    constantBuffer.Reset();
    xeGtaoCs.Reset();
    compositePs.Reset();
    fullScreenVs.Reset();
    aoReady = false;
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
        // 必須リソースが欠けている場合は何もせず、呼び出し側の描画を壊さない。
        return;
    }

    const bool aoActive_ = parameters_.enabled && parameters_.intensity > 0.0f && depthSRV_ != nullptr && normalSRV_ != nullptr;
    const int updateInterval_ = parameters_.updateInterval > 0 ? parameters_.updateInterval : 1;
    // updateIntervalでAO更新頻度を落とし、静止気味の場面でコストを抑える。
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

        // 8x8スレッドグループ前提で、半解像度AO Target全体を覆う。
        const UINT groupCountX_ = static_cast<UINT>((aoWidth + 7) / 8);
        const UINT groupCountY_ = static_cast<UINT>((aoHeight + 7) / 8);
        dc_->Dispatch(groupCountX_, groupCountY_, 1);

        // CSのSRV/UAV参照を外し、後続の合成Passで同じTextureを安全に読めるようにする。
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
        // AOが使えない時もCompositePSを通し、SceneSRVをそのまま出せる状態にする。
        compositeParameters_.enabled = false;
        compositeParameters_.showOnly = false;
    }
    UpdateConstantBuffer(rc_, compositeParameters_);

    // 最終出力先全体へSceneColor + AOを描き込む。
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
    // t0にSceneColor、t1にAO。AOが無効ならnullptrを渡す。
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
    // AOは1チャンネルfloatで十分なので、R32_FLOATとして確保する。
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
    // CSはAO Target解像度で走るため、ViewportもAO側のサイズを渡す。
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
        // DepthからWorld座標を復元するため、ViewProjectionの逆行列を送る。
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
    // FullScreenVS側で頂点を生成するため、入力レイアウトと頂点バッファは不要。
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->Draw(3, 0);
}
