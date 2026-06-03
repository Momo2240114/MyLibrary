#include "ToonShader.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

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

}

void ToonShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    CbMesh cb{};
    cb.baseColor = mesh.material->baseColor;
    cb.emissiveColor = mesh.material->emissiveColor;
    cb.metalness = mesh.material->metalness; 
    cb.roughness = mesh.material->roughness; 
    cb.shadeStep = this->shadeStep;      // クラス保持の値を使用
    cb.shadeFeather = this->shadeFeather; // クラス保持の値を使用
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
    // マテリアル単位の設定を表示
    ImGui::SliderFloat("Shade Step", &shadeStep, 0.0f, 1.0f);
    ImGui::SliderFloat("Shade Feather", &shadeFeather, 0.001f, 0.6f);
}

void ToonShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    // 自分が使ったテクスチャ（t0）だけをクリアする
    ID3D11ShaderResourceView* nullSRV = nullptr;
    dc->PSSetShaderResources(0, 1, &nullSRV);
}