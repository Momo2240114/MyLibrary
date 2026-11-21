#pragma once

#include "ModelShader.h"
#include <DirectXMath.h>

class ToonShader : public ModelShader
{
public:
    // Toon 用 Shader と ConstantBuffer を作成する。
    ToonShader(ID3D11Device* device);
    ~ToonShader() override = default;

    // 名前を保持するメンバ変数
    const std::string& GetName() const override {
        static const std::string name = "Toon";
        return name;
    }

    // Toon 描画パスを開始し、共通の Scene/Light 状態を設定する。
    void Begin(const RenderContext& rc) override;

    // Mesh ごとの Material 値と Toon 階調設定を ConstantBuffer へ送る。
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;

    // ShaderResource などを解除し、次の描画へ状態を残さない。
    void End(const RenderContext& rc) override;
    void Gui();

    const DirectX::XMFLOAT4& GetShadeSteps() const
    {
        return shadeSteps;
    }

    DirectX::XMFLOAT4 GetShadeFeather() const
    {
        return shadeFeathers;
    }

    void SetShadeFeather(DirectX::XMFLOAT4 shadeFeather_)
    {
        shadeFeathers = shadeFeather_;
    }

    void SetShadeSettings(const DirectX::XMFLOAT4& shadeSteps_, DirectX::XMFLOAT4 shadeFeather_);

private:
    // PBRと同様、メッシュ固有のパラメータをb0に集約する
    struct CbMesh
    {
        // Material 由来の基本色と発光色。
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT3 emissiveColor;
        float             metalness;      // Toonでは使わぬがパディングとして保持
        float             roughness;      // Toonでは使わぬがパディングとして保持
        
        float  occlusionStrength;

        // ライト強度をどの段階で区切るか、境界をどれだけぼかすか。
        DirectX::XMFLOAT4 shadeSteps;
        DirectX::XMFLOAT4  shadeFeather;   // 影の境界ぼかし
        float  padding[2];
    };

    // トゥーン用の階調しきい値と境界ぼかし。XYZW で複数段階を表す。
    DirectX::XMFLOAT4 shadeSteps{ 0.05f, 0.05f, 0.05f, 0.05f };
    DirectX::XMFLOAT4 shadeFeathers = { 0.05f, 0.05f, 0.05f, 0.05f };


    // ToonShader 専用の D3D11 リソース。
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        meshConstantBuffer; 
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samplerState;
};
