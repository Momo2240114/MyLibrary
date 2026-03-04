#pragma once

#include <d3d11.h>
#include <wrl.h>
#include "ParticleRessiource.h" // 構造体定義

class ParticleEmitter {
public:
    // コンストラクタ
    ParticleEmitter(ID3D11Device* device, const ParticleSystemConstants& initialSettings, int maxParticles = 1000);

    // Compute Shader による物理更新
    void Update(ID3D11DeviceContext* context, float dt);

    // レンダラーが描画する直前のリソースバインド
    void PrepareRender(ID3D11DeviceContext* context);

    // 設定とテクスチャの反映
    void ApplySettings(const ParticleSystemConstants& nextSettings) { settings = nextSettings; }
    ParticleSystemConstants& GetSettings() { return settings; }
    void SetTexture(Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv) { textureSRV = srv; }
    ID3D11ShaderResourceView* GetSRV() const { return bufferSRV[currIndex].Get(); }
    ID3D11ShaderResourceView* GetTextureSRV() const { return textureSRV.Get(); }
    ID3D11Buffer* GetConstantBuffer() const { return constantBuffer.Get(); }
private:
    void CreateGPUResources(ID3D11Device* device);

    ParticleSystemConstants settings;
    int maxParticles;
    uint32_t currIndex = 0; // ★ 0か1を指す現在のインデックス

    // GPU リソース（配列にする）
    Microsoft::WRL::ComPtr<ID3D11Buffer> particleBuffer[2];        // ★ 2つ用意
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> bufferSRV[2]; // ★ 2つ用意
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> bufferUAV[2]; // ★ 2つ用意
    Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;         // b9用
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureSRV; // t0用
};