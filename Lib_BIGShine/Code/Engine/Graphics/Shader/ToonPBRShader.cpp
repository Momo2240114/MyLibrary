#include "Engine/Graphics/Shader/ToonPBRShader.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

ToonPBRShader::ToonPBRShader(ID3D11Device* device)
{
    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/PBRVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

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
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
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

void ToonPBRShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    CbMesh cb{};
    cb.baseColor = mesh.material->baseColor;
    cb.emissiveColor = mesh.material->emissiveColor;
    cb.metalness = mesh.material->metalness;
    cb.roughness = mesh.material->roughness;
    cb.shadeStep = shadeStep;
    cb.shadeFeather = shadeFeather;
    cb.occlusionStrength = mesh.material->occlusionStrength;

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
    ImGui::SliderFloat("Shade Step", &shadeStep, 0.0f, 1.0f);
    ImGui::SliderFloat("Shade Feather", &shadeFeather, 0.001f, 0.6f);
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
