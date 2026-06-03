#pragma once
#include "ModelShader.h"

class ShadowCasterShader : public ModelShader
{
public:
    ShadowCasterShader(ID3D11Device* device);
    const std::string& GetName() const override { static const std::string name = "ShadowCaster"; return name; }
    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

private:
    struct CbShadow
    {
        DirectX::XMFLOAT4X4 lightViewProjection;
    };
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
};