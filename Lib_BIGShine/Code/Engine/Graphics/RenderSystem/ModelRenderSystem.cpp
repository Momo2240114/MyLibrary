#include "ModelRenderSystem.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Shader/PBRShader.h"
#include "Engine/Graphics/Shader/ToonPBRShader.h"
#include "Engine/Graphics/Shader/ToonShader.h"
#include "Engine/Graphics/Shader/ShadowCasterShader.h"
#include "ShapeRenderer.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include <algorithm>
#include <nlohmann/json.hpp>

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

	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbShadowData),
		cascadeShadowConstantBuffer.GetAddressOf());

	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbShadowCaster),
		shadowCasterConstantBuffer.GetAddressOf());

	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbOutline),
		outlineConstantBuffer.GetAddressOf());

	CPUResourceLoader::LoadVertexShader(
		device,
		"Shader/OutlineVS.cso",
		GltfModelCommponennt::InputElementDescs.data(),
		static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
		outlineInputLayout.GetAddressOf(),
		outlineVertexShader.GetAddressOf());

	CPUResourceLoader::LoadPixelShader(
		device,
		"Shader/OutlinePS.cso",
		outlinePixelShader.GetAddressOf());

	CPUResourceLoader::LoadVertexShader(
		device,
		"Shader/PBRVS.cso",
		GltfModelCommponennt::InputElementDescs.data(),
		static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
		gBufferInputLayout.GetAddressOf(),
		gBufferVertexShader.GetAddressOf());

	CPUResourceLoader::LoadPixelShader(
		device,
		"Shader/GBufferPS.cso",
		gBufferPixelShader.GetAddressOf());

	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbGBufferMaterial),
		gBufferMaterialConstantBuffer.GetAddressOf());

	// シェーダー生成
	shaders[static_cast<int>(ModelShaderID::Basic)] = std::make_shared<BasicShader>(device);
	shaders[static_cast<int>(ModelShaderID::Toon)] = std::make_shared<ToonShader>(device);
	shaders[static_cast<int>(ModelShaderID::PBR)] = std::make_shared<PbrShader>(device);
	shaders[static_cast<int>(ModelShaderID::ToonPBR)] = std::make_shared<ToonPBRShader>(device);
	shaders[static_cast<int>(ModelShaderID::ShadowCaster)] = std::make_shared<ShadowCasterShader>(device);

	// デカールシェーダーの生成
	decalShader = std::make_shared<DecalShader>(device);
	lightingShader = std::make_shared<LightingShader>(device);


	// PS用
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.ByteWidth = sizeof(GBufferDecalConstants); // decalInverseWorldが入る構造体
	device->CreateBuffer(&desc, nullptr, &decalConstantBuffer);

	CreateDefaultTextures(device);
}


void ModelRenderSystem::SerializeGraphicsSettings(nlohmann::json& j_) const
{
    if (auto toonShader_ = std::dynamic_pointer_cast<ToonShader>(shaders[static_cast<int>(ModelShaderID::Toon)]))
    {
        const DirectX::XMFLOAT4& shadeSteps_ = toonShader_->GetShadeSteps();
		const DirectX::XMFLOAT4& shadeFeathers_ = toonShader_->GetShadeFeather();
        j_["Toon"] = {
            {"shadeSteps", {shadeSteps_.x, shadeSteps_.y, shadeSteps_.z, shadeSteps_.w}},
			{"shadeFeathers",{ shadeFeathers_.x,shadeFeathers_.y,shadeFeathers_.z,shadeFeathers_.w}}
        };
    }

    if (auto toonPbrShader_ = std::dynamic_pointer_cast<ToonPBRShader>(shaders[static_cast<int>(ModelShaderID::ToonPBR)]))
    {
        const DirectX::XMFLOAT4& shadeSteps_ = toonPbrShader_->GetShadeSteps();
        const DirectX::XMFLOAT4& shadeFeathers_ = toonPbrShader_->GetShadeFeather();
        j_["ToonPBR"] = {
            {"shadeSteps", {shadeSteps_.x, shadeSteps_.y, shadeSteps_.z, shadeSteps_.w}},
			{"shadeFeathers",{ shadeFeathers_.x,shadeFeathers_.y,shadeFeathers_.z,shadeFeathers_.w}}
        };
    }

    const CascadeShadowMap& cascadeShadowMap_ = GraphicsDeviceManager::Instance().GetCascadeShadowMap();
    const float* splits_ = cascadeShadowMap_.GetCascadeSplitDistances();

    j_["ShadowMap"] = {
        {"useCascadeShadow", cascadeShadowMap_.IsUseCascadeShadow()},
        {"displayCascadeArea", cascadeShadowMap_.IsDisplayCascadeArea()},
        {"cascadeSplitDistances", {splits_[0], splits_[1], splits_[2], splits_[3]}},
        {"shadowAttenuation", shadowAttenuation},
        {"cascadeShadowBias", {cascadeShadowBias.x, cascadeShadowBias.y, cascadeShadowBias.z, cascadeShadowBias.w}}
    };

    j_["ToonOutline"] = {
        {"color", {outlineColor.x, outlineColor.y, outlineColor.z, outlineColor.w}},
        {"thickness", outlineThickness}
    };
}

void ModelRenderSystem::DeserializeGraphicsSettings(const nlohmann::json& j_)
{
    if (j_.contains("Toon"))
    {
        const auto& toonJson_ = j_["Toon"];
        if (auto toonShader_ = std::dynamic_pointer_cast<ToonShader>(shaders[static_cast<int>(ModelShaderID::Toon)]))
        {
            DirectX::XMFLOAT4 shadeSteps_ = toonShader_->GetShadeSteps();
            if (toonJson_.contains("shadeSteps") && toonJson_["shadeSteps"].is_array())
            {
                const auto& stepsJson_ = toonJson_["shadeSteps"];
                if (stepsJson_.size() > 0) shadeSteps_.x = stepsJson_[0].get<float>();
                if (stepsJson_.size() > 1) shadeSteps_.y = stepsJson_[1].get<float>();
                if (stepsJson_.size() > 2) shadeSteps_.z = stepsJson_[2].get<float>();
                if (stepsJson_.size() > 3) shadeSteps_.w = stepsJson_[3].get<float>();
            }
            else if (toonJson_.contains("shadeStep"))
            {
                const float shadeStep_ = toonJson_["shadeStep"].get<float>();
                shadeSteps_ = { shadeStep_, shadeStep_, shadeStep_, shadeStep_ };
            }
			DirectX::XMFLOAT4 shadeFeathers_ = toonShader_->GetShadeFeather();
			if (toonJson_.contains("shadeFeathers") && toonJson_["shadeFeathers"].is_array())
			{
				const auto& feathersJson_ = toonJson_["shadeFeathers"];
				if (feathersJson_.size() > 0) shadeFeathers_.x = feathersJson_[0].get<float>();
				if (feathersJson_.size() > 1) shadeFeathers_.y = feathersJson_[1].get<float>();
				if (feathersJson_.size() > 2) shadeFeathers_.z = feathersJson_[2].get<float>();
				if (feathersJson_.size() > 3) shadeFeathers_.w = feathersJson_[3].get<float>();
			}
			else if (toonJson_.contains("shadeFeather"))
			{
				// 過去の古いセーブデータ（単一の数値）だった場合は、すべての段階に一律で適用し破綻を防ぐ
				//const float oldFeather_ = toonJson_["shadeFeather"].get<float>();
				//shadeFeathers_ = { oldFeather_, oldFeather_, oldFeather_, oldFeather_ };
			}
            toonShader_->SetShadeSettings(shadeSteps_,shadeFeathers_);
        }
    }

    if (j_.contains("ToonPBR"))
    {
        const auto& toonPbrJson_ = j_["ToonPBR"];
        if (auto toonPbrShader_ = std::dynamic_pointer_cast<ToonPBRShader>(shaders[static_cast<int>(ModelShaderID::ToonPBR)]))
        {
            DirectX::XMFLOAT4 shadeSteps_ = toonPbrShader_->GetShadeSteps();
            if (toonPbrJson_.contains("shadeSteps") && toonPbrJson_["shadeSteps"].is_array())
            {
                const auto& stepsJson_ = toonPbrJson_["shadeSteps"];
                if (stepsJson_.size() > 0) shadeSteps_.x = stepsJson_[0].get<float>();
                if (stepsJson_.size() > 1) shadeSteps_.y = stepsJson_[1].get<float>();
                if (stepsJson_.size() > 2) shadeSteps_.z = stepsJson_[2].get<float>();
                if (stepsJson_.size() > 3) shadeSteps_.w = stepsJson_[3].get<float>();
            }
            else if (toonPbrJson_.contains("shadeStep"))
            {
                const float shadeStep_ = toonPbrJson_["shadeStep"].get<float>();
                shadeSteps_ = { shadeStep_, shadeStep_, shadeStep_, shadeStep_ };
            }
			DirectX::XMFLOAT4 shadeFeathers_ = toonPbrShader_->GetShadeFeather();
			if (toonPbrJson_.contains("shadeFeathers") && toonPbrJson_["shadeFeathers"].is_array())
			{
				const auto& feathersJson_ = toonPbrJson_["shadeFeathers"];
				if (feathersJson_.size() > 0) shadeFeathers_.x = feathersJson_[0].get<float>();
				if (feathersJson_.size() > 1) shadeFeathers_.y = feathersJson_[1].get<float>();
				if (feathersJson_.size() > 2) shadeFeathers_.z = feathersJson_[2].get<float>();
				if (feathersJson_.size() > 3) shadeFeathers_.w = feathersJson_[3].get<float>();
			}
			else if (toonPbrJson_.contains("shadeFeather"))
			{
				//// 過去の古いセーブデータ（単一の数値）だった場合は、すべての段階に一律で適用し破綻を防ぐ
				//const float oldFeather_ = toonPbrJson_["shadeFeather"].get<float>();
				//shadeFeathers_ = { oldFeather_, oldFeather_, oldFeather_, oldFeather_ };
			}
			toonPbrShader_->SetShadeSettings(shadeSteps_, shadeFeathers_);
        }
    }

    if (j_.contains("ShadowMap"))
    {
        const auto& shadowJson_ = j_["ShadowMap"];
        CascadeShadowMap& cascadeShadowMap_ = GraphicsDeviceManager::Instance().GetCascadeShadowMap();
        cascadeShadowMap_.SetUseCascadeShadow(shadowJson_.value("useCascadeShadow", cascadeShadowMap_.IsUseCascadeShadow()));
        cascadeShadowMap_.SetDisplayCascadeArea(shadowJson_.value("displayCascadeArea", cascadeShadowMap_.IsDisplayCascadeArea()));

        if (shadowJson_.contains("cascadeSplitDistances") && shadowJson_["cascadeSplitDistances"].is_array())
        {
            const auto& splitsJson_ = shadowJson_["cascadeSplitDistances"];
            for (int i_ = 0; i_ < CascadeShadowMap::CascadeCount && i_ < static_cast<int>(splitsJson_.size()); ++i_)
            {
                cascadeShadowMap_.SetCascadeSplitDistance(i_, splitsJson_[i_].get<float>());
            }
        }

        shadowAttenuation = shadowJson_.value("shadowAttenuation", shadowAttenuation);

        if (shadowJson_.contains("cascadeShadowBias") && shadowJson_["cascadeShadowBias"].is_array())
        {
            const auto& biasJson_ = shadowJson_["cascadeShadowBias"];
            if (biasJson_.size() > 0) cascadeShadowBias.x = biasJson_[0].get<float>();
            if (biasJson_.size() > 1) cascadeShadowBias.y = biasJson_[1].get<float>();
            if (biasJson_.size() > 2) cascadeShadowBias.z = biasJson_[2].get<float>();
            if (biasJson_.size() > 3) cascadeShadowBias.w = biasJson_[3].get<float>();
        }
    }

    if (j_.contains("ToonOutline"))
    {
        const auto& outlineJson_ = j_["ToonOutline"];
        outlineThickness = outlineJson_.value("thickness", outlineThickness);

        if (outlineJson_.contains("color") && outlineJson_["color"].is_array())
        {
            const auto& colorJson_ = outlineJson_["color"];
            if (colorJson_.size() > 0) outlineColor.x = colorJson_[0].get<float>();
            if (colorJson_.size() > 1) outlineColor.y = colorJson_[1].get<float>();
            if (colorJson_.size() > 2) outlineColor.z = colorJson_[2].get<float>();
            if (colorJson_.size() > 3) outlineColor.w = colorJson_[3].get<float>();
        }
    }
}
void ModelRenderSystem::ShaderSettingGui()
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

	if (ImGui::CollapsingHeader("Toon Outline"))
	{
		ImGui::ColorEdit4("Outline Color", reinterpret_cast<float*>(&outlineColor));
		ImGui::DragFloat("Outline Thickness", &outlineThickness, 0.0001f, 0.0f, 0.2f, "%.3f");
	}
}

void ModelRenderSystem::ShadowSettingGui()
{
	CascadeShadowMap& cascadeShadowMap =
		GraphicsDeviceManager::Instance().GetCascadeShadowMap();

	if (ImGui::CollapsingHeader("Cascade Shadow Map"))
	{
		bool useCascadeShadow = cascadeShadowMap.IsUseCascadeShadow();
		if (ImGui::Checkbox("Use Cascade Shadow", &useCascadeShadow))
		{
			cascadeShadowMap.SetUseCascadeShadow(useCascadeShadow);
		}

		bool displayCascadeArea = cascadeShadowMap.IsDisplayCascadeArea();
		if (ImGui::Checkbox("Display Cascade Area", &displayCascadeArea))
		{
			cascadeShadowMap.SetDisplayCascadeArea(displayCascadeArea);
		}

		float* splits = cascadeShadowMap.GetCascadeSplitDistances();

		ImGui::DragFloat("Cascade 0 Far", &splits[0], 0.1f, 0.1f, 5000.0f);
		ImGui::DragFloat("Cascade 1 Far", &splits[1], 0.1f, splits[0] + 0.1f, 5000.0f);
		ImGui::DragFloat("Cascade 2 Far", &splits[2], 0.1f, splits[1] + 0.1f, 5000.0f);
		ImGui::DragFloat("Cascade 3 Far", &splits[3], 0.1f, splits[2] + 0.1f, 5000.0f);


		ImGui::Separator();

		ImGui::Text("Shadow Strength");
		ImGui::SliderFloat("Shadow Attenuation", &shadowAttenuation, 0.0f, 1.0f);

		ImGui::Separator();

		ImGui::Text("Cascade Bias");
		ImGui::DragFloat("Bias 0", &cascadeShadowBias.x, 0.00001f, 0.0f, 0.1f, "%.6f");
		ImGui::DragFloat("Bias 1", &cascadeShadowBias.y, 0.00001f, 0.0f, 0.1f, "%.6f");
		ImGui::DragFloat("Bias 2", &cascadeShadowBias.z, 0.00001f, 0.0f, 0.1f, "%.6f");
		ImGui::DragFloat("Bias 3", &cascadeShadowBias.w, 0.00001f, 0.0f, 0.1f, "%.6f");


		DirectX::XMFLOAT4 appliedSplits = cascadeShadowMap.GetCascadeSplits();

		ImGui::Separator();
		ImGui::Text("Applied Splits");
		ImGui::Text("%.2f / %.2f / %.2f / %.2f",
			appliedSplits.x,
			appliedSplits.y,
			appliedSplits.z,
			appliedSplits.w);
	}
}

void ModelRenderSystem::OutLineSettingGui()
{
	ImGui::ColorEdit4("OutLineColor", &outlineColor.x);
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

		if (rc.cascadeShadowMap && rc.camera && rc.cascadeShadowMap->IsUseCascadeShadow())
		{
			DirectX::XMFLOAT3 shadowLightDirection{ strongestLight.direction.x, strongestLight.direction.y, strongestLight.direction.z };
			RenderContext shadowRc = rc;
			RenderCascadeShadowMaps(shadowRc, shadowLightDirection);
		}

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

	if (rc.cascadeShadowMap)
	{
		CbShadowData shadowData{};
		for (int i = 0; i < CascadeShadowMap::CascadeCount; ++i)
		{
			shadowData.lightViewProjections[i] = rc.cascadeShadowMap->GetLightViewProjection(i);
		}
		shadowData.cascadeSplits = rc.cascadeShadowMap->GetCascadeSplits();
		shadowData.cascadeShadowBias = cascadeShadowBias;
		shadowData.shadowAttenuation = shadowAttenuation;

		dc->UpdateSubresource(cascadeShadowConstantBuffer.Get(), 0, nullptr, &shadowData, 0, 0);
		dc->PSSetConstantBuffers(8, 1, cascadeShadowConstantBuffer.GetAddressOf());

		ID3D11ShaderResourceView* shadowSRVs[CascadeShadowMap::CascadeCount] = {};
		for (int i = 0; i < CascadeShadowMap::CascadeCount; ++i)
		{
			shadowSRVs[i] = rc.cascadeShadowMap->GetSRV(i);
		}
		dc->PSSetShaderResources(13, CascadeShadowMap::CascadeCount, shadowSRVs);
	}

	// サンプラステート設定
	ID3D11SamplerState* samplerStates[] =
	{
		rc.renderState->GetSamplerState(SamplerState::LinearWrap)
	};
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);
	ID3D11SamplerState* shadowSampler = rc.renderState->GetSamplerState(SamplerState::LinearBorder);
	dc->PSSetSamplers(1, 1, &shadowSampler);

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

	auto drawGBufferMesh = [&](const GltfModelCommponennt::Mesh& mesh, ModelShaderID shaderId_)
		{
			UINT stride_ = sizeof(GltfModelCommponennt::Vertex);
			UINT offset_ = 0;
			dc->IASetInputLayout(gBufferInputLayout.Get());
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride_, &offset_);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			dc->VSSetShader(gBufferVertexShader.Get(), nullptr, 0);
			dc->PSSetShader(gBufferPixelShader.Get(), nullptr, 0);
			dc->PSSetConstantBuffers(0, 1, gBufferMaterialConstantBuffer.GetAddressOf());

			CbSkeleton cbSkeleton_{};
			if (!mesh.bones.empty())
			{
				for (size_t boneIndex_ = 0; boneIndex_ < mesh.bones.size(); ++boneIndex_)
				{
					const GltfModelCommponennt::Bone& bone_ = mesh.bones.at(boneIndex_);
					DirectX::XMMATRIX worldTransform_ = DirectX::XMLoadFloat4x4(&bone_.node->worldTransform);
					DirectX::XMMATRIX offsetTransform_ = DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
					DirectX::XMStoreFloat4x4(&cbSkeleton_.boneTransforms[boneIndex_], offsetTransform_ * worldTransform_);
				}
			}
			else
			{
				cbSkeleton_.boneTransforms[0] = mesh.node->worldTransform;
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &cbSkeleton_, 0, 0);

			CbGBufferMaterial material_{};
			material_.baseColor = mesh.material->baseColor;
			material_.emissiveColor = mesh.material->emissiveColor;
			material_.metalness = mesh.material->metalness;
			material_.roughness = mesh.material->roughness;
			material_.occlusionStrength = mesh.material->occlusionStrength;
			material_.toonPbrBlend = mesh.material->toonPbrBlend;
			material_.shadingModel = static_cast<int>(shaderId_);
			dc->UpdateSubresource(gBufferMaterialConstantBuffer.Get(), 0, nullptr, &material_, 0, 0);

			ID3D11ShaderResourceView* materialSrvs_[] =
			{
				mesh.material->baseMap.Get(),
				mesh.material->normalMap.Get(),
				mesh.material->emissiveMap.Get(),
				mesh.material->occlusionMap.Get(),
				mesh.material->metalnessRoughnessMap.Get(),
			};
			dc->PSSetShaderResources(0, _countof(materialSrvs_), materialSrvs_);
			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		};

	auto isOutlineTarget = [](ModelShaderID shaderId)
		{
			return shaderId == ModelShaderID::Toon || shaderId == ModelShaderID::ToonPBR;
		};

	auto drawOutlineMesh = [&](const GltfModelCommponennt::Mesh& mesh)
		{
			if (!mesh.material->disableOutline || mesh.material->outlineThicknessOffset < 0)
			{
				return;
			}

			CbOutline cbOutline{};
			cbOutline.outlineColor = outlineColor;
			cbOutline.outlineThickness = mesh.material->outlineThicknessOffset;
			dc->UpdateSubresource(outlineConstantBuffer.Get(), 0, nullptr, &cbOutline, 0, 0);

			CbSkeleton cbSkeleton_{};
			if (!mesh.bones.empty())
			{
				for (size_t boneIndex_ = 0; boneIndex_ < mesh.bones.size(); ++boneIndex_)
				{
					const GltfModelCommponennt::Bone& bone_ = mesh.bones.at(boneIndex_);
					DirectX::XMMATRIX worldTransform_ = DirectX::XMLoadFloat4x4(&bone_.node->worldTransform);
					DirectX::XMMATRIX offsetTransform_ = DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
					DirectX::XMStoreFloat4x4(&cbSkeleton_.boneTransforms[boneIndex_], offsetTransform_ * worldTransform_);
				}
			}
			else
			{
				cbSkeleton_.boneTransforms[0] = mesh.node->worldTransform;
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &cbSkeleton_, 0, 0);

			UINT stride = sizeof(GltfModelCommponennt::Vertex);
			UINT offset = 0;
			dc->IASetInputLayout(outlineInputLayout.Get());
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			dc->VSSetShader(outlineVertexShader.Get(), nullptr, 0);
			dc->PSSetShader(outlinePixelShader.Get(), nullptr, 0);
			dc->VSSetConstantBuffers(0, 1, outlineConstantBuffer.GetAddressOf());
			dc->PSSetConstantBuffers(0, 1, outlineConstantBuffer.GetAddressOf());
			dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
			dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
			dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullFront));

			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);

			dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));
			dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
		};

	DirectX::XMVECTOR CameraPosition = DirectX::XMLoadFloat3(&rc.camera->GetEye());
	DirectX::XMVECTOR CameraFront = DirectX::XMLoadFloat3(&rc.camera->GetFront());


	GraphicsDeviceManager& graphics_ = GraphicsDeviceManager::Instance();
	const bool useDeferredRendering_ = rc.cascadeShadowMap != nullptr;
	if (useDeferredRendering_)
	{
		graphics_.SetGBufferRenderTargets();
	}
	else
	{
		graphics_.RenderSkyBox(rc);
	}


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

			// 半透明メッシュ登録
			if (mesh.material->alphaMode == GltfModelCommponennt::AlphaMode::Blend ||
				(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f))
			{
				TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
				transparencyDrawInfo.shaderId = currentShaderId;
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

			if (useDeferredRendering_)
			{
				drawGBufferMesh(mesh, currentShaderId);
			}
			else
			{
				shader->Begin(rc);
				drawMesh(mesh, shader);
				if (isOutlineTarget(currentShaderId))
				{
					drawOutlineMesh(mesh);
				}
				shader->End(rc);
			}
		}
	}

	if (useDeferredRendering_)
	{
		ID3D11ShaderResourceView* nullMaterialSrvs_[5] = {};
		dc->PSSetShaderResources(0, _countof(nullMaterialSrvs_), nullMaterialSrvs_);
		graphics_.SetSceneColorRenderTarget();
		dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
		dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
		dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));

		ID3D11ShaderResourceView* iblSrvs_[] =
		{
			graphics_.GetIBLDiffuseSRV(),
			graphics_.GetIBLSpecularSRV(),
			graphics_.GetIBLLutGGXSRV(),
		};
		dc->PSSetShaderResources(10, _countof(iblSrvs_), iblSrvs_);

		DirectX::XMFLOAT4 toonSteps_{ 0.05f, 0.05f, 0.05f, 0.05f };
		DirectX::XMFLOAT4 toonFeathers_{ 0.05f, 0.05f, 0.05f, 0.05f };
		DirectX::XMFLOAT4 toonPbrSteps_{ 0.45f, 0.45f, 0.45f, 0.45f };
		DirectX::XMFLOAT4 toonPbrFeathers_{ 0.1f, 0.1f, 0.1f, 0.1f };
		if (auto toonShader_ = std::dynamic_pointer_cast<ToonShader>(shaders[static_cast<int>(ModelShaderID::Toon)]))
		{
			toonSteps_ = toonShader_->GetShadeSteps();
			toonFeathers_ = toonShader_->GetShadeFeather();
		}
		if (auto toonPbrShader_ = std::dynamic_pointer_cast<ToonPBRShader>(shaders[static_cast<int>(ModelShaderID::ToonPBR)]))
		{
			toonPbrSteps_ = toonPbrShader_->GetShadeSteps();
			toonPbrFeathers_ = toonPbrShader_->GetShadeFeather();
		}
		lightingShader->Execute(
			rc,
			graphics_.GetGBufferBaseColorSRV(),
			graphics_.GetGBufferNormalSRV(),
			graphics_.GetGBufferMaterialSRV(),
			graphics_.GetGBufferEmissiveSRV(),
			graphics_.GetMainDepthSRV(),
			lightSRV.Get(),
			spotLightSRV.Get(),
			sceneConstantBuffer.Get(),
			toonSteps_,
			toonFeathers_,
			toonPbrSteps_,
			toonPbrFeathers_);

		graphics_.SetSceneRenderTarget();
		graphics_.RenderSkyBox(rc);

		for (DrawInfo& drawInfo : drawInfos)
		{
			for (const GltfModelCommponennt::Mesh& mesh : drawInfo.model->GetMeshes())
			{
				if (mesh.material->alphaMode != GltfModelCommponennt::AlphaMode::Blend &&
					isOutlineTarget(mesh.material->shaderId))
				{
					drawOutlineMesh(mesh);
				}
			}
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
	ID3D11SamplerState* nullSampler = nullptr;
	dc->PSSetSamplers(1, 1, &nullSampler);
	ID3D11Buffer* nullBuffer = nullptr;
	dc->PSSetConstantBuffers(8, 1, &nullBuffer);
	ID3D11ShaderResourceView* nullShadowSRVs[CascadeShadowMap::CascadeCount] = {};
	dc->PSSetShaderResources(13, CascadeShadowMap::CascadeCount, nullShadowSRVs);
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
	if (!rc.decal || !rc.shapeSystem || !rc.depthSRV || !rc.deviceContext || !rc.renderState) return;
	if (rc.decal->GetDecals().empty()) return;

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
	ID3D11ShaderResourceView* depthSRV = gdm.copyDepthStencilSRV.Get();
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

void ModelRenderSystem::RenderCascadeShadowMaps(RenderContext& rc, const DirectX::XMFLOAT3& lightDirection)
{
	if (!rc.cascadeShadowMap || !rc.camera) return;

	ID3D11DeviceContext* dc = rc.deviceContext;
	rc.cascadeShadowMap->UpdateMatrices(*rc.camera, lightDirection);
	rc.cascadeShadowMap->SetViewport(dc);

	ID3D11ShaderResourceView* nullShadowSRVs[CascadeShadowMap::CascadeCount] = {};
	dc->PSSetShaderResources(13, CascadeShadowMap::CascadeCount, nullShadowSRVs);

	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

	for (int cascadeIndex = 0; cascadeIndex < CascadeShadowMap::CascadeCount; ++cascadeIndex)
	{
		ID3D11DepthStencilView* dsv = rc.cascadeShadowMap->GetDSV(cascadeIndex);
		dc->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		dc->OMSetRenderTargets(0, nullptr, dsv);
		RenderShadowCasters(rc, cascadeIndex);
	}

	GraphicsDeviceManager::Instance().SetSceneRenderTarget();
}

void ModelRenderSystem::RenderShadowCasters(const RenderContext& rc, int cascadeIndex)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	ModelShader* shader = shaders[static_cast<int>(ModelShaderID::ShadowCaster)].get();
	if (!shader || !rc.cascadeShadowMap) return;

	CbShadowCaster shadowCaster{};
	shadowCaster.lightViewProjection = rc.cascadeShadowMap->GetLightViewProjection(cascadeIndex);
	dc->UpdateSubresource(shadowCasterConstantBuffer.Get(), 0, nullptr, &shadowCaster, 0, 0);
	dc->VSSetConstantBuffers(8, 1, shadowCasterConstantBuffer.GetAddressOf());

	ID3D11Buffer* vsConstantBuffers[] = { skeletonConstantBuffer.Get() };
	dc->VSSetConstantBuffers(6, _countof(vsConstantBuffers), vsConstantBuffers);

	shader->Begin(rc);
	for (const DrawInfo& drawInfo : drawInfos)
	{
		for (const GltfModelCommponennt::Mesh& mesh : drawInfo.model->GetMeshes())
		{
			if (mesh.material && mesh.material->alphaMode == GltfModelCommponennt::AlphaMode::Blend)
			{
				continue;
			}

			UINT stride = sizeof(GltfModelCommponennt::Vertex);
			UINT offset = 0;
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			CbSkeleton cbSkeleton{};
			if (!mesh.bones.empty())
			{
				for (size_t i = 0; i < mesh.bones.size(); ++i)
				{
					const GltfModelCommponennt::Bone& bone = mesh.bones.at(i);
					DirectX::XMMATRIX worldTransform = DirectX::XMLoadFloat4x4(&bone.node->worldTransform);
					DirectX::XMMATRIX offsetTransform = DirectX::XMLoadFloat4x4(&bone.offsetTransform);
					DirectX::XMStoreFloat4x4(&cbSkeleton.boneTransforms[i], offsetTransform * worldTransform);
				}
			}
			else
			{
				cbSkeleton.boneTransforms[0] = mesh.node->worldTransform;
			}
			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &cbSkeleton, 0, 0);
			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		}
	}
	shader->End(rc);

	ID3D11Buffer* nullBuffer = nullptr;
	dc->VSSetConstantBuffers(8, 1, &nullBuffer);
}
