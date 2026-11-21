#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Graphics/Resource/GLTFModel.h"

// =========================================
// ModelShader (Model用シェーダを保持するComponent)
// =========================================
class ModelShader
{
public:
	ModelShader()
	{
	}

	virtual ~ModelShader()
	{
	}

	// シェーダーの名前を返す（派生クラスでの実装を強制）
	virtual const std::string& GetName() const = 0;

	// 開始処理
	virtual void Begin(const RenderContext& rc) = 0;

	// 更新処理
	virtual void Update(const RenderContext& rc, const GltfModelCommponennt::Mesh& mesh) = 0;

	// 終了処理
	virtual void End(const RenderContext& rc) = 0;
	virtual void Gui() {};
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