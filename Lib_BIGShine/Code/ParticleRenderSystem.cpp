#include "ParticleRenderSystem.h"
#include "CPUResourceLoader.h"
#include "GraphicsDeviceManager.h"
#include <DirectXMath.h>

void ParticleRenderSystem::Initialize(ID3D11Device* device)
{
    // 各種シェーダーを CSO からロード
    CPUResourceLoader::LoadVertexShader(device, "Shader/ParticleDraw_VS.cso", nullptr, 0, nullptr, vertexShader.GetAddressOf());
    CPUResourceLoader::LoadGeometryShader(device, "Shader/ParticleDraw_GS.cso", geometryShader.GetAddressOf());
    CPUResourceLoader::LoadPixelShader(device, "Shader/ParticleDraw_PS.cso", pixelShader.GetAddressOf());
    CPUResourceLoader::LoadComputeShader(device, "Shader/ParticleUpdate_CS.cso", computeShader.GetAddressOf());

    // パーティクル用共通定数バッファ (シーンスロット用)
    // b7用のバッファを、正しい構造体サイズ (CbScene) で作成する
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CbScene);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = 0;
    device->CreateBuffer(&cbDesc, nullptr, sceneConstantBuffer.GetAddressOf());

    // サンプラーステート (線形補間)
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    device->CreateSamplerState(&samplerDesc, linearSampler.GetAddressOf());
}

void ParticleRenderSystem::PushSubmit(ParticleComponent* component)
{
    if (!component || !component->IsEnabled()) return;
    drawInfos.push_back({ component });
}

void ParticleRenderSystem::RenderAll(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    if (drawInfos.empty()) return;

    // --- 1. シーン共通定数バッファ (b7) の更新とバインド ---
    {
        CbScene cbScene{};
        DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
        DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
        DirectX::XMStoreFloat4x4(&cbScene.viewProjection, V * P);

        const DirectX::XMFLOAT3& eye = rc.camera->GetEye();
        cbScene.cameraPosition = { eye.x, eye.y, eye.z, 0.0f };

        // バッファサイズが sizeof(CbScene) で作られていることが前提
        dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);

        // GSに行列を届けないとビルボード計算が0になる
        ID3D11Buffer* b7 = sceneConstantBuffer.Get();
        dc->VSSetConstantBuffers(7, 1, &b7);
        dc->GSSetConstantBuffers(7, 1, &b7);
        dc->PSSetConstantBuffers(7, 1, &b7);
    }

    // --- 2. 共通ステートの設定 ---
    dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
    dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

    float blendFactor[4] = { 0, 0, 0, 0 };
    dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xffffffff);



    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->GSSetShader(geometryShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->IASetInputLayout(nullptr);
    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    // テクスチャが補間されるよう LinearWrap や LinearClamp を選択
    ID3D11SamplerState* sampler = rc.renderState->GetSamplerState(SamplerState::LinearWrap);
    dc->PSSetSamplers(0, 1, &sampler);

    // --- 3. 個別エミッターの描画ループ ---
    for (auto& info : drawInfos)
    {
        for (auto& track : info.component->GetTracks())
        {
            if (!track.isStarted || !track.emitter) continue;

            // 2. エミッター個別のリソースバインド
            // SRV(t9) に現在のパーティクルバッファをセット
            ID3D11ShaderResourceView* srv = track.emitter->GetSRV();
            dc->GSSetShaderResources(9, 1, &srv);

            // 個別定数(b9)
            auto cb = track.emitter->GetConstantBuffer(); // getterが必要
            dc->VSSetConstantBuffers(9, 1, &cb);
            dc->GSSetConstantBuffers(9, 1, &cb);
            dc->PSSetConstantBuffers(9, 1, &cb);

            // テクスチャ(t0)
            ID3D11ShaderResourceView* tex = track.emitter->GetTextureSRV();
            dc->PSSetShaderResources(0, 1, &tex);

            // 3. 描画実行
            dc->IASetInputLayout(nullptr); // 頂点レイアウト不要
            dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

            // 描画実行
            dc->Draw(track.settings.maxParticleCount, 0);
        }
    }

    // --- 4. クリーンアップ (ブルームエラー防止の要) ---
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11Buffer* nullCB = nullptr;

    // 全ステージの特定スロットを掃除
    dc->VSSetShaderResources(9, 1, &nullSRV);
    dc->GSSetShaderResources(9, 1, &nullSRV);
    dc->PSSetShaderResources(0, 1, &nullSRV);

    dc->VSSetConstantBuffers(7, 1, &nullCB);
    dc->GSSetConstantBuffers(7, 1, &nullCB);
    dc->GSSetConstantBuffers(9, 1, &nullCB);

    dc->VSSetShader(nullptr, nullptr, 0);
    dc->GSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);



    drawInfos.clear();
}


void ParticleRenderSystem::UpdateAll(ID3D11DeviceContext* context, float et, const DirectX::XMMATRIX& vpMatrix, const std::vector<std::weak_ptr<ParticleComponent>>& components)
{
    drawInfos.clear();
    for (auto& wp : components)
    {
        auto comp = wp.lock();
        if (!comp || !comp->IsEnabled()) continue;

        // 描画リストに登録
        drawInfos.push_back({ comp.get() });
    }
}