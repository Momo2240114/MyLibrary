#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <DirectXMath.h>

class CameraComponent;

class CascadeShadowMap
{
public:
    static constexpr int CascadeCount = 4;
    static constexpr UINT ShadowmapSize = 2048;

    bool Initialize(ID3D11Device* device);
    void UpdateMatrices(const CameraComponent& camera, const DirectX::XMFLOAT3& lightDirection);

    ID3D11DepthStencilView* GetDSV(int index) const { return shadowDSVs[index].Get(); }
    ID3D11ShaderResourceView* GetSRV(int index) const { return shadowSRVs[index].Get(); }
    const DirectX::XMFLOAT4X4& GetLightViewProjection(int index) const { return lightViewProjections[index]; }
    DirectX::XMFLOAT4 GetCascadeSplits() const { return cascadeSplits; }

    void SetViewport(ID3D11DeviceContext* dc) const;

    //ÉQÉbÉ^Å[
    bool IsUseCascadeShadow() const { return useCascadeShadow; }
    void SetUseCascadeShadow(bool enable) { useCascadeShadow = enable; }

    bool IsDisplayCascadeArea() const { return displayCascadeArea; }
    void SetDisplayCascadeArea(bool enable) { displayCascadeArea = enable; }

    float* GetCascadeSplitDistances() { return cascadeSplitDistances; }
    const float* GetCascadeSplitDistances() const { return cascadeSplitDistances; }
    void SetCascadeSplitDistance(int index_, float distance_);

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTextures[CascadeCount];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowDSVs[CascadeCount];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowSRVs[CascadeCount];
    D3D11_VIEWPORT viewport{};

    DirectX::XMFLOAT4X4 lightViewProjections[CascadeCount]{};
    DirectX::XMFLOAT4 cascadeSplits{ 25.0f, 100.0f, 250.0f, 500.0f };

    bool useCascadeShadow = true;
    bool displayCascadeArea = false;
    
    float cascadeSplitDistances[4] =
    {
        25.0f,
        100.0f,
        250.0f,
        500.0f,
    };
};