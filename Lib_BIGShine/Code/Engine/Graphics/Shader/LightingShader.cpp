#include "LightingShader.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h" // 君が使っているローダーに合わせてくれ

LightingShader::LightingShader(ID3D11Device* device)
{
    // 1. 頂点シェーダーの読み込み (全画面四角形用)
    // ※ 頂点レイアウト(InputLayout)は不要（VS内部でIDから頂点を生成するため）
    CPUResourceLoader::LoadVertexShader(
        device,
        "Shader/FullScreenVS.cso",
        nullptr, 0, // InputLayoutはnullptrでよい
        nullptr,
        vertexShader.GetAddressOf());

    // 2. ピクセルシェーダーの読み込み (計算ロジックが入ったもの)
    CPUResourceLoader::LoadPixelShader(
        device,
        "Shader/LightingPS.cso",
        pixelShader.GetAddressOf());


    // 4. サンプラーステートの作成
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 画面端はCLAMPが良い
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&samplerDesc, samplerState.GetAddressOf());
}

void LightingShader::Execute(const RenderContext& rc,
    ID3D11ShaderResourceView* color, 
    ID3D11ShaderResourceView* normal, 
    ID3D11ShaderResourceView* material,
    ID3D11ShaderResourceView* pointLightSRV,  // 追加
    ID3D11ShaderResourceView* spotLightSRV,
    ID3D11Buffer* sceneConstantBuffer) // b7 を受け取るために追加// 追加
{
    auto dc = rc.deviceContext;


    _ASSERT_EXPR(color != nullptr, L"Color SRV is null!");
    _ASSERT_EXPR(normal != nullptr, L"Normal SRV is null!");
    _ASSERT_EXPR(material != nullptr, L"Material SRV is null!");
    _ASSERT_EXPR(sceneConstantBuffer != nullptr, L"Constant Buffer is null!");

    // 1. シェーダーとサンプラーセット
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->PSSetSamplers(0, 1, samplerState.GetAddressOf());

    // 2. G-Bufferの3枚をバインド (t0, t1, t2)
    ID3D11ShaderResourceView* srvs[] = { color, normal, material };
    dc->PSSetShaderResources(0, 3, srvs);


    // 3. ライティングに必要なライト情報を定数バッファへ更新
    // rc.lights を使って lightConstantBuffer を更新せよ
    dc->PSSetConstantBuffers(7, 1, &sceneConstantBuffer); // b7 (CbScene)

    ID3D11ShaderResourceView* lights[] = { pointLightSRV, spotLightSRV };
    dc->PSSetShaderResources(5, 2, lights); // t5, t6 (StructuredBuffers)


    // 4. 四角形描画 (インデックスバッファなしで描画する手法)
    dc->IASetInputLayout(nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    dc->Draw(3, 0);

    // 5. 解除 (バインドしたものをすべてnullptrにする)
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    dc->PSSetShaderResources(0, 7, nullSRVs);
}