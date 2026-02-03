#pragma once


#include <vector>
#include <memory>
#include <wrl.h>
#include <map>
#include "ModelResource.h"
#include "ModelShader.h"
#include "BasicShader.h"
#include "GltfModel.h"

#include "ModelShaderID.h"
#include "DecalShader.h"
#include "RenderContext.h"

class DecalComponent;

class ModelRenderSystem
{
public:
	ModelRenderSystem() {}
	~ModelRenderSystem() {}

	void Initialize(ID3D11Device* device);
	// 箱描画
	void PushSubmit(ModelShaderID shaderId, std::shared_ptr<GltfModelCommponennt> model);

	// 描画実行
	void RenderAll(const RenderContext& rc);
	void RenderDecals(const RenderContext& rc);
private:
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

	// GPU側の StructuredBuffer と同じ構造にする
	struct PointLightData
	{
		DirectX::XMFLOAT3 position;
		float range;
		DirectX::XMFLOAT3 color;
		float intensity;
	};

	struct SpotLightData
	{
		DirectX::XMFLOAT3 position;
		float range;
		DirectX::XMFLOAT3 direction;
		float intensity;
		DirectX::XMFLOAT3 color;
		float innerAngle;
		float outerAngle;
		float padding[3]; // HLSL側の16バイトアライメントに合わせる
	};

	struct GBufferDecalConstants {
		DirectX::XMFLOAT4X4 decal_world_matrix;      // VS用：箱を置く
		DirectX::XMFLOAT4X4 decal_inverse_transform; // PS用：床座標を箱空間に戻す
		DirectX::XMFLOAT4   decal_direction;         // 予備・方向制御用
		DirectX::XMFLOAT4   decalBaseColor;          // 色・アルファ
	};

	struct CbSkeleton
	{
		DirectX::XMFLOAT4X4		boneTransforms[256];
	};

	struct DrawInfo
	{
		ModelShaderID				shaderId;
		std::shared_ptr<GltfModelCommponennt>	model;
	};

	struct TransparencyDrawInfo
	{
		ModelShaderID				shaderId;
		const GltfModelCommponennt::Mesh* mesh;
		float					distance;
	};

	std::shared_ptr<ModelShader>			shaders[static_cast<int>(ModelShaderID::EnumCount)];
	std::vector<DrawInfo>					drawInfos;
	std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	skeletonConstantBuffer;

	// --- StructuredBuffer 管理用 ---
	Microsoft::WRL::ComPtr<ID3D11Buffer> lightStructuredBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> lightSRV;
	UINT currentLightBufferSize = 0; // 現在のバッファ容量（個数）

	// 内部的なバッファ更新用メソッド（RenderAllをスッキリさせるため）
	void UpdatePointLightBuffer(ID3D11DeviceContext* dc, const std::vector<PointLight>& lights);

	Microsoft::WRL::ComPtr<ID3D11Buffer> spotLightStructuredBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> spotLightSRV;
	UINT currentSpotLightBufferSize = 0;

	// 更新用関数
	void UpdateSpotLightBuffer(ID3D11DeviceContext* dc, const std::vector<SpotLight>& lights);


private:
	std::shared_ptr<DecalShader> decalShader; // デカール専用シェーダークラス
	Microsoft::WRL::ComPtr<ID3D11Buffer> decalConstantBuffer;
	// 描画登録用リスト
	struct DecalDrawInfo {
		std::shared_ptr<DecalComponent> component;
	};
	std::vector<DecalDrawInfo> decalDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> whiteTextureSRV;
	void CreateDefaultTextures(ID3D11Device* device);

public:
	void PushDecalSubmit(std::shared_ptr<DecalComponent> decal) {
		decalDrawInfos.push_back({ decal });
	}
};

