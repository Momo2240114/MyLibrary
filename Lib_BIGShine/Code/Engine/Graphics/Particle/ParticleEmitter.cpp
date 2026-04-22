#include "ParticleEmitter.h"

ParticleEmitter::ParticleEmitter(ID3D11Device* device, const ParticleSystemConstants& initialSettings, int maxParticles)
    : settings(initialSettings), maxParticles(maxParticles)
{
    // 初期設定に最大数を反映
    settings.maxParticleCount = static_cast<uint32_t>(maxParticles);
    CreateGPUResources(device);
}

void ParticleEmitter::CreateGPUResources(ID3D11Device* device)
{
    D3D11_BUFFER_DESC sbDesc = {};
    sbDesc.ByteWidth = sizeof(Particle) * maxParticles;
    sbDesc.Usage = D3D11_USAGE_DEFAULT;
    sbDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    sbDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    sbDesc.StructureByteStride = sizeof(Particle);

    for (int i = 0; i < 2; ++i) { // ★ ループで2セット作成
        device->CreateBuffer(&sbDesc, nullptr, particleBuffer[i].GetAddressOf());

        // UAV (Updateでの書き込み用: u0)
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = maxParticles;
        device->CreateUnorderedAccessView(particleBuffer[i].Get(), &uavDesc, bufferUAV[i].GetAddressOf());

        // SRV (Updateでの読み取り用: t0 / Renderでの頂点データ: t9)
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.NumElements = maxParticles;
        device->CreateShaderResourceView(particleBuffer[i].Get(), &srvDesc, bufferSRV[i].GetAddressOf());
    }

    // 定数バッファは1つでOK
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(ParticleSystemConstants);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbDesc, nullptr, constantBuffer.GetAddressOf());
}

void ParticleEmitter::Update(ID3D11DeviceContext* context, float et)
{
    // 1. 定数バッファの更新
    settings.time += et;
    settings.deltaTime = et;

    // ★放出の制御 (例: 毎秒60個出すなら、1フレームあたり 60 * dt 個)
    // ParticleSystemConstants に spawnCount などの項目がある前提
    // settings.spawnCount = static_cast<uint32_t>(60.0f * dt); 

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(context->Map(constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, &settings, sizeof(ParticleSystemConstants));
        context->Unmap(constantBuffer.Get(), 0);
    }

    // 2. ピンポンのインデックス計算
    // nextIndex が「今回書き込む先(New)」、currIndex が「前回の結果(Old)」
    uint32_t nextIndex = (currIndex + 1) % 2;

    // 3. リソースのバインド
    // 定数バッファは b9
    context->CSSetConstantBuffers(9, 1, constantBuffer.GetAddressOf());

    // t0: 前回の結果 (OldParticles)
    context->CSSetShaderResources(0, 1, bufferSRV[currIndex].GetAddressOf());

    // u0: 今回の書き込み先 (NewParticles)
    context->CSSetUnorderedAccessViews(0, 1, bufferUAV[nextIndex].GetAddressOf(), nullptr);

    // 4. 計算実行
    UINT groups = (maxParticles + 15) / 16;
    context->Dispatch(groups, 1, 1);

    // 5. 後片付け (SRV/UAVを解除しないと、次の描画でバインドエラーになる)
    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    context->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
    ID3D11ShaderResourceView* nullSRV[] = { nullptr };
    context->CSSetShaderResources(0, 1, nullSRV);

    // 6. インデックスを更新
    // これにより、次のフレームでは今回書き込んだ nextIndex が「Old」として読み込まれる
    currIndex = nextIndex;
}

void ParticleEmitter::PrepareRender(ID3D11DeviceContext* context)
{
    // b9: エミッター個別の設定値 (ParticleSystemConstants)
    ID3D11Buffer* cbs[] = { constantBuffer.Get() };
    context->VSSetConstantBuffers(9, 1, cbs);
    context->GSSetConstantBuffers(9, 1, cbs);
    context->PSSetConstantBuffers(9, 1, cbs);

    // t9: 構造化バッファ (最新のシミュレーション結果)
    // VS/GS両方で粒子座標を参照するために必要
    ID3D11ShaderResourceView* srv = bufferSRV[currIndex].Get();
    context->VSSetShaderResources(9, 1, &srv);
    context->GSSetShaderResources(9, 1, &srv);

    // t0: 粒子のテクスチャ
    if (textureSRV) {
        context->PSSetShaderResources(0, 1, textureSRV.GetAddressOf());
    }
}