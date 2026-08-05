#pragma once

#include "ModelShader.h"

#include <DirectXMath.h>

class ToonPBRShader : public ModelShader
{
public:
    // Toon寄りのPBR描画用Shader一式を読み込む。
    ToonPBRShader(ID3D11Device* device);
    ~ToonPBRShader() override = default;

    const std::string& GetName() const override
    {
        static const std::string name = "ToonPBR";
        return name;
    }

    // Shader/共通Sampler/IBL TextureをBindする。
    void Begin(const RenderContext& rc) override;
    // Mesh MaterialごとのConstantBufferとTextureを更新する。
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;
    // BindしたShader Resourceを解除する。
    void End(const RenderContext& rc) override;
    void Gui() override;
    // MeshがSkinnedかStaticかでVertex Shaderを切り替える。
    void BindVertexShader(const RenderContext& rc_, bool isSkinnedMesh_);
    const DirectX::XMFLOAT4& GetShadeSteps() const { return shadeSteps; }
    DirectX::XMFLOAT4 GetShadeFeather() const { return shadeFeathers; }
    void SetShadeFeather(DirectX::XMFLOAT4 shadeFeather_) { shadeFeathers = shadeFeather_; }
    void SetShadeSettings(const DirectX::XMFLOAT4& shadeSteps_, const DirectX::XMFLOAT4& shadeFeather_);

private:
    struct CbMesh
    {
        // MaterialごとにPixel Shaderへ送るPBR/Toon設定。
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

    // 明暗段階のしきい値。小さい順に整列して使う。
    DirectX::XMFLOAT4 shadeSteps{ 0.45f, 0.45f, 0.45f, 0.45f };
    // 各段階の境界をどれだけぼかすか。
    DirectX::XMFLOAT4 shadeFeathers = { 0.1f,0.1f,0.1f,0.1f };

    // Static/Skinned Mesh用のVertex ShaderとToonPBR Pixel Shader。
    Microsoft::WRL::ComPtr<ID3D11VertexShader> staticVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> skinnedVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    // Mesh Material値を送るConstantBuffer。
    Microsoft::WRL::ComPtr<ID3D11Buffer> meshConstantBuffer;
    // Material Texture/IBL Texture用のSampler。
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
};
