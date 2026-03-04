#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <vector.h>
#include <memory>
#include "ParticleComponent.h"

struct RenderContext;

class ParticleRenderSystem {
public:
    void Initialize(ID3D11Device* device);

    // 描画登録
    void PushSubmit(ParticleComponent* component);

    // 更新（物理演算）
    void UpdateAll(ID3D11DeviceContext* context,
        float et,
        const DirectX::XMMATRIX& vpMatrix,
        const std::vector<std::weak_ptr<ParticleComponent>>& components);

    // 描画実行
    void RenderAll(const RenderContext& rc);

private:
    struct DrawInfo {
        ParticleComponent* component;
    };

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
    std::vector<DrawInfo> drawInfos;

    // シェーダー
    Microsoft::WRL::ComPtr<ID3D11VertexShader>   vertexShader;
    Microsoft::WRL::ComPtr<ID3D11GeometryShader> geometryShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>    pixelShader;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader>  computeShader;

    // 共有リソース
    Microsoft::WRL::ComPtr<ID3D11SamplerState>   linearSampler;
    Microsoft::WRL::ComPtr<ID3D11Buffer>         sceneConstantBuffer; // b9用
};