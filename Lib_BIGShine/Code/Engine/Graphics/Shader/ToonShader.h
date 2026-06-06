#pragma once

#include "ModelShader.h"
#include <DirectXMath.h>

class ToonShader : public ModelShader
{
public:
    ToonShader(ID3D11Device* device);
    ~ToonShader() override = default;

    // 名前を保持するメンバ変数
    const std::string& GetName() const override {
        static const std::string name = "Toon";
        return name;
    }

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;
    void End(const RenderContext& rc) override;
    void Gui();
    float GetShadeStep() const { return shadeStep; }
    float GetShadeFeather() const { return shadeFeather; }
    void SetShadeStep(float shadeStep_) { shadeStep = shadeStep_; }
    void SetShadeFeather(float shadeFeather_) { shadeFeather = shadeFeather_; }
    void SetShadeSettings(float shadeStep_, float shadeFeather_) { shadeStep = shadeStep_; shadeFeather = shadeFeather_; }
private:
    // PBRと同様、メッシュ固有のパラメータをb0に集約する
    struct CbMesh
    {
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT3 emissiveColor;
        float             metalness;      // Toonでは使わぬがパディングとして保持
        float             roughness;      // Toonでは使わぬがパディングとして保持
        float  shadeStep;      // 影のしきい値
        float  shadeFeather;   // 影の境界ぼかし
        float  occlusionStrength; // パディングの代わりにここへ移動
    };

    // トゥーン用パラメータを追加
    float shadeStep = 0.05f;
    float shadeFeather = 0.05f;


    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>        meshConstantBuffer; 
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  samplerState;
};