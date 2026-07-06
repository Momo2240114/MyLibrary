#pragma once


#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class ShapeRenderSystem
{
public:
	ShapeRenderSystem() {};
	~ShapeRenderSystem() {}

	void Initialize(ID3D11Device* device);

	// 箱描画
	void DrawBox(
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& angle,
		const DirectX::XMFLOAT3& size,
		const DirectX::XMFLOAT4& color);

	// 球描画
	void DrawSphere(
		const DirectX::XMFLOAT3& position,
		float radius,
		const DirectX::XMFLOAT4& color);

	// カプセル描画
	void DrawCapsule(
		const DirectX::XMFLOAT4X4& transform,
		float radius,
		float height,
		const DirectX::XMFLOAT4& color);

	// 円柱描画
	void DrawCylinder(
		const DirectX::XMFLOAT4X4& transform,
		float radius,
		float height,
		const DirectX::XMFLOAT4& color);

	// 骨描画
	void DrawBone(
		const DirectX::XMFLOAT4X4& transform,
		float length,
		const DirectX::XMFLOAT4& color);

	// 描画実行
	void Render(
		ID3D11DeviceContext* dc,
		const DirectX::XMFLOAT4X4& view,
		const DirectX::XMFLOAT4X4& projection);

	void CreateUnitBoxMesh(ID3D11Device* device);

	// ★ 追加：デカール描画用にBoxメッシュをバインドする
	void BindBoxVB(ID3D11DeviceContext* dc)
	{
		if (!unitBoxMesh.vertexBuffer) return;

		UINT stride = sizeof(DirectX::XMFLOAT3); 
		UINT offset = 0;
		dc->IASetVertexBuffers(0, 1, unitBoxMesh.vertexBuffer.GetAddressOf(), &stride, &offset);
		// プリミティブトポロジも設定
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ★ 追加：Boxの頂点数を取得する
	UINT GetBoxVertexCount() const { return unitBoxMesh.vertexCount; }

private:
	struct Mesh
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		UINT									vertexCount;
	};

	struct Instance
	{
		Mesh* mesh;
		DirectX::XMFLOAT4X4		worldTransform;
		DirectX::XMFLOAT4		color;
	};

	struct CbMesh
	{
		DirectX::XMFLOAT4X4		worldViewProjection;
		DirectX::XMFLOAT4		color;
	};

	// メッシュ生成
	void CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh);

	// 箱メッシュ作成
	void CreateBoxMesh(ID3D11Device* device, float width, float height, float depth);

	// 球メッシュ作成
	void CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 半球メッシュ作成
	void CreateHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 円柱
	void CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions);

	// 骨メッシュ作成
	void CreateBoneMesh(ID3D11Device* device, float length);

private:
	Mesh										boxMesh;
	Mesh										sphereMesh;
	Mesh										halfSphereMesh;
	Mesh										cylinderMesh;
	Mesh										boneMesh;
	Mesh unitBoxMesh;  // ★追加：デカール用 (1x1x1)
	std::vector<Instance>						instances;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer;


};
