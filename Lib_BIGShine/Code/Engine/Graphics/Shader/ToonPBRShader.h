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
    float GetShadeStep() const { return shadeStep; }
    float GetShadeFeather() const { return shadeFeather; }
    void SetShadeStep(float shadeStep_) { shadeStep = shadeStep_; }
    void SetShadeFeather(float shadeFeather_) { shadeFeather = shadeFeather_; }
    void SetShadeSettings(float shadeStep_, float shadeFeather_) { shadeStep = shadeStep_; shadeFeather = shadeFeather_; }

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
        float toonPbrBlend;
        float padding[3];
    };

    float shadeStep = 0.45f;
    float shadeFeather = 0.08f;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> meshConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
};
