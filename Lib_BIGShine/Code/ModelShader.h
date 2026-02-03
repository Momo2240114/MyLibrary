#pragma once

#include "ComponentBase.h"
#include "GLTFModel.h"

// =========================================
// ModelShader (Model用シェーダを保持するComponent)
// =========================================
class ModelShader
{
public:
	ModelShader() {}
	virtual ~ModelShader() {}

	// 開始処理
	virtual void Begin(const RenderContext& rc) = 0;

	// 更新処理
	virtual void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) = 0;

	// 終了処理
	virtual void End(const RenderContext& rc) = 0;
};


// class ModelShader : public Component
//{
//public:
//    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
//    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
//    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
//
//    ModelShader() = default;
//    ModelShader(Microsoft::WRL::ComPtr<ID3D11VertexShader> vs,
//        Microsoft::WRL::ComPtr<ID3D11PixelShader> ps,
//        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout)
//        : vertexShader(vs), pixelShader(ps), inputLayout(layout) {
//    }
//
//    const std::string& GetName() const override
//    {
//        static std::string name = "ModelShader";
//        return name;
//    }
//
//    // Begin / Update / End パターン
//    virtual void Begin(ID3D11DeviceContext* context) {}
//    virtual void Update(ID3D11DeviceContext* context, const struct ModelResource::Mesh& mesh) {}
//    virtual void End(ID3D11DeviceContext* context) {}
//};