#pragma once

#include "ModelShader.h"

#include <DirectXMath.h>

class PbrShader : public ModelShader
{
public:
    PbrShader(ID3D11Device* device);
    ~PbrShader() override = default;

    // 名前を保持するメンバ変数
    const std::string& GetName() const override {
        static const std::string name = "PBR";
        return name;
    }

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

private:
    // HLSLの cbuffer CbMesh : register(b0) と完全に一致させる
    struct CbMesh
    {
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT3 emissiveColor;
        float             metalness;
        float             roughness;
        float             occlusionStrength;
        float             padding[2];        //  8バイト (40～47) -> 全体を16の倍数にする
    };

    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        meshConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  linearSampler;
};