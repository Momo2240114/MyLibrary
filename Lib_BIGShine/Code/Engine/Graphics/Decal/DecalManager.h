#pragma once
#include <vector>
#include <cstdint>
#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>

struct DecalData {
    // デカール投影ボックスのワールド行列。
    DirectX::XMFLOAT4X4 worldMatrix;

    // ベース色と透明度。
    DirectX::XMFLOAT4 color;

    // xyz:発光色、w:発光強度。
    DirectX::XMFLOAT4 emissiveColorAndIntensity{ 1.0f, 1.0f, 1.0f, 0.0f };

    // xy:UVスケール、zw:UVオフセット。
    DirectX::XMFLOAT4 uvScaleOffset{ 1.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 noiseUvScaleOffset{ 1.0f, 1.0f, 0.0f, 0.0f };

    // x:しきい値、y:境界幅、z:境界強度など、Dissolve用パラメータ。
    DirectX::XMFLOAT4 dissolveParams{ 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 dissolveEdgeColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    uint32_t blendMode = 0u;
    uint32_t useDissolve = 0u;
    bool groundSurface = false;

    // DecalShaderへ渡す各Texture。
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> maskTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> noiseTexture;
};

class DecalManager {
public:
    // フレーム開始時に、前フレームのデカール描画要求を消す。
    void Clear() { decals.clear(); }

    // DecalComponentから送られてきた描画要求をキューへ積む。
    void AddDecal(const DecalData& data) {
        decals.push_back(data);
    }

    // RenderSystemがこのフレーム描画するデカール一覧を読む。
    const std::vector<DecalData>& GetDecals() const { return decals; }

private:
    // 1フレーム分のデカール描画キュー。
    std::vector<DecalData> decals;
};
