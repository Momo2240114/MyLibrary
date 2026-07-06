#pragma once

#include "ModelShader.h"

#include <DirectXMath.h>

class BasicShader : public ModelShader
{
public:
	BasicShader(ID3D11Device* device);
	~BasicShader() override = default;

	// 名前を保持するメンバ変数
	const std::string& GetName() const override {
		static const std::string name = "Basic";
		return name;
	}

	// 開始処理
	void Begin(const RenderContext& rc) override;

	// 更新処理
	void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) override;

	// 終了処理
	void End(const RenderContext& rc) override;

private:
	struct CbMesh
	{
		DirectX::XMFLOAT4		materialColor;
	};

	Microsoft::WRL::ComPtr<ID3D11VertexShader>		vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>		pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>		inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>			meshConstantBuffer;
};


