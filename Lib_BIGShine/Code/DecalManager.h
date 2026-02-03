#pragma once
#include <vector>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

struct DecalData {
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4 color;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    // •K—v‚É‰‚¶‚Ä—Dæ“x‚âõ–½‚ğ’Ç‰Á
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