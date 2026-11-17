#include "PrecompiledHeader.h"

#include "VignetteSystem.h"

#include <cstring>

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Debug/TracyProfiler.h"

bool VignetteSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    // ViewportとShaderのUV計算に使う画面Sizeを保持する。
    width = width_;
    height = height_;

    // PostEffect共通のFullScreen Vertex Shaderを使う。
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

    // Vignette合成専用のPixel Shaderを読み込む。
    if (FAILED(CPUResourceLoader::LoadPixelShader(
        device_,
        "Data/Shader/VignettePS.cso",
        pixelShader.GetAddressOf())))
    {
        return false;
    }

    // Vignette設定は演出で変わるため、CPUから毎Frame書けるBufferにする。
    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbVignette);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void VignetteSystem::Shutdown()
{
    BS_PROFILE_SCOPE("VignetteSystem::Shutdown");

    constantBuffer.Reset();
    pixelShader.Reset();
    fullScreenVs.Reset();
    vignetteTexture.Reset();
    loadedTexturePath.clear();
}

void VignetteSystem::Execute(
    const RenderContext& rc_,
    ID3D11ShaderResourceView* sceneSRV_,
    ID3D11RenderTargetView* outputRTV_,
    const Parameters& parameters_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    const RenderState* rs_ = rc_.renderState;

    // 入出力やRenderStateが無い場合は、現在のFrameではPostEffectをSkipする。
    if (dc_ == nullptr || rs_ == nullptr || sceneSRV_ == nullptr || outputRTV_ == nullptr)
    {
        return;
    }

    if (loadedTexturePath != parameters_.texturePath)
    {
        vignetteTexture.Reset();
        loadedTexturePath = parameters_.texturePath;
        if (!loadedTexturePath.empty())
        {
            const auto path_ = WindowsDialogHelper::Utf8ToPath(loadedTexturePath);
            CPUResourceLoader::LoadTextureFromFile(rc_.device, path_.c_str(),
                vignetteTexture.GetAddressOf(), nullptr, TextureType::Data);
        }
    }

    UpdateConstantBuffer(rc_, parameters_);

    // FullScreen PassなのでDepthを無効化し、面のCullも行わない。
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

    // Scene TextureをSampleして、Vignetteを合成した色をoutputへ書き込む。
    ID3D11SamplerState* samplers_[] = {
        rs_->GetSamplerState(SamplerState::LinearClamp),
        rs_->GetSamplerState(SamplerState::LinearWrap) };
    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc_->PSSetSamplers(0, 2, samplers_);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    ID3D11ShaderResourceView* resources_[] = { sceneSRV_, vignetteTexture.Get() };
    dc_->PSSetShaderResources(0, 2, resources_);
    DrawFullScreenQuad(dc_);

    // 次のPassで同じTextureをRTVとして使えるよう、SRV参照を解除しておく。
    ID3D11ShaderResourceView* nullSRVs_[2]{};
    dc_->PSSetShaderResources(0, 2, nullSRVs_);
    dc_->OMSetDepthStencilState(rs_->GetDepthStencilState(DepthState::TestOnly), 0);
}

void VignetteSystem::UpdateConstantBuffer(const RenderContext& rc_, const Parameters& parameters_)
{
    // Shaderが必要な値をまとめ、未使用領域は0初期化で安定させる。
    CbVignette cb_{};
    cb_.screenSize = { static_cast<float>(width), static_cast<float>(height) };
    cb_.center = parameters_.center;
    cb_.color = parameters_.color;
    cb_.enabled = parameters_.enabled ? 1 : 0;
    cb_.intensity = parameters_.intensity;
    cb_.smoothness = parameters_.smoothness;
    cb_.rounded = parameters_.rounded;
    cb_.roundness = parameters_.roundness;
    cb_.opacity = parameters_.opacity;
    cb_.damageOpacity = parameters_.damageOpacity;
    cb_.textureParams = { parameters_.textureTiling.x, parameters_.textureTiling.y,
        parameters_.textureStrength, vignetteTexture.Get() ? 1.0f : 0.0f };
    cb_.hatchParams = { parameters_.hatchStrength, parameters_.hatchScale, 0.0f, 0.0f };

    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (SUCCEEDED(rc_.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        // Dynamic Bufferへ現在FrameのVignette設定を転送する。
        std::memcpy(mapped_.pData, &cb_, sizeof(cb_));
        rc_.deviceContext->Unmap(constantBuffer.Get(), 0);
    }
}

void VignetteSystem::DrawFullScreenQuad(ID3D11DeviceContext* dc_)
{
    // FullScreenVSがSV_VertexIDから三角形を作るため、頂点入力は空にする。
    dc_->IASetInputLayout(nullptr);
    dc_->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->Draw(3, 0);
}
