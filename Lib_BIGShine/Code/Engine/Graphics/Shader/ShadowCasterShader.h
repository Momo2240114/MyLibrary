#pragma once
#include "ModelShader.h"

class ShadowCasterShader : public ModelShader
{
public:
    // ShadowMap生成用の深度描画Shaderを作成する。
    ShadowCasterShader(ID3D11Device* device);

    const std::string& GetName() const override
    {
        static const std::string name = "ShadowCaster";
        return name;
    }

    void Begin(const RenderContext& rc) override;

    // MeshごとにWorld/Skinning状態を更新し、ShadowMapへ深度だけ描く。
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;
    void End(const RenderContext& rc) override;

    // Static MeshとSkinned Meshで使用するVSを切り替える。
    void BindVertexShader(const RenderContext& rc_, bool isSkinnedMesh_);

private:
    struct CbShadow
    {
        // Light ViewProjection。ShadowCasterVSで頂点をLight空間へ変換する。
        DirectX::XMFLOAT4X4 lightViewProjection;
    };

    // Shadow描画用のD3D11リソース。
    Microsoft::WRL::ComPtr<ID3D11Buffer> shadowConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  staticVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  skinnedVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
};
