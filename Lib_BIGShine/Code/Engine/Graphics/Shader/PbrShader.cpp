#include "PbrShader.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"

#include "Engine/Graphics/Core/GraphicsDeviceManager.h"  // IBL SRV 取得用

PbrShader::PbrShader(ID3D11Device* device)
{
    // 頂点シェーダー（PBR用のVSがあればそちらを読み込む）
    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/PBRVS.cso", // または共通のVSがあればそれを指定
        GltfModelCommponennt::InputElementDescs.data(),
        static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    // ピクセルシェーダー
    CPUResourceLoader::LoadPixelShader(
        device,
        "Shader/PBRPS.cso", // 先ほど作成したPBR用HLSLをコンパイルしたもの
        pixelShader.GetAddressOf());

    // メッシュ用定数バッファ (PBR用の構造体サイズで作成)
    CPUResourceLoader::CreateConstantBuffer(
        device,
        sizeof(CbMesh),
        meshConstantBuffer.GetAddressOf());

    // サンプラーステートの作成 (線形補間)
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

void PbrShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);

    // 定数バッファ (b0) とサンプラー (s0) の設定
    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
    dc->PSSetSamplers(0, 1, linearSampler.GetAddressOf());

    // -------------------------------------------------------
    // IBL テクスチャを t10からt12 にバインド
    //   t10 : diffuse_iem    (拡散反射 IrradianceMap)
    //   t11 : specular_pmrem (鏡面反射 RadianceMap)
    //   t12 : lut_ggx        (GGX BRDF LUT)
    // PBRPS.hlsl の register 番号と必ず合わせること
    // -------------------------------------------------------
    auto& gdm = GraphicsDeviceManager::Instance();
    ID3D11ShaderResourceView* iblSRVs[] =
    {
        gdm.GetIBLDiffuseSRV(),   // t10
        gdm.GetIBLSpecularSRV(),  // t11
        gdm.GetIBLLutGGXSRV(),    // t12
    };
    dc->PSSetShaderResources(10, _countof(iblSRVs), iblSRVs);
}

void PbrShader::Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    // 定数バッファの更新
    CbMesh cb{};
    cb.baseColor = mesh.material->baseColor;
    cb.emissiveColor = mesh.material->emissiveColor;
    cb.metalness = mesh.material->metalness;
    cb.roughness = mesh.material->roughness;
    cb.occlusionStrength = mesh.material->occlusionStrength;

    dc->UpdateSubresource(meshConstantBuffer.Get(), 0, 0, &cb, 0, 0);

    // テクスチャを HLSL の register(t0)～(t4) に合わせてセット
    ID3D11ShaderResourceView* srvs[] =
    {
        mesh.material->baseMap.Get(),       // t0
        mesh.material->normalMap.Get(),     // t1
        mesh.material->emissiveMap.Get(),   // t2
        mesh.material->occlusionMap.Get(),  // t3
        mesh.material->metalnessRoughnessMap.Get()  // t4
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

void PbrShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);

    // マテリアルテクスチャ (t0からt4) のクリア
    ID3D11ShaderResourceView* nullSRVs[5] = { nullptr };
    dc->PSSetShaderResources(0, 5, nullSRVs);

    // IBL テクスチャ (t10からt12) のクリア
    ID3D11ShaderResourceView* nullIBL[3] = { nullptr };
    dc->PSSetShaderResources(10, 3, nullIBL);
}