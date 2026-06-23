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
    const DirectX::XMFLOAT4& GetShadeSteps() const { return shadeSteps; }
    DirectX::XMFLOAT4 GetShadeFeather() const { return shadeFeathers; }
    void SetShadeFeather(DirectX::XMFLOAT4 shadeFeather_) { shadeFeathers = shadeFeather_; }
    void SetShadeSettings(const DirectX::XMFLOAT4& shadeSteps_, const DirectX::XMFLOAT4& shadeFeather_);

private:
    struct CbMesh
    {
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT3 emissiveColor;
        float metalness;
        float roughness;
        float occlusionStrength;
        float toonPbrBlend;
        float normalScale;
        DirectX::XMFLOAT4  shadeFeathers;
        DirectX::XMFLOAT4 shadeSteps;

    };

    DirectX::XMFLOAT4 shadeSteps{ 0.45f, 0.45f, 0.45f, 0.45f };
    DirectX::XMFLOAT4 shadeFeathers = { 0.1f,0.1f,0.1f,0.1f };

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> meshConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
};
