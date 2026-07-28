#pragma once
#include <vector>
#include <cstdint>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

struct DecalData {
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT4 emissiveColorAndIntensity{ 1.0f, 1.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT4 uvScaleOffset{ 1.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 noiseUvScaleOffset{ 1.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 dissolveParams{ 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 dissolveEdgeColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t blendMode = 0u;
    uint32_t useDissolve = 0u;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
};

class DecalManager {
public:
    void Clear() { decals.clear(); }

    void AddDecal(const DecalData& data) {
        decals.push_back(data);
    }

    const std::vector<DecalData>& GetDecals() const { return decals; }

private:
    std::vector<DecalData> decals;
};