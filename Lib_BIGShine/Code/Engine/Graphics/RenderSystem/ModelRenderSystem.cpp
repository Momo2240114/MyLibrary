#include "ModelRenderSystem.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Shader/PBRShader.h"
#include "Engine/Graphics/Shader/ToonPBRShader.h"
#include "Engine/Graphics/Shader/ToonShader.h"
#include "ShapeRenderer.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"

//---
// @brief モデル描画に必要なバッファとシェーダーを初期化する
// @param device Direct3D11 デバイス
//---
void ModelRenderSystem::Initialize(ID3D11Device* device)
{
	// シーン用定数バッファ
	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbScene),
		sceneConstantBuffer.GetAddressOf());

	// スケルトン用定数バッファ
	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbSkeleton),
		skeletonConstantBuffer.GetAddressOf());

	// シェーダー生成
	shaders[static_cast<int>(ModelShaderID::Basic)] = std::make_shared<BasicShader>(device);
	shaders[static_cast<int>(ModelShaderID::Toon)] = std::make_shared<ToonShader>(device);
	shaders[static_cast<int>(ModelShaderID::PBR)] = std::make_shared<PbrShader>(device);
	shaders[static_cast<int>(ModelShaderID::ToonPBR)] = std::make_shared<ToonPBRShader>(device);

	// デカールシェーダーの生成
	decalShader = std::make_shared<DecalShader>(device);


	// PS用
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(GBufferDecalConstants); // decalInverseWorldが入る構造体
	device->CreateBuffer(&desc, nullptr, &decalConstantBuffer);

	CreateDefaultTextures(device);
}

void ModelRenderSystem::Gui()
{
	for (auto shader : shaders)
	{
		if (shader)
		{
			if (ImGui::CollapsingHeader(shader->GetName().c_str()))
			{
				shader->Gui();
			}
		}
	}
}

//描画登録
//---
// @brief 描画予定のモデルを描画キューに登録する
// @param shaderId 使用するシェーダーID
// @param model 描画対象のモデルコンポーネント
//---
void ModelRenderSystem::PushSubmit(ModelShaderID shaderId, std::shared_ptr<GltfModelCommponennt> model)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
}

// 描画実行
//---
// @brief 登録されたモデルを全て描画する（不透明 → 半透明の順に描画）
// @param rc 描画コンテキスト（カメラ, ライト, ステートを含む）
//---
void ModelRenderSystem::RenderAll(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シーン用定数バッファ更新
	{
		static LightManager defaultLightManager;
		const LightManager* lightManager = rc.lights ? rc.lights : &defaultLightManager;

		CbScene cbScene{};
		DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
		DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
		DirectX::XMMATRIX VP = V * P;

		DirectX::XMStoreFloat4x4(&cbScene.viewProjection, VP);
		// ★デカール座標復元用の逆行列を計算
		DirectX::XMStoreFloat4x4(&cbScene.invViewProjection, DirectX::XMMatrixInverse(nullptr, VP));

		const auto& directionalLights = lightManager->GetAllDirectionalLights(); // 複数保持していると仮定
		DirectionalLight strongestLight = lightManager->GetDirectionalLight();   // デフォルトの1つ

		// 複数ある場合は、その中で強度が最大のものを探し出す
		for (const auto& light : directionalLights)
		{
			if (light.intensity > strongestLight.intensity)
			{
				strongestLight = light;
			}
		}

		// ここで選ばれた最強の光をバッファにセットする
		cbScene.lightDirection = { strongestLight.direction.x, strongestLight.direction.y, strongestLight.direction.z, strongestLight.intensity };
		cbScene.lightColor = { strongestLight.color.x, strongestLight.color.y, strongestLight.color.z, 0.0f };

		const auto& pointLights = lightManager->GetPointLights();
		const auto& spotLights = lightManager->GetSpotLights();

		// StructuredBuffer を更新
		UpdatePointLightBuffer(dc, pointLights);
		UpdateSpotLightBuffer(dc, spotLights);
		// 点光源の数をセット (HLSLのループ回数に使用)
		// CbScene構造体に int numPointLights; を追加してください
		cbScene.numPointLights = (int)pointLights.size();
		cbScene.numSpotLights = (int)spotLights.size();


		const DirectX::XMFLOAT3& eye = rc.camera->GetEye();
		cbScene.cameraPosition = { eye.x, eye.y, eye.z, 0.0f };

		float nearZ = rc.camera->GetNear();
		float farZ = rc.camera->GetFar();

		// お手本の convert_projection_depth_to_linear_depth に対応する計算
		cbScene.zBufferParameteres.x = (nearZ - farZ) / (farZ * nearZ);
		cbScene.zBufferParameteres.y = 1.0f / nearZ;

		dc->UpdateSubresource(sceneConstantBuffer.Get(), 0, 0, &cbScene, 0, 0);
	}


	// ★重要: StructuredBuffer (点光源) をレジスタ t5 にセット
	if (lightSRV)
	{
		dc->PSSetShaderResources(5, 1, lightSRV.GetAddressOf());
	}
	// ★重要: StructuredBuffer (スポット光源) をレジスタ t6 にセット
	if (spotLightSRV)
	{
		dc->PSSetShaderResources(6, 1, spotLightSRV.GetAddressOf());
	}


	// 定数バッファ設定
	ID3D11Buffer* vsConstantBuffers[] =
	{
		skeletonConstantBuffer.Get(),
		sceneConstantBuffer.Get(),
	};
	ID3D11Buffer* psConstantBuffers[] =
	{
		sceneConstantBuffer.Get(),
	};
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);

	// サンプラステート設定
	ID3D11SamplerState* samplerStates[] =
	{
		rc.renderState->GetSamplerState(SamplerState::LinearWrap)
	};
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

	// レンダーステート設定
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

	// メッシュ描画関数
	auto drawMesh = [&](const GltfModelCommponennt::Mesh& mesh, ModelShader* shader)
		{
			// 頂点バッファ設定
			UINT stride = sizeof(GltfModelCommponennt::Vertex);
			UINT offset = 0;
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// スケルトン用定数バッファ更新
			CbSkeleton cbSkeleton{};
			if (mesh.bones.size() > 0)
			{
				for (size_t i = 0; i < mesh.bones.size(); ++i)
				{
					const GltfModelCommponennt::Bone& bone = mesh.bones.at(i);
					DirectX::XMMATRIX WorldTransform = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
					DirectX::XMMATRIX OffsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
					DirectX::XMMATRIX BoneTransform = OffsetTransform * WorldTransform;
					DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], BoneTransform);
				}
			}
			else
			{
				cbSkeleton.boneTransforms[0] = mesh.node->worldTransform;
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, 0, &cbSkeleton, 0, 0);

			// 更新
			shader->Update(rc, mesh);

			// 描画
			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		};

	DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.camera->GetEye());
	DirectX::XMVECTOR CameraFront = DirectX::XMLoadFloat3(&rc.camera->GetFront());


	GraphicsDeviceManager::Instance().RenderSkyBox(rc);


	// ブレンドステート設定
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);

	// 不透明描画処理
	for (DrawInfo& drawInfo : drawInfos)
	{

		for (const GltfModelCommponennt::Mesh& mesh : drawInfo.model->GetMeshes())
		{
			// マテリアルからシェーダーIDを取得
			ModelShaderID currentShaderId = mesh.material->shaderId;
			ModelShader* shader = shaders[static_cast<int>(currentShaderId)].get();

			// ここでシェーダーを開始
			shader->Begin(rc);

			// 半透明メッシュ登録
			if (mesh.material->alphaMode == GltfModelCommponennt::AlphaMode::Blend ||
				(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f))
			{
				TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
				transparencyDrawInfo.mesh = &mesh;
				// カメラとの距離を算出
				DirectX::XMVECTOR Position = DirectX::XMVectorSet(
					mesh.node->worldTransform._41,
					mesh.node->worldTransform._42,
					mesh.node->worldTransform._43,
					0.0f);
				DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Position, CameraPosition);
				transparencyDrawInfo.distance = DirectX::XMVectorGetX(DirectX::XMVector3Dot(CameraFront, Vec));

				continue;
			}

			// 描画
			drawMesh(mesh, shader);
			shader->End(rc);
		}
	}
	drawInfos.clear();


	// ★ デカール描画実行
	// RenderDecals 内では copyDepthStencilSRV (コピーされた方) を使うこと！
	RenderDecals(rc);

	// カメラから遠い順にソート
	std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs)
		{
			return lhs.distance > rhs.distance;
		});

	// 半透明描画処理
	for (const TransparencyDrawInfo& transparencyDrawInfo : transparencyDrawInfos)
	{
		ModelShader* shader = shaders[static_cast<int>(transparencyDrawInfo.shaderId)].get();

		shader->Begin(rc);

		drawMesh(*transparencyDrawInfo.mesh, shader);

		shader->End(rc);
	}
	transparencyDrawInfos.clear();

	// 定数バッファ設定解除
	for (ID3D11Buffer*& vsConstantBuffer : vsConstantBuffers) { vsConstantBuffer = nullptr; }
	for (ID3D11Buffer*& psConstantBuffer : psConstantBuffers) { psConstantBuffer = nullptr; }
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);
	dc->PSSetConstantBuffers(7, _countof(psConstantBuffers), psConstantBuffers);


	// サンプラステート設定解除
	for (ID3D11SamplerState*& samplerState : samplerStates) { samplerState = nullptr; }
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);
}

void ModelRenderSystem::UpdatePointLightBuffer(ID3D11DeviceContext* dc, const std::vector<PointLight>& lights)
{
	if (lights.empty()) return;

	UINT numLights = static_cast<UINT>(lights.size());
	ID3D11Device* device = nullptr;
	dc->GetDevice(&device);

	// バッファが未作成、またはサイズが不足している場合に作り直す
	if (!lightStructuredBuffer || numLights > currentLightBufferSize)
	{
		lightStructuredBuffer.Reset();
		lightSRV.Reset();

		currentLightBufferSize = numLights;

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(PointLightData) * currentLightBufferSize;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(PointLightData);

		//ここでDeviceがさしているメモリの場所がおかしいというエラーが発生した(今のところ1回だけ)
		device->CreateBuffer(&desc, nullptr, lightStructuredBuffer.GetAddressOf());

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = currentLightBufferSize;

		device->CreateShaderResourceView(lightStructuredBuffer.Get(), &srvDesc, lightSRV.GetAddressOf());
	}

	// データの書き込み
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(dc->Map(lightStructuredBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		PointLightData* dst = static_cast<PointLightData*>(mapped.pData);
		for (const auto& src : lights)
		{
			dst->position = src.position;
			dst->range = src.range;
			dst->color = src.color;
			dst->intensity = src.intensity;
			dst++;
		}
		dc->Unmap(lightStructuredBuffer.Get(), 0);
	}

	// --- ここで必ず解放する ---
	if (device) {
		device->Release(); // カウント -1
	}
}

void ModelRenderSystem::UpdateSpotLightBuffer(ID3D11DeviceContext* dc, const std::vector<SpotLight>& lights)
{
	if (lights.empty()) return;

	UINT numLights = static_cast<UINT>(lights.size());
	ID3D11Device* device = nullptr;
	dc->GetDevice(&device);

	if (!spotLightStructuredBuffer || numLights > currentSpotLightBufferSize)
	{
		spotLightStructuredBuffer.Reset();
		spotLightSRV.Reset();

		currentSpotLightBufferSize = numLights;

		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = sizeof(SpotLight) * currentSpotLightBufferSize; // SpotLight構造体を使用
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = sizeof(SpotLight);

		device->CreateBuffer(&desc, nullptr, spotLightStructuredBuffer.GetAddressOf());

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.NumElements = currentSpotLightBufferSize;

		device->CreateShaderResourceView(spotLightStructuredBuffer.Get(), &srvDesc, spotLightSRV.GetAddressOf());
	}

	// データの書き込み
	D3D11_MAPPED_SUBRESOURCE mapped;
	if (SUCCEEDED(dc->Map(spotLightStructuredBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		SpotLightData* dst = static_cast<SpotLightData*>(mapped.pData);
		for (const auto& src : lights)
		{
			dst->position = src.position;
			dst->range = src.range;
			dst->direction = src.direction;
			dst->intensity = src.intensity;
			dst->color = src.color;
			dst->innerAngle = src.innerAngle;
			dst->outerAngle = src.outerAngle;
			// padding は触らなくてOK（あるいはゼロ初期化）
			dst++;
		}
		dc->Unmap(spotLightStructuredBuffer.Get(), 0);
	}

	// --- ここで必ず解放する ---
	if (device) {
		device->Release(); // カウント -1
	}
}

void ModelRenderSystem::RenderDecals(const RenderContext& rc)
{
	// 必要なリソースが揃っているかチェック
	if (rc.decal->GetDecals().empty() || !rc.shapeSystem || !rc.depthSRV) return;

	ID3D11DeviceContext* dc = rc.deviceContext;
	auto& gdm = GraphicsDeviceManager::Instance();

	// --- ① 深度コピーの儀式 (ターゲットを一時解除) ---
	dc->OMSetRenderTargets(0, nullptr, nullptr);
	dc->CopyResource(gdm.copyDepthStencilTexture.Get(), gdm.depthStencilTexture.Get());

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	gdm.GetSceneSRV()->GetResource(&res);
	dc->CopyResource(gdm.copySceneColorTexture.Get(), res.Get());

	if (gdm.copyNormalTexture) {
		Microsoft::WRL::ComPtr<ID3D11Resource> resNormal;
		gdm.GetNormalSRV()->GetResource(&resNormal); // GetNormalSRV() は sceneNormalSRV を返す想定
		dc->CopyResource(gdm.copyNormalTexture.Get(), resNormal.Get());
	}

	// --- ② 【重要】元のターゲット設定を呼び直して復元する ---
	// これで sceneRTV と depthStencilView が正しくバインドされ、デプスが復活します
	gdm.SetSceneRenderTarget();

	// t0 に「背景のコピー」を渡す
	ID3D11ShaderResourceView* sceneSRV = gdm.copySceneColorSRV.Get();
	dc->PSSetShaderResources(0, 1, &sceneSRV);

	// --- ③ デカール投影の設定 ---
	// t2: ★法線をセット
	ID3D11ShaderResourceView* normalSRV = gdm.copyNormalSRV.Get();
	dc->PSSetShaderResources(2, 1, &normalSRV);

	 
	// コピーした深度(t4)をセット
	ID3D11ShaderResourceView* depthSRV = gdm.copyDepthStenciSRV.Get();
	dc->PSSetShaderResources(4, 1, &depthSRV);

	// 各種ステート (デカール中は深度テストをしない)
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	ID3D11SamplerState* decalSampler = rc.renderState->GetSamplerState(SamplerState::LinearBorder);
	dc->PSSetSamplers(10, 1, &decalSampler);

	// --- ④ 描画ループ ---
	for (const auto& decal : rc.decal->GetDecals())
	{
		DirectX::XMMATRIX World = DirectX::XMLoadFloat4x4(&decal.worldMatrix);
		DirectX::XMMATRIX invWorld = DirectX::XMMatrixInverse(nullptr, World);

		GBufferDecalConstants cb;
		DirectX::XMStoreFloat4x4(&cb.decal_world_matrix, World);
		DirectX::XMStoreFloat4x4(&cb.decal_inverse_transform, invWorld);
		cb.decalBaseColor = decal.color;

		dc->UpdateSubresource(decalConstantBuffer.Get(), 0, nullptr, &cb, 0, 0);
		dc->VSSetConstantBuffers(10, 1, decalConstantBuffer.GetAddressOf());
		dc->PSSetConstantBuffers(10, 1, decalConstantBuffer.GetAddressOf());

		ID3D11ShaderResourceView* texSRV = decal.texture ? decal.texture.Get() : whiteTextureSRV.Get();
		dc->PSSetShaderResources(10, 1, &texSRV);

		rc.shapeSystem->BindBoxVB(dc);
		dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


		// --- 1回目：裏面描画 (ステンシル作成) ---
		// 色を書き込まず、ステンシルだけを更新するために BlendState::None を使用
		dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::None), nullptr, 0xFFFFFFFF);
		decalShader->usePixelShader = false;
		decalShader->Begin(rc);
		dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
		dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::Decal), 1); // 参照値1で塗る
		dc->Draw(rc.shapeSystem->GetBoxVertexCount(), 0);

		// --- 2回目：表面描画 (実際のテクスチャ描画) ---
		// ★テクスチャのアルファを有効にするため Transparency (通常合成) に変更
		dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);
		decalShader->usePixelShader = true;
		decalShader->Begin(rc);
		dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullFront));
		dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::Decal), 0);
		dc->Draw(rc.shapeSystem->GetBoxVertexCount(), 0);

	}
	decalShader->End(rc);

	// --- ⑤ 【最重要】クリーンアップ ---
	// ここでt4(深度SRV)をNULLにしないと、次の描画（シェイプ等）でDSVがバインドエラーを起こして消えます
	ID3D11ShaderResourceView* nullSRV = nullptr;
	ID3D11SamplerState* nullSampler = nullptr;
	dc->PSSetShaderResources(0, 1, &nullSRV); // ★追加
	dc->PSSetShaderResources(2, 1, &nullSRV); // ★追加
	dc->PSSetShaderResources(4, 1, &nullSRV);
	dc->PSSetShaderResources(10, 1, &nullSRV);
	dc->PSSetSamplers(10, 1, &nullSampler); // サンプラーも解除

	// ② この「スロットが空になった瞬間」に、もう一度ターゲットをセットし直す
	// これにより、GPUは「もう読み取り競合はないから、デプスバッファを書き込み用に使えるぞ！」と判断します
	gdm.SetSceneRenderTarget();

	// ③ ステートもシェイプ用に一旦戻しておく
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
	// ブレンドステート設定
	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
}

void ModelRenderSystem::CreateDefaultTextures(ID3D11Device* device)
{
	// 1x1の白いピクセルデータ
	uint32_t white = 0xFFFFFFFF;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = &white;
	data.SysMemPitch = 4;

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = 1;
	desc.Height = 1;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
	device->CreateTexture2D(&desc, &data, texture.GetAddressOf());
	device->CreateShaderResourceView(texture.Get(), nullptr, whiteTextureSRV.GetAddressOf());
}

void ModelRenderSystem::RenderShadowCasters(const RenderContext& rc, int cascadeIndex)
{
}
