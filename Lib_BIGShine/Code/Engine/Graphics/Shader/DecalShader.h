#pragma once
#include "ModelShader.h"
#include <DirectXMath.h>

class DecalShader : public ModelShader
{
public:
    DecalShader(ID3D11Device* device);
    ~DecalShader() override = default;

	// 名前を保持するメンバ変数
	const std::string& GetName() const override {
		static const std::string name = "Decal";
		return name;
	}

    void Begin(const RenderContext& rc) override;
    void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override; // 基本使用しない
    void End(const RenderContext& rc) override;

	bool usePixelShader;
	struct CbScene
	{
		DirectX::XMFLOAT4X4		viewProjection;
		DirectX::XMFLOAT4		lightDirection;
		DirectX::XMFLOAT4		lightColor;
		DirectX::XMFLOAT4		cameraPosition;

		DirectX::XMFLOAT4 zBufferParameteres;

		int numPointLights;   // 有効なポイントライトの数
		int numSpotLights;   // 有効なスポットライトの数
		float padding[2];     // 16バイト境界合わせ

		//逆行列
		DirectX::XMFLOAT4X4 invViewProjection;
	};
private:
    Microsoft::WRL::ComPtr<ID3D11VertexShader>  vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   inputLayout;

    // サンプラー (s0: Point for Depth, s1: Linear for Decal)
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  pointSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>  linearBorderSampler;
};