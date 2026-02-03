#pragma once


#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include "Vector.h"
#include <DebugDraw.h>
#include <DetourDebugDraw.h>
#include <DetourNavMesh.h>
#include <DetourCrowd.h>
#include <Recast.h>


class NavMeshRenderer :public duDebugDraw
{
public:
	NavMeshRenderer(ID3D11Device* device,UINT vertexCount = 4096);
	~NavMeshRenderer()override{}

public:

	//ï`âÊ
	void Draw(dtNavMesh* navMesh,dtNavMeshQuery* navQuery);
	void Draw(dtCrowd* crowd);
	void Draw(rcPolyMeshDetail* polyMeshDetail);

	void Gui();
	//ï`âÊé¿çs
	void Render(ID3D11DeviceContext* context, const Matrix& view, const Matrix& projection);

private:
	void depthMask(bool state) override;
	void texture(bool state)override;
	void begin(duDebugDrawPrimitives prim,float size = 1.0f)override;
	void vertex(const float*pos,unsigned int color)override;
	void vertex(const float x, const float y, const float z, unsigned int color)override;
	void vertex(const float* pos, unsigned int color, const float* uv)override;
	void vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v);
	void end()override;

	bool test = false;
private:
	struct ConstantBuffer
	{
		Matrix m_Wvp;
	};

	struct Primitive
	{
		D3D11_PRIMITIVE_TOPOLOGY m_PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ID3D11RasterizerState* m_pRasterizerState = nullptr;
		ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
		ID3D11ShaderResourceView* m_pShaderResourceView = nullptr;

		UINT m_VertexCount = 0;
		UINT m_StartVertex = 0;
	};

	struct Vertex
	{
		Vector3 m_Position;
		Vector2 m_Texcoord;
		UINT m_Color;
	};

	Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> m_pConstantBuffer;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_pPixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pDummyShaderResourceView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pCheckerShaderResourceView;

	Microsoft::WRL::ComPtr<ID3D11BlendState> m_pAlphaBlendState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_pCullBackRasterizerState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_pCullNoneRasterizerState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pMaskZeroDepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pMaskAllDepthStencilState;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pSamplerState;



	std::vector<Primitive> m_Primitives;
	std::vector<Vertex> m_Vertices;
	UINT m_Capacity = 0;
	int m_QuadCount = 0;
	float m_VertexSize = 0;

	Vertex testVertex[35];


	bool m_CurrentDepthMask = false;
	bool m_CurrentTexture = false;
	unsigned char m_NavMeshDrawFlags = DU_DRAWNAVMESH_OFFMESHCONS | DU_DRAWNAVMESH_CLOSEDLIST;
};