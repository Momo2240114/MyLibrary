#pragma once


#include <vector>
#include <memory>
#include <wrl.h>
#include <map>
#include "Engine/Graphics/Resource/ModelResource.h"
#include "Engine/Graphics/Shader/ModelShader.h"
#include "Engine/Graphics/Shader/BasicShader.h"
#include "Engine/Graphics/Resource/GltfModel.h"

#include "Engine/Graphics/Shader/ModelShaderID.h"
#include "Engine/Graphics/Shader/DecalShader.h"
#include "Engine/Graphics/Core/RenderContext.h"
#include <nlohmann/json_fwd.hpp>

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
	const auto& GetShaders() const { return shaders; }
	void ShaderSettingGui();
	void ShadowSettingGui();
	void OutLineSettingGui();
	void SerializeGraphicsSettings(nlohmann::json& j_) const;
	void DeserializeGraphicsSettings(const nlohmann::json& j_);
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

	struct CbOutline
	{
		DirectX::XMFLOAT4 outlineColor;
		float outlineThickness;
		DirectX::XMFLOAT3 padding;
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

	struct CbShadowData {
		DirectX::XMFLOAT4X4 lightViewProjections[4];
		DirectX::XMFLOAT4   cascadeSplits;
		DirectX::XMFLOAT4   cascadeShadowBias{ 0.001f, 0.002f, 0.003f, 0.004f };
		float shadowAttenuation = 0.35f;
		DirectX::XMFLOAT3 padding{};
	};

	std::shared_ptr<ModelShader>			shaders[static_cast<int>(ModelShaderID::EnumCount)];
	std::vector<DrawInfo>					drawInfos;
	std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	skeletonConstantBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> outlineVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> outlinePixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> outlineInputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer> outlineConstantBuffer;
	DirectX::XMFLOAT4 outlineColor = { 0.02f, 0.02f, 0.025f, 1.0f };
	float outlineThickness = 0.00f;

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

	struct CbShadowCaster
	{
		DirectX::XMFLOAT4X4 lightViewProjection;
	};

	Microsoft::WRL::ComPtr<ID3D11Buffer> cascadeShadowConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> shadowCasterConstantBuffer;

	DirectX::XMFLOAT4 cascadeShadowBias =
	{
		0.001f,
		0.002f,
		0.003f,
		0.004f,
	};

	bool useCascadeShadow;
	float shadowAttenuation = 0.35f;

	void RenderCascadeShadowMaps(RenderContext& rc, const DirectX::XMFLOAT3& lightDirection);
	void RenderShadowCasters(const RenderContext& rc, int cascadeIndex);
public:
	void PushDecalSubmit(std::shared_ptr<DecalComponent> decal) {
		decalDrawInfos.push_back({ decal });
	}
};

