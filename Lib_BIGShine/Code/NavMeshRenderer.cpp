

#include<memory>
#include <RecastDebugDraw.h>
#include "Misc.h"
#include "NavMeshRenderer.h"
#include"CPUResourceLoader.h"
#include"RenderState.h"

#include"imgui.h"

NavMeshRenderer::NavMeshRenderer(ID3D11Device* device, UINT vertexCount)
	:m_Capacity((vertexCount/6) * 6 + 6)
{
	//入力レイアウト
	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,D3D11_APPEND_ALIGNED_ELEMENT,D3D11_INPUT_PER_VERTEX_DATA,0},
	};


	//頂点シェーダー
	{
		auto hr = CPUResourceLoader::LoadVertexShader(device,
			"Shader/NavMeshVS.cso",
			inputElementDesc,
			sizeof(inputElementDesc)/sizeof(D3D11_INPUT_ELEMENT_DESC),
			m_pInputLayout.GetAddressOf(),
			m_pVertexShader.GetAddressOf());



		_ASSERT_EXPR(SUCCEEDED(hr), L"NavMesh頂点シェーダー作成失敗");
	}

	//ピクセルシェーダー
	{
		auto hr = CPUResourceLoader::LoadPixelShader(device,"Shader/NavMeshPS.cso",
			m_pPixelShader.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"NavMeshピクセルシェーダー作成失敗");
	}

	//定数バッファ
	{
		//シーン用バッファ
		D3D11_BUFFER_DESC desc;

		//リセット
		::memset(&desc, 0, sizeof(desc));

		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		desc.ByteWidth = sizeof(ConstantBuffer);
		desc.StructureByteStride = 0;

		HRESULT hr = device->CreateBuffer(&desc, 0, m_pConstantBuffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), "定数バッファ作成失敗");
	}

	//ブレンドステート
	{
		D3D11_BLEND_DESC desc;
		::memset(&desc, 0, sizeof(desc));
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;
		desc.RenderTarget[0].BlendEnable = true;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		HRESULT hr = device->CreateBlendState(&desc, m_pAlphaBlendState.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"アルファブレンドステート作成失敗");
	}

	//深度ステンシルステート
	{
		D3D11_DEPTH_STENCIL_DESC desc;
		::memset(&desc, 0, sizeof(desc));
		desc.DepthEnable = true;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

		HRESULT hr = device->CreateDepthStencilState(&desc, m_pMaskZeroDepthStencilState.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"深度を書き込まない深度ステンシルステート作成失敗");

		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		hr = device->CreateDepthStencilState(&desc,m_pMaskAllDepthStencilState.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"深度を書き込む深度ステンシルステート作成失敗");
	}

	//ラスタライザステート
	{
		D3D11_RASTERIZER_DESC desc;
		::memset(&desc, 0, sizeof(desc));
		desc.FrontCounterClockwise = false;
		desc.DepthBias = 0;
		desc.DepthBiasClamp = 0;
		desc.SlopeScaledDepthBias = 0;
		desc.DepthClipEnable = true;
		desc.ScissorEnable = false;
		desc.MultisampleEnable = true;
		desc.FillMode = D3D11_FILL_SOLID;
		//desc.FillMode = D3D11_FILL_WIREFRAME;
		desc.CullMode = D3D11_CULL_BACK;
		desc.AntialiasedLineEnable = false;

		HRESULT hr = device->CreateRasterizerState(&desc,m_pCullBackRasterizerState.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"ラスタライザステート作成失敗");

		desc.CullMode = D3D11_CULL_NONE;
		hr = device->CreateRasterizerState(&desc, m_pCullNoneRasterizerState.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"ラスタライザステート作成");
	}

	//サンプラステート
	{
		D3D11_SAMPLER_DESC desc;
		::memset(&desc, 0, sizeof(desc));
		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = -FLT_MAX;
		desc.MaxLOD = FLT_MAX;
		desc.BorderColor[0] = 1.0f;
		desc.BorderColor[1] = 1.0f;
		desc.BorderColor[2] = 1.0f;
		desc.BorderColor[3] = 1.0f;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

		HRESULT hr = device->CreateSamplerState(&desc, m_pSamplerState.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), "サンプラーステート作成失敗");
	}

	//頂点バッファ
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = sizeof(Vertex) * m_Capacity;
		//desc.ByteWidth = sizeof(Vertex) * 100000;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		desc.StructureByteStride = 0;


		HRESULT hr = device->CreateBuffer(&desc, nullptr, m_pVertexBuffer.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"頂点バッファ作成失敗");
	}

	//ダミーテクスチャ
	{
		const int size = 8;
		UINT pixels[size * size];
		::memset(pixels, 0xFF, sizeof(pixels));

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = size;
		desc.Height = size;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA data;
		::memset(&data, 0, sizeof(data));

		data.pSysMem = pixels;
		data.SysMemPitch = size;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		HRESULT hr = device->CreateTexture2D(&desc, &data, texture.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr),L"ダミーテクスチャ作成失敗");

		hr = device->CreateShaderResourceView(texture.Get(),nullptr,m_pDummyShaderResourceView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"ダミーテクスチャのシェーダーリソースビュー作成失敗");
	}

	//チェッカーテクスチャ
	{
		const unsigned int col0 = duRGBA(215,215,215,255);
		const unsigned int col1 = duRGBA(255, 255, 255, 255);
		const int size = 64;
		UINT pixels[size * size];

		{
			for (int y = 0;y < size;++y)
			{
				for (int x = 0;x < size;x++)
				{
					pixels[x + y * size] = (x == 0 || y == 0) ? col0 : col1;
				}
			}
		}

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = size;
		desc.Height = size;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		D3D11_SUBRESOURCE_DATA data;
		::memset(&data, 0, sizeof(data));
		data.pSysMem = pixels;
		data.SysMemPitch = size * 4;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
		HRESULT hr = device->CreateTexture2D(&desc, &data, texture.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"チェッカーテクスチャ作成失敗");

		hr = device->CreateShaderResourceView(texture.Get(),nullptr,m_pCheckerShaderResourceView.GetAddressOf());
		_ASSERT_EXPR(SUCCEEDED(hr), L"チェッカーテクスチャのシェーダーリソースビュー作成失敗");
	}
}

//ナビメッシュの描画
void NavMeshRenderer::Draw(dtNavMesh* navMesh, dtNavMeshQuery* navQuery)
{
	if (navMesh != nullptr && navQuery != nullptr)
	{
		//ナビメッシュ描画
		duDebugDrawNavMeshWithClosedList(this, *navMesh, *navQuery, m_NavMeshDrawFlags);
	}
}

void NavMeshRenderer::Draw(dtCrowd* crowd)
{
	if (crowd != nullptr)
	{
		for (int i = 0; i < crowd->getAgentCount(); ++i)
		{
			const dtCrowdAgent* ag = crowd->getAgent(i);
			if (!ag->active) continue;

			const float radius = ag->params.radius;
			const float* pos = ag->npos;

			unsigned int col = duRGBA(0, 0, 0, 32);
			//if (m_agentDebug.idx == i)
			//	col = duRGBA(255, 0, 0, 128);

			duDebugDrawCircle(this, pos[0], pos[1], pos[2], radius, col, 2.0f);
		}

		for (int i = 0; i < crowd->getAgentCount(); ++i)
		{
			const dtCrowdAgent* ag = crowd->getAgent(i);
			if (!ag->active) continue;

			const float height = ag->params.height;
			const float radius = ag->params.radius;
			const float* pos = ag->npos;
			const float* vel = ag->vel;
			const float* dvel = ag->dvel;

			unsigned int col = duRGBA(220, 220, 220, 128);
			if (ag->targetState == DT_CROWDAGENT_TARGET_REQUESTING || ag->targetState == DT_CROWDAGENT_TARGET_WAITING_FOR_QUEUE)
			{
				col = duLerpCol(col, duRGBA(128, 0, 255, 128), 32);
			}
			else if (ag->targetState == DT_CROWDAGENT_TARGET_WAITING_FOR_PATH)
			{
				col = duLerpCol(col, duRGBA(128, 0, 255, 128), 128);
			}
			else if (ag->targetState == DT_CROWDAGENT_TARGET_FAILED)
			{
				col = duRGBA(255, 32, 16, 128);
			}
			else if (ag->targetState == DT_CROWDAGENT_TARGET_VELOCITY)
			{
				col = duLerpCol(col, duRGBA(64, 255, 0, 128), 128);
			}

			duDebugDrawCylinder(this,
				pos[0] - radius, pos[1] + radius * 0.1f, pos[2] - radius,
				pos[0] + radius, pos[1] + height, pos[2] + radius, col
			);

			duDebugDrawCircle(this, pos[0], pos[1] + height, pos[2], radius, col, 2.0f);

			duDebugDrawArrow(this, pos[0], pos[1] + height, pos[2],
				pos[0] + dvel[0], pos[1] + height + dvel[1], pos[2] + dvel[2],
				0.0f, 0.4f, duRGBA(0, 192, 255, 192), 2.0f);
			duDebugDrawArrow(this, pos[0], pos[1] + height, pos[2],
				pos[0] + vel[0], pos[1] + height + vel[1], pos[2] + vel[2],
				0.0f, 0.4f, duRGBA(0, 0, 0, 160), 2.0f);

			Gui();
		}
	}
	
}

//詳細メッシュの描画
void NavMeshRenderer::Draw(rcPolyMeshDetail* polyMeshDetail)
{
	if (polyMeshDetail != nullptr)
	{
		duDebugDrawPolyMeshDetail(this, *polyMeshDetail);
	}
}
void NavMeshRenderer::Gui()
{
	ImGui::Begin("Test");
	ImGui::Checkbox("DrawCheck",&test);
	ImGui::End();
	//test = false;
}
//描画開始
void NavMeshRenderer::Render(ID3D11DeviceContext* context, const Matrix& view, const Matrix& projection)
{
	if (m_Primitives.size() > 0)
	{
		//シェーダー設定
		context->IASetInputLayout(m_pInputLayout.Get());
		context->VSSetShader(m_pVertexShader.Get(),nullptr,0);
		context->PSSetShader(m_pPixelShader.Get(),nullptr,0);

		//定数バッファ設定
		context->VSSetConstantBuffers(0, 1, m_pConstantBuffer.GetAddressOf());

		//定数バッファ更新
		Matrix viewProjection = view * projection;
		ConstantBuffer data;
		
		Matrix WorldMatrix = Matrix::Identity;
		data.m_Wvp = WorldMatrix * viewProjection;


		context->UpdateSubresource(m_pConstantBuffer.Get(), 0, 0, &data, 0, 0);

		//頂点バッファ設定
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(),&stride,&offset);

		const FLOAT blendFactor[] = { 1,1,1,1 };
		context->OMSetBlendState(m_pAlphaBlendState.Get(), blendFactor, 0xFFFFFFFF);


		//プリミティブの頂点
		Vertex* v = m_Vertices.data();


		for (Primitive& primitive : m_Primitives)
		{
			//描画設定
			//プリミティブトポロジー設定
			context->IASetPrimitiveTopology(primitive.m_PrimitiveTopology);

			//深度ステンシルステート設定
			context->OMSetDepthStencilState(primitive.m_pDepthStencilState, 0);

			//ラスタライザステート設定
			context->RSSetState(primitive.m_pRasterizerState);

			//シェーダーリソースビュー設定
			context->PSSetShaderResources(0,1,&primitive.m_pShaderResourceView);

			//描画(頂点バッファサイズが足りない場合は複数回に分けて描画)

			//今回描画するプリミティブの頂点数
			UINT totalVertexCount = primitive.m_VertexCount;
			UINT startVertex = 0;

			//頂点バッファのサイズ
			UINT vertexCount = (totalVertexCount < m_Capacity) ? totalVertexCount : m_Capacity;

			while (startVertex < totalVertexCount)
			{
				//頂点バッファ更新
				D3D11_MAPPED_SUBRESOURCE mappedVB;
				context->Map(m_pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB);
				memcpy(mappedVB.pData, v + startVertex, sizeof(Vertex) * vertexCount);
				context->Unmap(m_pVertexBuffer.Get(), 0);

				//描画実行
				context->Draw(vertexCount, 0);

				//次の描画準備
				startVertex += vertexCount;

				//もしも頂点バッファのサイズが足りなかったとき
				if ((startVertex + vertexCount) > totalVertexCount)
				{
					//残りの頂点数を取得する
					vertexCount = totalVertexCount - startVertex;
				}
			}

			v += totalVertexCount;
		}

		m_Vertices.clear();
		m_Primitives.clear();
	}
}

void NavMeshRenderer::depthMask(bool state)
{
	m_CurrentDepthMask = state;
}

void NavMeshRenderer::texture(bool state)
{
	m_CurrentTexture = state;
}

//プリミティブ描画開始
void NavMeshRenderer::begin(duDebugDrawPrimitives prim,float size)
{
	Primitive& primitive = m_Primitives.emplace_back();

	m_QuadCount = -1;
	m_VertexSize = -1;

	//プリミティブトポロジーとラスタライズステートを変更
	switch (prim)
	{
	case DU_DRAW_POINTS:
		m_VertexSize = size;
		primitive.m_PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		primitive.m_pRasterizerState = m_pCullNoneRasterizerState.Get();
		break;
	case DU_DRAW_LINES:
		m_VertexSize = size;
		primitive.m_PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		primitive.m_pRasterizerState = m_pCullNoneRasterizerState.Get();
		break;
	case DU_DRAW_QUADS:
		m_QuadCount = 0;
		primitive.m_PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		primitive.m_pRasterizerState = m_pCullBackRasterizerState.Get();
		break;
	case DU_DRAW_TRIS:
		primitive.m_PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		primitive.m_pRasterizerState = m_pCullBackRasterizerState.Get();
		break;
	}

	primitive.m_StartVertex = static_cast<UINT>(m_Vertices.size());
	primitive.m_VertexCount = 0;
}

//頂点積み込み
void NavMeshRenderer::vertex(const float* pos, unsigned int color)
{
	vertex(pos[0], pos[1], pos[2], color);
}

void NavMeshRenderer::vertex(const float x, const float y, const float z, unsigned int color)
{
	vertex(x, y, z, color, 0, 0);

}

void NavMeshRenderer::vertex(const float* pos, unsigned int color, const float* uv)
{
	vertex(pos[0], pos[1], pos[2], color, uv[0], uv[1]);
}

void NavMeshRenderer::vertex(const float x, const float y, const float z, unsigned int color, const float u, const float v)
{

	//頂点情報を追加
	Vertex& vertex = m_Vertices.emplace_back();
	vertex.m_Position.x = x;
	vertex.m_Position.y = y;
	vertex.m_Position.z = z;
	vertex.m_Texcoord.x = u;
	vertex.m_Texcoord.y = v;
	vertex.m_Color = color;

	
	//プリミティブの最後列を取得する
	Primitive& primitive = m_Primitives.back();

	//頂点カウントを増やす
	primitive.m_VertexCount++;

	//クアッドの場合はトライアングルリストで表示するための残り2点を追加
	if (m_QuadCount >= 0)
	{
		m_QuadCount++;
		if (m_QuadCount == 4)
		{
			Vertex* v = &vertex - 3;
			m_Vertices.emplace_back(v[0]);
			m_Vertices.emplace_back(v[2]);

			m_QuadCount = 0;
			primitive.m_VertexCount += 2;
		}
	}
}

//プリミティブ終了処理
void NavMeshRenderer::end()
{
	Primitive& primitive = m_Primitives.back();

	//深度ステンシルを保存する
	if (m_CurrentDepthMask)
	{
		primitive.m_pDepthStencilState = m_pMaskAllDepthStencilState.Get();
	}
	else
	{
		primitive.m_pDepthStencilState = m_pMaskZeroDepthStencilState.Get();
	}

	//テクスチャを保存する
	if (m_CurrentTexture)
	{
		primitive.m_pShaderResourceView = m_pCheckerShaderResourceView.Get();
	}
	else
	{
		primitive.m_pShaderResourceView = m_pDummyShaderResourceView.Get();
	}
}