#include "PrecompiledHeader.h"

#include "Engine/Graphics/Shader/ToonPBRShader.h"

#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

#include <algorithm>

namespace
{
    void SortShadeSteps(DirectX::XMFLOAT4& shadeSteps_)
    {
        // しきい値の順序が入れ替わるとShader側の段階計算が崩れるため昇順へ揃える。
        float steps_[] = { shadeSteps_.x, shadeSteps_.y, shadeSteps_.z, shadeSteps_.w };
        std::sort(steps_, steps_ + 4);
        shadeSteps_ = { steps_[0], steps_[1], steps_[2], steps_[3] };
    }
}

ToonPBRShader::ToonPBRShader(ID3D11Device* device)
{
    // Static Mesh用Vertex Shader。InputLayoutはStatic/Skinnedで同じ頂点定義を使う。
    CPUResourceLoader::LoadVertexShader(
        device,
        "Data/Shader/ToonPBRStaticVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        staticVertexShader.GetAddressOf());

    // Skinned Mesh用Vertex Shader。Bone変形はShader側で行う。
    CPUResourceLoader::LoadVertexShader(
        device,
        "Data/Shader/ToonPBRSkinnedVS.cso",
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        nullptr,
        skinnedVertexShader.GetAddressOf());

    // PBR TextureとToon段階設定を組み合わせて最終色を作るPixel Shader。
    CPUResourceLoader::LoadPixelShader(
        device,
        "Data/Shader/ToonPBRPS.cso",
        pixelShader.GetAddressOf());

    // MeshごとのMaterial値を送るConstantBuffer。
    CPUResourceLoader::CreateConstantBuffer(
        device,
        sizeof(CbMesh),
        meshConstantBuffer.GetAddressOf());

    // glTFのTextureを繰り返しSampleできる標準Linear Wrap Sampler。
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

    // Begin時は基本PipelineをBindし、Meshごとの差分はUpdateで上書きする。
    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(skinnedVertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);

    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
    dc->PSSetSamplers(0, 1, linearSampler.GetAddressOf());

    auto& gdm = GraphicsDeviceManager::Instance();
    ID3D11ShaderResourceView* iblSRVs[] =
    {
        // PBRの環境光計算用IBL Texture群。
        gdm.GetIBLDiffuseSRV(),
        gdm.GetIBLSpecularSRV(),
        gdm.GetIBLLutGGXSRV(),
    };
    dc->PSSetShaderResources(10, _countof(iblSRVs), iblSRVs);
}

void ToonPBRShader::BindVertexShader(const RenderContext& rc_, bool isSkinnedMesh_)
{
    // DrawするMeshのBone有無に合わせてVSだけ切り替える。
    ID3D11DeviceContext* dc_ = rc_.deviceContext;
    dc_->VSSetShader(
        isSkinnedMesh_ ? skinnedVertexShader.Get() : staticVertexShader.Get(),
        nullptr,
        0);
}

void ToonPBRShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    // MaterialからPBR値とToon設定を詰め、Pixel Shaderへ送る。
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
        // glTF Materialで使うTexture群。欠けているSlotはnullptrのままShader側/Resource側で扱う。
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
    // Toonの明暗しきい値はEditorで動かし、常に小さい順へ整列する。
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
    // 外部から設定された場合も、Shaderへ渡す前にしきい値順を保証する。
    shadeSteps = shadeSteps_;
    SortShadeSteps(shadeSteps);
    shadeFeathers = shadeFeather_;
}

void ToonPBRShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    // 後続Shaderへ状態が漏れないよう、Shader/InputLayout/SRVを解除する。
    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);

    ID3D11ShaderResourceView* nullSRVs[5] = { nullptr };
    dc->PSSetShaderResources(0, 5, nullSRVs);

    ID3D11ShaderResourceView* nullIBL[3] = { nullptr };
    dc->PSSetShaderResources(10, 3, nullIBL);
}
