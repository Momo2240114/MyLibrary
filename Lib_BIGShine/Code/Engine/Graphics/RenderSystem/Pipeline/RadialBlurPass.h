// RadialBlurPass.h
#pragma once
#include "IRenderPass.h"
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>

class RadialBlurPass : public IRenderPass 
{
public:
    struct Constants {
        float blur_radius = 50.0f;
        int blur_sampling_count = 10;
        DirectX::XMFLOAT2 blur_center = { 0.5f, 0.5f };
        float blur_mask_radius = 0.0f;
        DirectX::XMFLOAT3 blur_dummy;
    };

    void Initialize(ID3D11Device* device, float width, float height)  override;
    void Execute(const RenderContext& rc, ID3D11ShaderResourceView* inputSRV, ID3D11RenderTargetView* outputRTV) override;

    Constants& GetConstants() { return m_constants; }

private:
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_ps;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cb;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler;
    Constants m_constants;
};