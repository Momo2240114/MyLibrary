#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <DirectXMath.h>

class CascadeShadowMap
{
public:
    static constexpr int CascadeCount = 4;
    static constexpr UINT ShadowmapSize = 2048;

    bool Initialize(ID3D11Device* device);

    // 特定のカスケード用リソースを取得
    ID3D11DepthStencilView* GetDSV(int index) const { return shadowDSVs[index].Get(); }
    ID3D11ShaderResourceView* GetSRV(int index) const { return shadowSRVs[index].Get(); }

    // 定数バッファ等の更新用
    void SetViewport(ID3D11DeviceContext* dc) const;

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shadowTextures[CascadeCount];
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> shadowDSVs[CascadeCount];
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shadowSRVs[CascadeCount];
    D3D11_VIEWPORT viewport;
};