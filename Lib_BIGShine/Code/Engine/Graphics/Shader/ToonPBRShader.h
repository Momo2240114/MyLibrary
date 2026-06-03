#pragma once

#include "ModelShader.h"

#include <DirectXMath.h>

class ToonPBRShader : public ModelShader
{
public:
    ToonPBRShader(ID3D11Device* device);
    ~ToonPBRShader() override = default;

    const std::string& GetName() const override
    {
        static const std::string name = "ToonPBR";
        return name;
    }

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;
    void End(const RenderContext& rc) override;
    void Gui() override;

private:
    struct CbMesh
    {
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT3 emissiveColor;
        float metalness;
        float roughness;
        float shadeStep;
        float shadeFeather;
        float occlusionStrength;
    };

    float shadeStep = 0.45f;
    float shadeFeather = 0.08f;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> meshConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
};
