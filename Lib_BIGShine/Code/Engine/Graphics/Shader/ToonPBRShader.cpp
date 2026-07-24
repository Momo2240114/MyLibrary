#include "PrecompiledHeader.h"

#include "Engine/Graphics/Shader/ToonPBRShader.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>

namespace
{
    void SortShadeSteps(DirectX::XMFLOAT4& shadeSteps_)
    {
        float steps_[] = { shadeSteps_.x, shadeSteps_.y, shadeSteps_.z, shadeSteps_.w };
        std::sort(steps_, steps_ + 4);
        shadeSteps_ = { steps_[0], steps_[1], steps_[2], steps_[3] };
    }
}

ToonPBRShader::ToonPBRShader(ID3D11Device* device)
{
    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/ToonPBRStaticVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        staticVertexShader.GetAddressOf());

    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/ToonPBRSkinnedVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        nullptr,
        skinnedVertexShader.GetAddressOf());

    CPUResourceLoader::LoadPixelShader(
        device,
        "Shader/ToonPBRPS.cso",
        pixelShader.GetAddressOf());

    CPUResourceLoader::CreateConstantBuffer(
        device,
        sizeof(CbMesh),
        meshConstantBuffer.GetAddressOf());

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&samplerDesc, linearSampler.GetAddressOf());
}

void ToonPBRShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(skinnedVertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);

    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
    dc->PSSetSamplers(0, 1, linearSampler.GetAddressOf());

    auto& gdm = GraphicsDeviceManager::Instance();
    ID3D11ShaderResourceView* iblSRVs[] =
    {
        gdm.GetIBLDiffuseSRV(),
        gdm.GetIBLSpecularSRV(),
        gdm.GetIBLLutGGXSRV(),
    };
    dc->PSSetShaderResources(10, _countof(iblSRVs), iblSRVs);
}

void ToonPBRShader::BindVertexShader(const RenderContext& rc_, bool isSkinnedMesh_)
{
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    dc_->VSSetShader(
        isSkinnedMesh_ ? skinnedVertexShader.Get() : staticVertexShader.Get(),
        nullptr,
        0);
}

void ToonPBRShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    CbMesh cb{};
    cb.baseColor = mesh.material->baseColor;
    cb.emissiveColor = mesh.material->emissiveColor;
    cb.metalness = mesh.material->metalness;
    cb.roughness = mesh.material->roughness;
    cb.shadeSteps = shadeSteps;
    cb.shadeFeathers = shadeFeathers;
    cb.occlusionStrength = mesh.material->occlusionStrength;
    cb.toonPbrBlend = mesh.material->toonPbrBlend;
    cb.normalScale = mesh.material->normalScale;

    dc->UpdateSubresource(meshConstantBuffer.Get(), 0, 0, &cb, 0, 0);

    ID3D11ShaderResourceView* srvs[] =
    {
        mesh.material->baseMap.Get(),
        mesh.material->normalMap.Get(),
        mesh.material->emissiveMap.Get(),
        mesh.material->occlusionMap.Get(),
        mesh.material->metalnessRoughnessMap.Get(),
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

void ToonPBRShader::Gui()
{
    if (ImGui::SliderFloat4("ToonPBRPS_Shade Thresholds", &shadeSteps.x, 0.001f, 1.0f))
    {
        SortShadeSteps(shadeSteps);
    }
    if (ImGui::SliderFloat4("ToonPBRPS_Shade Feather", &shadeFeathers.x, 0.001f, 1.0f))
    {
    }
}

void ToonPBRShader::SetShadeSettings(const DirectX::XMFLOAT4& shadeSteps_, const DirectX::XMFLOAT4& shadeFeather_)
{
    shadeSteps = shadeSteps_;
    SortShadeSteps(shadeSteps);
    shadeFeathers = shadeFeather_;
}

void ToonPBRShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);

    ID3D11ShaderResourceView* nullSRVs[5] = { nullptr };
    dc->PSSetShaderResources(0, 5, nullSRVs);

    ID3D11ShaderResourceView* nullIBL[3] = { nullptr };
    dc->PSSetShaderResources(10, 3, nullIBL);
}
