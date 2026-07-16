#include "PrecompiledHeader.h"

#include "ToonShader.h"
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

ToonShader::ToonShader(ID3D11Device* device)
{
    CPUResourceLoader::LoadVertexShader(device, "Shader/ToonVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(), vertexShader.GetAddressOf());

    CPUResourceLoader::LoadPixelShader(device, "Shader/ToonPS.cso", pixelShader.GetAddressOf());

    CPUResourceLoader::CreateConstantBuffer(device, sizeof(CbMesh), meshConstantBuffer.GetAddressOf());

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    device->CreateSamplerState(&sd, samplerState.GetAddressOf());
}
void ToonShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);

    // 定数バッファ (b0) とサンプラー (s0) の設定
    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
    dc->PSSetSamplers(0, 1, samplerState.GetAddressOf());

    auto& gdm = GraphicsDeviceManager::Instance();
    ID3D11ShaderResourceView* iblSRVs[] =
    {
        gdm.GetIBLDiffuseSRV(),
        gdm.GetIBLSpecularSRV(),
        gdm.GetIBLLutGGXSRV(),
    };
    dc->PSSetShaderResources(10, _countof(iblSRVs), iblSRVs);
}

void ToonShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    CbMesh cb{};
    cb.baseColor = mesh.material->baseColor;
    cb.emissiveColor = mesh.material->emissiveColor;
    cb.metalness = mesh.material->metalness; 
    cb.roughness = mesh.material->roughness; 
    cb.shadeSteps = shadeSteps;
    cb.shadeFeather = this->shadeFeathers; // クラス保持の値を使用
    cb.occlusionStrength = mesh.material->occlusionStrength;

    dc->UpdateSubresource(meshConstantBuffer.Get(), 0, 0, &cb, 0, 0);

    // PBRと同じスロットにテクスチャを流し込む
    ID3D11ShaderResourceView* srvs[] = {
        mesh.material->baseMap.Get(),
        mesh.material->normalMap.Get(),
        mesh.material->emissiveMap.Get(),
        mesh.material->occlusionMap.Get(),
        mesh.material->metalnessRoughnessMap.Get()
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

void ToonShader::Gui()
{
    if (ImGui::SliderFloat4("Toon_Shade Thresholds", &shadeSteps.x, 0.001f, 1.0f))
    {
        SortShadeSteps(shadeSteps);
    }
    ImGui::SliderFloat4("Toon_Shade Feather", &shadeFeathers.x, 0.001f, 1.0f);
}

void ToonShader::SetShadeSettings(const DirectX::XMFLOAT4& shadeSteps_, DirectX::XMFLOAT4 shadeFeather_)
{
    shadeSteps = shadeSteps_;
    SortShadeSteps(shadeSteps);
    shadeFeathers = shadeFeather_;
}

void ToonShader::End(const RenderContext& rc)
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