#include "PrecompiledHeader.h"

#include "FogSystem.h"

#include <cstring>

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Platform/WindowsDialogHelper.h"

bool FogSystem::Initialize(ID3D11Device* device_, int width_, int height_)
{
    if (!device_ || width_ <= 0 || height_ <= 0)
    {
        return false;
    }
    width = width_;
    height = height_;
    if (FAILED(CPUResourceLoader::LoadVertexShader(device_, "Data/Shader/FullScreenVS.cso",
        nullptr, 0, nullptr, fullScreenVs.GetAddressOf())))
    {
        return false;
    }
    if (FAILED(CPUResourceLoader::LoadPixelShader(device_, "Data/Shader/FogPS.cso",
        pixelShader.GetAddressOf())))
    {
        return false;
    }

    D3D11_BUFFER_DESC desc_{};
    desc_.ByteWidth = sizeof(CbFog);
    desc_.Usage = D3D11_USAGE_DYNAMIC;
    desc_.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc_.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device_->CreateBuffer(&desc_, nullptr, constantBuffer.GetAddressOf()));
}

void FogSystem::Shutdown()
{
    fullScreenVs.Reset();
    pixelShader.Reset();
    constantBuffer.Reset();
    fogTexture.Reset();
    loadedTexturePath.clear();
}

void FogSystem::Execute(const RenderContext& rc_, ID3D11ShaderResourceView* sceneSRV_,
    ID3D11ShaderResourceView* depthSRV_, ID3D11RenderTargetView* outputRTV_, const Parameters& parameters_)
{
    if (!rc_.device || !rc_.deviceContext || !rc_.renderState || !rc_.camera ||
        !sceneSRV_ || !depthSRV_ || !outputRTV_ || !pixelShader || !constantBuffer)
    {
        return;
    }

    if (loadedTexturePath != parameters_.texturePath)
    {
        fogTexture.Reset();
        loadedTexturePath = parameters_.texturePath;
        if (!loadedTexturePath.empty())
        {
            const auto path_ = WindowsDialogHelper::Utf8ToPath(loadedTexturePath);
            CPUResourceLoader::LoadTextureFromFile(rc_.device, path_.c_str(),
                fogTexture.GetAddressOf(), nullptr, TextureType::Data);
        }
    }

    const CbFog constants_ {
        { parameters_.color.x, parameters_.color.y, parameters_.color.z, 1.0f },
        { rc_.camera->GetNear(), rc_.camera->GetFar(), parameters_.startDistance, parameters_.endDistance },
        { parameters_.density, parameters_.textureStrength, parameters_.hatchStrength, parameters_.hatchScale },
        { parameters_.textureTiling.x, parameters_.textureTiling.y, fogTexture.Get() ? 1.0f : 0.0f, 0.0f }
    };
    D3D11_MAPPED_SUBRESOURCE mapped_{};
    if (FAILED(rc_.deviceContext->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_)))
    {
        return;
    }
    std::memcpy(mapped_.pData, &constants_, sizeof(constants_));
    rc_.deviceContext->Unmap(constantBuffer.Get(), 0);

    D3D11_VIEWPORT viewport_{};
    viewport_.Width = static_cast<float>(width);
    viewport_.Height = static_cast<float>(height);
    viewport_.MaxDepth = 1.0f;
    auto* dc_ = rc_.deviceContext;
    dc_->RSSetViewports(1, &viewport_);
    dc_->RSSetState(rc_.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
    dc_->OMSetDepthStencilState(rc_.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
    dc_->OMSetRenderTargets(1, &outputRTV_, nullptr);
    ID3D11ShaderResourceView* resources_[] = { sceneSRV_, depthSRV_, fogTexture.Get() };
    ID3D11SamplerState* samplers_[] = {
        rc_.renderState->GetSamplerState(SamplerState::LinearClamp),
        rc_.renderState->GetSamplerState(SamplerState::LinearWrap) };
    dc_->IASetInputLayout(nullptr);
    ID3D11Buffer* emptyVertexBuffer_ = nullptr;
    UINT zero_ = 0;
    dc_->IASetVertexBuffers(0, 1, &emptyVertexBuffer_, &zero_, &zero_);
    dc_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dc_->VSSetShader(fullScreenVs.Get(), nullptr, 0);
    dc_->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc_->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
    dc_->PSSetSamplers(0, 2, samplers_);
    dc_->PSSetShaderResources(0, 3, resources_);
    dc_->Draw(3, 0);
    ID3D11ShaderResourceView* emptyResources_[3]{};
    dc_->PSSetShaderResources(0, 3, emptyResources_);
    dc_->OMSetDepthStencilState(rc_.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
}
