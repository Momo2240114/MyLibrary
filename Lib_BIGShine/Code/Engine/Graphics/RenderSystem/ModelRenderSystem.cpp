#include "PrecompiledHeader.h"

#include "ModelRenderSystem.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Graphics/Shader/PBRShader.h"
#include "Engine/Graphics/Shader/ToonPBRShader.h"
#include "Engine/Graphics/Shader/ToonShader.h"
#include "Engine/Graphics/Shader/ShadowCasterShader.h"
#include "ShapeRenderer.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/Graphics/VFX/MaterialOverlayComponent.h"
#include "Engine/Debug/TracyProfiler.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <nlohmann/json.hpp>

namespace
{
	enum class RENDER_PROFILE_DRAW_KIND
	{
		DRAW_KIND_OPAQUE,
		DRAW_KIND_ALPHA_TEST,
		DRAW_KIND_TRANSPARENT,
		DRAW_KIND_SHADOW
	};

	std::string ToLowerAscii(std::string text_)
	{
		for (char& character_ : text_)
		{
			character_ = static_cast<char>(std::tolower(static_cast<unsigned char>(character_)));
		}
		return text_;
	}

	bool ContainsAny(const std::string& text_, std::initializer_list<const char*> keywords_)
	{
		for (const char* keyword_ : keywords_)
		{
			if (text_.find(keyword_) != std::string::npos)
			{
				return true;
			}
		}
		return false;
	}

	const char* GetRenderProfileCategoryName(ModelRenderSystem::RENDER_PROFILE_CATEGORY category_)
	{
		switch (category_)
		{
		case ModelRenderSystem::RENDER_PROFILE_CATEGORY::STAGE:
			return "Stage";
		case ModelRenderSystem::RENDER_PROFILE_CATEGORY::BARRIER:
			return "Barrier";
		case ModelRenderSystem::RENDER_PROFILE_CATEGORY::DRAGON:
			return "Dragon";
		case ModelRenderSystem::RENDER_PROFILE_CATEGORY::PLAYER:
			return "Player";
		case ModelRenderSystem::RENDER_PROFILE_CATEGORY::WEAPON:
			return "Weapon";
		default:
			return "OtherModels";
		}
	}

	void PlotRenderProfileStatistics(const char* prefix_, const ModelRenderSystem::RenderProfileStatistics& statistics_)
	{
		char plotName_[128]{};

		sprintf_s(plotName_, "%s.DrawCallCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.drawCallCount));
		sprintf_s(plotName_, "%s.DrawIndexedCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.drawIndexedCount));
		sprintf_s(plotName_, "%s.TriangleCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.triangleCount));
		sprintf_s(plotName_, "%s.IndexCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.indexCount));
		sprintf_s(plotName_, "%s.VertexCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.vertexCount));
		sprintf_s(plotName_, "%s.VisibleModelCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.visibleModelCount));
		sprintf_s(plotName_, "%s.SkippedModelCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.skippedModelCount));
		sprintf_s(plotName_, "%s.VisibleMeshCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.visibleMeshCount));
		sprintf_s(plotName_, "%s.SkippedMeshCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.skippedMeshCount));
		sprintf_s(plotName_, "%s.StaticModelCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.staticModelCount));
		sprintf_s(plotName_, "%s.SkinnedModelCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.skinnedModelCount));
		sprintf_s(plotName_, "%s.StaticDrawCallCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.staticDrawCallCount));
		sprintf_s(plotName_, "%s.SkinnedDrawCallCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.skinnedDrawCallCount));
		sprintf_s(plotName_, "%s.StaticTriangleCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.staticTriangleCount));
		sprintf_s(plotName_, "%s.SkinnedTriangleCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.skinnedTriangleCount));
		sprintf_s(plotName_, "%s.OpaqueDrawCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.opaqueDrawCount));
		sprintf_s(plotName_, "%s.AlphaTestDrawCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.alphaTestDrawCount));
		sprintf_s(plotName_, "%s.TransparentDrawCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.transparentDrawCount));
		sprintf_s(plotName_, "%s.MaterialBindCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.materialBindCount));
		sprintf_s(plotName_, "%s.TextureBindCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.textureBindCount));
		sprintf_s(plotName_, "%s.ShaderBindCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shaderBindCount));
		sprintf_s(plotName_, "%s.ShadowDrawCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shadowDrawCount));
		sprintf_s(plotName_, "%s.ShadowTriangleCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shadowTriangleCount));
		sprintf_s(plotName_, "%s.ShadowStaticDrawCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shadowStaticDrawCount));
		sprintf_s(plotName_, "%s.ShadowSkinnedDrawCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shadowSkinnedDrawCount));
		sprintf_s(plotName_, "%s.ShadowStaticTriangleCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shadowStaticTriangleCount));
		sprintf_s(plotName_, "%s.ShadowSkinnedTriangleCount", prefix_);
		BS_PROFILE_PLOT(plotName_, static_cast<int64_t>(statistics_.shadowSkinnedTriangleCount));
	}

	ModelRenderSystem::RENDER_PROFILE_CATEGORY ResolveRenderProfileCategory(
		const std::shared_ptr<GltfModelCommponennt>& model_)
	{
		if (!model_)
		{
			return ModelRenderSystem::RENDER_PROFILE_CATEGORY::OTHER;
		}

		std::string categoryText_ = ToLowerAscii(model_->GetModelName());
		const std::shared_ptr<GameObject> owner_ = model_->GetGameObject();
		if (owner_)
		{
			categoryText_ += " ";
			categoryText_ += ToLowerAscii(owner_->GetName());

			if (owner_->GetLayer() == LayerType::Player)
			{
				return ModelRenderSystem::RENDER_PROFILE_CATEGORY::PLAYER;
			}
			if (owner_->GetLayer() == LayerType::Environment ||
				owner_->GetLayer() == LayerType::Terrain)
			{
				return ModelRenderSystem::RENDER_PROFILE_CATEGORY::STAGE;
			}
		}

		if (ContainsAny(categoryText_, { "stage", "terrain", "ground", "floor", "map" }))
		{
			return ModelRenderSystem::RENDER_PROFILE_CATEGORY::STAGE;
		}
		if (ContainsAny(categoryText_, { "barrier", "shield" }))
		{
			return ModelRenderSystem::RENDER_PROFILE_CATEGORY::BARRIER;
		}
		if (ContainsAny(categoryText_, { "dragon", "enemy" }))
		{
			return ModelRenderSystem::RENDER_PROFILE_CATEGORY::DRAGON;
		}
		if (ContainsAny(categoryText_, { "player", "hero" }))
		{
			return ModelRenderSystem::RENDER_PROFILE_CATEGORY::PLAYER;
		}
		if (ContainsAny(categoryText_, { "weapon", "sword", "blade" }))
		{
			return ModelRenderSystem::RENDER_PROFILE_CATEGORY::WEAPON;
		}
		return ModelRenderSystem::RENDER_PROFILE_CATEGORY::OTHER;
	}
}

//---
//  モデル描画に必要なバッファとシェーダーを初期化する
//  Direct3D11 デバイス
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
		sizeof(CbStaticMesh),
		staticMeshConstantBuffer.GetAddressOf());

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
		"Shader/ToonPBRStaticVS.cso",
		GltfModelCommponennt::InputElementDescs.data(),
		static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
		gBufferInputLayout.GetAddressOf(),
		gBufferStaticVertexShader.GetAddressOf());

	CPUResourceLoader::LoadVertexShader(
		device,
		"Shader/ToonPBRSkinnedVS.cso",
		GltfModelCommponennt::InputElementDescs.data(),
		static_cast<UINT>(GltfModelCommponennt::InputElementDescs.size()),
		nullptr,
		gBufferSkinnedVertexShader.GetAddressOf());

	CPUResourceLoader::LoadPixelShader(
		device,
		"Shader/GBufferPS.cso",
		gBufferPixelShader.GetAddressOf());

	CPUResourceLoader::LoadPixelShader(
		device,
		"Shader/GpuDebugSimpleGBufferPS.cso",
		gBufferSimplePixelShader.GetAddressOf());

	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbGBufferMaterial),
		gBufferMaterialConstantBuffer.GetAddressOf());

	CPUResourceLoader::CreateConstantBuffer(
		device,
		sizeof(CbMaterialOverlay),
		materialOverlayConstantBuffer.GetAddressOf());

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
		{"texelSnapEnabled", cascadeShadowMap_.IsTexelSnapEnabled()},
		{"lightDistance", cascadeShadowMap_.GetLightDistance()},
		{"cascadeXyPadding", cascadeShadowMap_.GetCascadeXyPadding()},
		{"cascadeDepthPadding", cascadeShadowMap_.GetCascadeDepthPadding()},
		{"cascadeSplitDistances", {splits_[0], splits_[1], splits_[2], splits_[3]}},
		{"shadowAttenuation", shadowAttenuation},
		{"useCascadeShadowSoftness", useCascadeShadowSoftness},
		{"useCascadeShadowRandomRotation", useCascadeShadowRandomRotation},
		{"cascadeShadowSoftnessRadius", cascadeShadowSoftnessRadius},
		{"cascadeShadowSoftnessSampleCount", cascadeShadowSoftnessSampleCount},
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
			toonShader_->SetShadeSettings(shadeSteps_, shadeFeathers_);
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
		cascadeShadowMap_.SetTexelSnapEnabled(shadowJson_.value("texelSnapEnabled", cascadeShadowMap_.IsTexelSnapEnabled()));
		cascadeShadowMap_.SetLightDistance(shadowJson_.value("lightDistance", cascadeShadowMap_.GetLightDistance()));
		cascadeShadowMap_.SetCascadeXyPadding(shadowJson_.value("cascadeXyPadding", cascadeShadowMap_.GetCascadeXyPadding()));
		cascadeShadowMap_.SetCascadeDepthPadding(shadowJson_.value("cascadeDepthPadding", cascadeShadowMap_.GetCascadeDepthPadding()));

		if (shadowJson_.contains("cascadeSplitDistances") && shadowJson_["cascadeSplitDistances"].is_array())
		{
			const auto& splitsJson_ = shadowJson_["cascadeSplitDistances"];
			for (int i_ = 0; i_ < CascadeShadowMap::CascadeCount && i_ < static_cast<int>(splitsJson_.size()); ++i_)
			{
				cascadeShadowMap_.SetCascadeSplitDistance(i_, splitsJson_[i_].get<float>());
			}
		}

		shadowAttenuation = shadowJson_.value("shadowAttenuation", shadowAttenuation);
		useCascadeShadowSoftness = shadowJson_.value("useCascadeShadowSoftness", useCascadeShadowSoftness);
		useCascadeShadowRandomRotation = shadowJson_.value("useCascadeShadowRandomRotation", useCascadeShadowRandomRotation);
		cascadeShadowSoftnessRadius = shadowJson_.value("cascadeShadowSoftnessRadius", cascadeShadowSoftnessRadius);
		cascadeShadowSoftnessSampleCount = shadowJson_.value("cascadeShadowSoftnessSampleCount", cascadeShadowSoftnessSampleCount);

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

void ModelRenderSystem::RenderInvestigationSettingGui()
{
	auto applyDefaultSettings_ = [&]()
		{
			renderInvestigationSettings = {};
			renderInvestigationSettings.enabled = true;
			renderInvestigationSettings.enableDeferredPass = true;
			renderInvestigationSettings.enableShadowPass = true;
			renderInvestigationSettings.renderStaticModels = true;
			renderInvestigationSettings.renderSkinnedModels = true;
			renderInvestigationSettings.renderShadowStaticModels = true;
			renderInvestigationSettings.renderShadowSkinnedModels = true;
			renderInvestigationSettings.enableRenderStatistics = true;
		};

	auto drawPresetButton_ = [&](
		const char* label_,
		bool enableDeferredPass_,
		bool enableShadowPass_,
		bool renderStaticModels_,
		bool renderSkinnedModels_,
		bool renderShadowStaticModels_,
		bool renderShadowSkinnedModels_)
		{
			if (ImGui::Button(label_))
			{
				applyDefaultSettings_();
				renderInvestigationSettings.enableDeferredPass = enableDeferredPass_;
				renderInvestigationSettings.enableShadowPass = enableShadowPass_;
				renderInvestigationSettings.renderStaticModels = renderStaticModels_;
				renderInvestigationSettings.renderSkinnedModels = renderSkinnedModels_;
				renderInvestigationSettings.renderShadowStaticModels = renderShadowStaticModels_;
				renderInvestigationSettings.renderShadowSkinnedModels = renderShadowSkinnedModels_;
			}
		};

	ImGui::Checkbox("Enable Investigation Mode", &renderInvestigationSettings.enabled);
	ImGui::Separator();

	ImGui::BeginDisabled(!renderInvestigationSettings.enabled);
	ImGui::Checkbox("Use Simple GBuffer Pixel Shader", &renderInvestigationSettings.useSimpleGBufferPixelShader);
	ImGui::Checkbox("Use Depth Only Main Pass", &renderInvestigationSettings.useDepthOnlyMainPass);
	ImGui::Checkbox("Enable Deferred Lighting Pass", &renderInvestigationSettings.enableDeferredPass);
	ImGui::Checkbox("Enable Shadow Pass", &renderInvestigationSettings.enableShadowPass);
	ImGui::Separator();
	ImGui::Checkbox("Main Static Models", &renderInvestigationSettings.renderStaticModels);
	ImGui::Checkbox("Main Skinned Models", &renderInvestigationSettings.renderSkinnedModels);
	ImGui::Checkbox("Shadow Static Models", &renderInvestigationSettings.renderShadowStaticModels);
	ImGui::Checkbox("Shadow Skinned Models", &renderInvestigationSettings.renderShadowSkinnedModels);
	ImGui::Checkbox("Render Statistics", &renderInvestigationSettings.enableRenderStatistics);
	ImGui::Separator();
	if (ImGui::CollapsingHeader("Capture Presets", ImGuiTreeNodeFlags_DefaultOpen))
	{
		drawPresetButton_("01 Baseline", true, true, true, true, true, true);
		ImGui::SameLine();
		drawPresetButton_("02 No Shadow Pass", true, false, true, true, true, true);
		ImGui::SameLine();
		drawPresetButton_("03 No Deferred Lighting", false, true, true, true, true, true);
		drawPresetButton_("04 Main Static OFF", true, true, false, true, true, true);
		ImGui::SameLine();
		drawPresetButton_("05 Shadow Static OFF", true, true, true, true, false, true);
		ImGui::SameLine();
		drawPresetButton_("06 Main Skinned OFF", true, true, true, false, true, true);
		ImGui::SameLine();
		drawPresetButton_("07 Shadow Skinned OFF", true, true, true, true, true, false);
		ImGui::TextUnformatted("Send Tracy screenshots in preset order 01 to 07 with the same camera.");
	}
	ImGui::Separator();
	if (ImGui::Button("Reset Investigation Settings"))
	{
		renderInvestigationSettings = {};
	}
	ImGui::EndDisabled();
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

		bool texelSnapEnabled_ = cascadeShadowMap.IsTexelSnapEnabled();
		if (ImGui::Checkbox("Texel Snap", &texelSnapEnabled_))
		{
			cascadeShadowMap.SetTexelSnapEnabled(texelSnapEnabled_);
		}

		float* splits = cascadeShadowMap.GetCascadeSplitDistances();

		ImGui::DragFloat("Cascade 0 Far", &splits[0], 0.1f, 0.1f, 5000.0f);
		ImGui::DragFloat("Cascade 1 Far", &splits[1], 0.1f, splits[0] + 0.1f, 5000.0f);
		ImGui::DragFloat("Cascade 2 Far", &splits[2], 0.1f, splits[1] + 0.1f, 5000.0f);
		ImGui::DragFloat("Cascade 3 Far", &splits[3], 0.1f, splits[2] + 0.1f, 5000.0f);

		ImGui::Separator();

		ImGui::Text("Cascade Fit");
		float lightDistance_ = cascadeShadowMap.GetLightDistance();
		if (ImGui::DragFloat("Light Distance", &lightDistance_, 1.0f, 1.0f, 5000.0f))
		{
			cascadeShadowMap.SetLightDistance(lightDistance_);
		}

		float cascadeXyPadding_ = cascadeShadowMap.GetCascadeXyPadding();
		if (ImGui::DragFloat("XY Padding", &cascadeXyPadding_, 0.1f, 0.0f, 500.0f))
		{
			cascadeShadowMap.SetCascadeXyPadding(cascadeXyPadding_);
		}

		float cascadeDepthPadding_ = cascadeShadowMap.GetCascadeDepthPadding();
		if (ImGui::DragFloat("Depth Padding", &cascadeDepthPadding_, 1.0f, 0.0f, 5000.0f))
		{
			cascadeShadowMap.SetCascadeDepthPadding(cascadeDepthPadding_);
		}


		ImGui::Separator();

		ImGui::Text("Shadow Strength");
		ImGui::SliderFloat("Shadow Attenuation", &shadowAttenuation, 0.0f, 1.0f);

		ImGui::Separator();

		ImGui::Text("Shadow Softness");
		ImGui::Checkbox("Use Soft Shadow", &useCascadeShadowSoftness);
		ImGui::Checkbox("Random Rotation", &useCascadeShadowRandomRotation);
		ImGui::DragFloat("Poisson Radius", &cascadeShadowSoftnessRadius, 0.05f, 0.0f, 16.0f, "%.2f");
		ImGui::SliderInt("Poisson Samples", &cascadeShadowSoftnessSampleCount, 1, 16);

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
// 描画予定のモデルを描画キューに登録する
// shaderId 使用するシェーダーID
// model 描画対象のモデルコンポーネント
//---
void ModelRenderSystem::PushSubmit(ModelShaderID shaderId, std::shared_ptr<GltfModelCommponennt> model)
{
	DrawInfo& drawInfo = drawInfos.emplace_back();
	drawInfo.shaderId = shaderId;
	drawInfo.model = model;
	const std::shared_ptr<GameObject> owner_ = model ? model->GetGameObject() : nullptr;
	drawInfo.materialOverlay = owner_ ? owner_->GetComponent<MaterialOverlayComponent>() : nullptr;
}

// 描画実行
//---
// 登録されたモデルを全て描画する（不透明 → 半透明の順に描画）
// rc 描画コンテキスト（カメラ, ライト, ステートを含む）
//---
void ModelRenderSystem::RenderAll(const RenderContext& rc)
{
	BS_PROFILE_SCOPE("Render::Model");
	ID3D11DeviceContext* dc = rc.deviceContext;
	auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
	BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::RenderModels");
	renderProfileFrame = {};
	const bool investigationEnabled_ = renderInvestigationSettings.enabled;
	const bool depthOnlyMainPass_ =
		investigationEnabled_ && renderInvestigationSettings.useDepthOnlyMainPass;

	// シーン用定数バッファ更新
	{
		static LightManager defaultLightManager;
		const LightManager* lightManager = rc.lights ? rc.lights : &defaultLightManager;

		CbScene cbScene{};
		DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4(&rc.camera->GetView());
		DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&rc.camera->GetProjection());
		DirectX::XMMATRIX VP = V * P;

		DirectX::XMStoreFloat4x4(&cbScene.viewProjection, VP);
		// デカール座標復元用の逆行列を計算
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

		if (rc.cascadeShadowMap &&
			rc.camera &&
			rc.cascadeShadowMap->IsUseCascadeShadow() &&
			(!investigationEnabled_ || renderInvestigationSettings.enableShadowPass))
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
	ID3D11Buffer* psConstantBuffers[] =
	{
		sceneConstantBuffer.Get(),
	};
	dc->VSSetConstantBuffers(7, 1, sceneConstantBuffer.GetAddressOf());
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
		shadowData.cascadeShadowSoftness =
		{
			cascadeShadowSoftnessRadius,
			static_cast<float>(cascadeShadowSoftnessSampleCount),
			useCascadeShadowRandomRotation ? 1.0f : 0.0f,
			useCascadeShadowSoftness ? 1.0f : 0.0f
		};
		if (investigationEnabled_ && !renderInvestigationSettings.enableShadowPass)
		{
			shadowData.shadowAttenuation = 1.0f;
		}

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
	size_t modelSubmitCount_ = drawInfos.size();
	size_t meshSubmitCount_ = 0;
	std::vector<RENDER_PROFILE_CATEGORY> drawInfoCategories_;
	drawInfoCategories_.reserve(drawInfos.size());
	for (const DrawInfo& drawInfo_ : drawInfos)
	{
		const RENDER_PROFILE_CATEGORY category_ = ResolveRenderProfileCategory(drawInfo_.model);
		drawInfoCategories_.push_back(category_);
		if (!drawInfo_.model) continue;
		meshSubmitCount_ += drawInfo_.model->GetMeshes().size();

		++renderProfileFrame.total.visibleModelCount;
		++renderProfileFrame.categories[static_cast<int>(category_)].visibleModelCount;
		bool hasSkinningMesh_ = false;
		for (const GltfModelCommponennt::Mesh& mesh_ : drawInfo_.model->GetMeshes())
		{
			if (!mesh_.bones.empty())
			{
				hasSkinningMesh_ = true;
				break;
			}
		}
		if (hasSkinningMesh_)
		{
			++renderProfileFrame.total.skinnedModelCount;
			++renderProfileFrame.categories[static_cast<int>(category_)].skinnedModelCount;
		}
		else
		{
			++renderProfileFrame.total.staticModelCount;
			++renderProfileFrame.categories[static_cast<int>(category_)].staticModelCount;
		}
	}
	size_t skeletonUploadCount_ = 0;
	const GltfModelCommponennt::Material* previousMaterial_ = nullptr;
	std::array<ID3D11ShaderResourceView*, 5> previousTextureBindings_{};
	bool hasPreviousTextureBindings_ = false;
	ModelShaderID previousShaderId_ = ModelShaderID::EnumCount;
	bool hasPreviousShaderId_ = false;

	auto addRenderProfileDraw_ = [&](
		RENDER_PROFILE_CATEGORY category_,
		const GltfModelCommponennt::Mesh& mesh_,
		RENDER_PROFILE_DRAW_KIND drawKind_,
		ModelShaderID shaderId_,
		bool isSkinnedMesh_)
		{
			RenderProfileStatistics& totalStatistics_ = renderProfileFrame.total;
			RenderProfileStatistics& categoryStatistics_ =
				renderProfileFrame.categories[static_cast<int>(category_)];

			auto addToStatistics_ = [&](RenderProfileStatistics& statistics_)
				{
					++statistics_.drawCallCount;
					++statistics_.drawIndexedCount;
					statistics_.triangleCount += mesh_.indices.size() / 3;
					statistics_.indexCount += mesh_.indices.size();
					statistics_.vertexCount += mesh_.vertices.size();
					++statistics_.visibleMeshCount;
					if (isSkinnedMesh_)
					{
						++statistics_.skinnedDrawCallCount;
						statistics_.skinnedTriangleCount += mesh_.indices.size() / 3;
					}
					else
					{
						++statistics_.staticDrawCallCount;
						statistics_.staticTriangleCount += mesh_.indices.size() / 3;
					}
					switch (drawKind_)
					{
					case RENDER_PROFILE_DRAW_KIND::DRAW_KIND_ALPHA_TEST:
						++statistics_.alphaTestDrawCount;
						break;
					case RENDER_PROFILE_DRAW_KIND::DRAW_KIND_TRANSPARENT:
						++statistics_.transparentDrawCount;
						break;
					case RENDER_PROFILE_DRAW_KIND::DRAW_KIND_SHADOW:
						++statistics_.shadowDrawCount;
						statistics_.shadowTriangleCount += mesh_.indices.size() / 3;
						break;
					default:
						++statistics_.opaqueDrawCount;
						break;
					}
				};

			addToStatistics_(totalStatistics_);
			addToStatistics_(categoryStatistics_);

			if (mesh_.material != previousMaterial_)
			{
				++totalStatistics_.materialBindCount;
				++categoryStatistics_.materialBindCount;
				previousMaterial_ = mesh_.material;
			}

			std::array<ID3D11ShaderResourceView*, 5> textureBindings_ =
			{
				mesh_.material ? mesh_.material->baseMap.Get() : nullptr,
				mesh_.material ? mesh_.material->normalMap.Get() : nullptr,
				mesh_.material ? mesh_.material->emissiveMap.Get() : nullptr,
				mesh_.material ? mesh_.material->occlusionMap.Get() : nullptr,
				mesh_.material ? mesh_.material->metalnessRoughnessMap.Get() : nullptr,
			};
			if (!hasPreviousTextureBindings_ || textureBindings_ != previousTextureBindings_)
			{
				++totalStatistics_.textureBindCount;
				++categoryStatistics_.textureBindCount;
				previousTextureBindings_ = textureBindings_;
				hasPreviousTextureBindings_ = true;
			}

			if (!hasPreviousShaderId_ || shaderId_ != previousShaderId_)
			{
				++totalStatistics_.shaderBindCount;
				++categoryStatistics_.shaderBindCount;
				previousShaderId_ = shaderId_;
				hasPreviousShaderId_ = true;
			}
		};

	auto uploadSkeletonConstants_ = [&](GltfModelCommponennt& model_, int meshIndex_)
		{
			BS_PROFILE_SCOPE("Render::UploadSkeletonConstants");
			++skeletonUploadCount_;

			CbSkeleton cbSkeleton_{};
			const std::vector<DirectX::XMFLOAT4X4>& transforms_ =
				model_.GetSkinningTransforms(meshIndex_);
			const size_t transformCount_ = (std::min)(transforms_.size(), static_cast<size_t>(256));
			if (transformCount_ > 0)
			{
				std::memcpy(
					cbSkeleton_.boneTransforms,
					transforms_.data(),
					sizeof(DirectX::XMFLOAT4X4) * transformCount_);
			}
			else
			{
				DirectX::XMStoreFloat4x4(
					&cbSkeleton_.boneTransforms[0],
					DirectX::XMMatrixIdentity());
			}

			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &cbSkeleton_, 0, 0);
			++renderProfileFrame.total.skeletonBufferUpdateCount;
			renderProfileFrame.total.skeletonMatrixUploadCount += transformCount_;
		};

	auto isSkinnedMesh_ = [](
		const GltfModelCommponennt& model_,
		const GltfModelCommponennt::Mesh& mesh_)
		{
			return !model_.IsStaticModel() && !mesh_.bones.empty();
		};

	auto updateStaticMeshConstants_ = [&](const GltfModelCommponennt::Mesh& mesh_)
		{
			CbStaticMesh cbStaticMesh_{};
			if (mesh_.node)
			{
				cbStaticMesh_.world = mesh_.node->worldTransform;
				const DirectX::XMMATRIX worldMatrix_ =
					DirectX::XMLoadFloat4x4(&mesh_.node->worldTransform);
				const DirectX::XMMATRIX normalTransform_ =
					DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, worldMatrix_));
				DirectX::XMStoreFloat4x4(&cbStaticMesh_.normalTransform, normalTransform_);
			}
			else
			{
				DirectX::XMStoreFloat4x4(&cbStaticMesh_.world, DirectX::XMMatrixIdentity());
				DirectX::XMStoreFloat4x4(&cbStaticMesh_.normalTransform, DirectX::XMMatrixIdentity());
			}
			dc->UpdateSubresource(staticMeshConstantBuffer.Get(), 0, nullptr, &cbStaticMesh_, 0, 0);
		};

	auto bindMeshVertexConstants_ = [&](
		GltfModelCommponennt& model_,
		const GltfModelCommponennt::Mesh& mesh_,
		int meshIndex_)
		{
			ID3D11Buffer* nullBuffer_ = nullptr;
			if (isSkinnedMesh_(model_, mesh_))
			{
				uploadSkeletonConstants_(model_, meshIndex_);
				dc->VSSetConstantBuffers(5, 1, &nullBuffer_);
				dc->VSSetConstantBuffers(6, 1, skeletonConstantBuffer.GetAddressOf());
				return true;
			}

			updateStaticMeshConstants_(mesh_);
			dc->VSSetConstantBuffers(5, 1, staticMeshConstantBuffer.GetAddressOf());
			dc->VSSetConstantBuffers(6, 1, &nullBuffer_);
			return false;
		};

	auto shouldRenderInvestigationMesh_ = [&](
		GltfModelCommponennt& model_,
		const GltfModelCommponennt::Mesh& mesh_)
		{
			if (!investigationEnabled_)
			{
				return true;
			}

			const bool isSkinnedMeshResult_ = isSkinnedMesh_(model_, mesh_);
			if (isSkinnedMeshResult_ && !renderInvestigationSettings.renderSkinnedModels)
			{
				++renderProfileFrame.total.skippedMeshCount;
				return false;
			}
			if (!isSkinnedMeshResult_ && !renderInvestigationSettings.renderStaticModels)
			{
				++renderProfileFrame.total.skippedMeshCount;
				return false;
			}
			return true;
		};

	auto drawMesh = [&](GltfModelCommponennt& model_, const GltfModelCommponennt::Mesh& mesh, int meshIndex_, ModelShader* shader)
		{
			// 頂点バッファ設定
			UINT stride = sizeof(GltfModelCommponennt::Vertex);
			UINT offset = 0;
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// スケルトン用定数バッファ更新
			const bool isSkinnedMeshResult_ = bindMeshVertexConstants_(model_, mesh, meshIndex_);
			if (ToonPBRShader* toonPbrShader_ = dynamic_cast<ToonPBRShader*>(shader))
			{
				toonPbrShader_->BindVertexShader(rc, isSkinnedMeshResult_);
			}

			// 更新
			shader->Update(rc, mesh);

			ID3D11SamplerState* materialSampler_ = rc.renderState->GetSamplerState(
				mesh.material->uvAddressMode == GltfModelCommponennt::UV_ADDRESS_MODE::CLAMP
				? SamplerState::LinearClamp
				: SamplerState::LinearWrap);
			dc->PSSetSamplers(0, 1, &materialSampler_);

			// 描画
			dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
		};

	auto drawGBufferMesh = [&](GltfModelCommponennt& model_, const GltfModelCommponennt::Mesh& mesh, int meshIndex_,
		ModelShaderID shaderId_, const MaterialOverlayComponent* materialOverlay_)
		{
			UINT stride_ = sizeof(GltfModelCommponennt::Vertex);
			UINT offset_ = 0;
			dc->IASetInputLayout(gBufferInputLayout.Get());
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride_, &offset_);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			const bool isSkinnedMeshResult_ = bindMeshVertexConstants_(model_, mesh, meshIndex_);
			dc->VSSetShader(
				isSkinnedMeshResult_ ? gBufferSkinnedVertexShader.Get() : gBufferStaticVertexShader.Get(),
				nullptr,
				0);
			dc->PSSetShader(
				depthOnlyMainPass_
				? nullptr
				: (investigationEnabled_ && renderInvestigationSettings.useSimpleGBufferPixelShader
					? gBufferSimplePixelShader.Get()
					: gBufferPixelShader.Get()),
				nullptr,
				0);
			if (depthOnlyMainPass_)
			{
				if (isSkinnedMeshResult_)
				{
					BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Main::SkinnedModels");
					dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
				}
				else
				{
					BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Main::StaticModels");
					dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
				}
				return;
			}
			dc->PSSetConstantBuffers(0, 1, gBufferMaterialConstantBuffer.GetAddressOf());
			dc->PSSetConstantBuffers(1, 1, materialOverlayConstantBuffer.GetAddressOf());

			CbGBufferMaterial material_{};
			material_.baseColor = mesh.material->baseColor;
			material_.emissiveColor = mesh.material->emissiveColor;
			material_.metalness = mesh.material->metalness;
			material_.roughness = mesh.material->roughness;
			material_.occlusionStrength = mesh.material->occlusionStrength;
			material_.toonPbrBlend = mesh.material->toonPbrBlend;
			material_.shadingModel = static_cast<int>(shaderId_);
			material_.uvScale = mesh.material->uvScale;
			material_.uvOffset = mesh.material->uvOffset;
			material_.uvRotation = mesh.material->uvRotation;
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

			ID3D11SamplerState* materialSampler_ = rc.renderState->GetSamplerState(
				mesh.material->uvAddressMode == GltfModelCommponennt::UV_ADDRESS_MODE::CLAMP
				? SamplerState::LinearClamp
				: SamplerState::LinearWrap);
			dc->PSSetSamplers(0, 1, &materialSampler_);

			CbMaterialOverlay overlayConstants_{};
			ID3D11ShaderResourceView* overlaySrvs_[] =
			{
				whiteTextureSRV.Get(),
				whiteTextureSRV.Get(),
			};
			if (materialOverlay_ && materialOverlay_->IsEnabled() &&
				materialOverlay_->IsOverlayActive() &&
				!materialOverlay_->GetAsset().parts.empty() &&
				!materialOverlay_->GetRuntimeParts().empty())
			{
				const auto& overlayParts_ = materialOverlay_->GetAsset().parts;
				const auto& overlayRuntimeParts_ = materialOverlay_->GetRuntimeParts();
				const MeshVFXPart* selectedPart_ = nullptr;
				const MeshVFXRuntimePart* selectedRuntime_ = nullptr;

				auto selectPart_ = [&](bool exactMatch_) -> bool
					{
						const size_t partCount_ = (std::min)(overlayParts_.size(), overlayRuntimeParts_.size());
						for (size_t partIndex_ = 0; partIndex_ < partCount_; ++partIndex_)
						{
							const MeshVFXPart& part_ = overlayParts_[partIndex_];
							const bool matches_ = exactMatch_
								? part_.meshIndex == meshIndex_
								: part_.meshIndex < 0;
							if (!matches_ || !part_.renderEnabled) continue;

							selectedPart_ = &part_;
							selectedRuntime_ = &overlayRuntimeParts_[partIndex_];
							return true;
						}
						return false;
					};

				if (!selectPart_(true))
				{
					selectPart_(false);
				}
				if (selectedPart_ && selectedRuntime_)
				{
					overlayConstants_.color = selectedPart_->color;
					overlayConstants_.maskUVSpeed = selectedPart_->maskUVSpeed;
					overlayConstants_.noiseUVSpeed = selectedPart_->noiseUVSpeed;
					overlayConstants_.time = materialOverlay_->GetTime();
					overlayConstants_.alpha = selectedPart_->alpha * materialOverlay_->GetOverlayWeight();
					overlayConstants_.noiseStrength = selectedPart_->noiseStrength;
					overlayConstants_.dissolveThreshold = selectedPart_->dissolveThreshold;
					overlayConstants_.emissiveStrength = selectedPart_->emissiveStrength;
					overlayConstants_.emissivePulseSpeed = selectedPart_->emissivePulseSpeed;
					overlayConstants_.emissivePulseAmount = selectedPart_->emissivePulseAmount;
					const bool hasMask_ =
						!selectedRuntime_->maskTextures.empty() && selectedRuntime_->maskTextures.front();
					const bool hasNoise_ =
						!selectedRuntime_->noiseTextures.empty() && selectedRuntime_->noiseTextures.front();
					overlayConstants_.enabled = 1u;
					overlayConstants_.useMask = selectedPart_->useMask && hasMask_ ? 1u : 0u;
					overlayConstants_.useNoise = selectedPart_->useNoise && hasNoise_ ? 1u : 0u;
					if (hasMask_)
					{
						overlaySrvs_[0] = selectedRuntime_->maskTextures.front().Get();
					}
					if (hasNoise_)
					{
						overlaySrvs_[1] = selectedRuntime_->noiseTextures.front().Get();
					}
				}
			}
			dc->UpdateSubresource(materialOverlayConstantBuffer.Get(), 0, nullptr, &overlayConstants_, 0, 0);
			dc->PSSetShaderResources(9, _countof(overlaySrvs_), overlaySrvs_);
			if (isSkinnedMeshResult_)
			{
				BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Main::SkinnedModels");
				dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
			}
			else
			{
				BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Main::StaticModels");
				dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
			}
		};

	auto isOutlineTarget = [](ModelShaderID shaderId)
		{
			return shaderId == ModelShaderID::Toon || shaderId == ModelShaderID::ToonPBR;
		};

	auto drawOutlineMesh = [&](GltfModelCommponennt& model_, const GltfModelCommponennt::Mesh& mesh, int meshIndex_)
		{
			if (!mesh.material->disableOutline || mesh.material->outlineThicknessOffset < 0)
			{
				return;
			}

			CbOutline cbOutline{};
			cbOutline.outlineColor = outlineColor;
			cbOutline.outlineThickness = mesh.material->outlineThicknessOffset;
			dc->UpdateSubresource(outlineConstantBuffer.Get(), 0, nullptr, &cbOutline, 0, 0);

			uploadSkeletonConstants_(model_, meshIndex_);

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
			ID3D11SamplerState* outlineSamplers_[] =
			{
				rc.renderState->GetSamplerState(SamplerState::LinearWrap),
				rc.renderState->GetSamplerState(SamplerState::LinearBorder),
			};
			dc->PSSetSamplers(0, _countof(outlineSamplers_), outlineSamplers_);
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
	const bool useDeferredRendering_ = rc.cascadeShadowMap != nullptr || depthOnlyMainPass_;
	if (depthOnlyMainPass_)
	{
		dc->OMSetRenderTargets(0, nullptr, graphics_.GetDepthStencilView());
	}
	else if (useDeferredRendering_)
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
	{
		BS_PROFILE_SCOPE("RenderModels::OpaqueModels");
		BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Main::OpaqueModels");

		for (size_t drawInfoIndex_ = 0; drawInfoIndex_ < drawInfos.size(); ++drawInfoIndex_)
		{
			DrawInfo& drawInfo = drawInfos[drawInfoIndex_];
			if (!drawInfo.model) continue;

			const RENDER_PROFILE_CATEGORY category_ =
				drawInfoIndex_ < drawInfoCategories_.size()
				? drawInfoCategories_[drawInfoIndex_]
				: RENDER_PROFILE_CATEGORY::OTHER;
			const auto& meshes_ = drawInfo.model->GetMeshes();
			for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(meshes_.size()); ++meshIndex_)
			{
				const GltfModelCommponennt::Mesh& mesh = meshes_[meshIndex_];
				if (!mesh.material) continue;
				if (!shouldRenderInvestigationMesh_(*drawInfo.model, mesh)) continue;
				const bool isSkinnedMeshResult_ = isSkinnedMesh_(*drawInfo.model, mesh);

				// マテリアルからシェーダーIDを取得
				ModelShaderID currentShaderId = mesh.material->shaderId;
				ModelShader* shader = shaders[static_cast<int>(currentShaderId)].get();

				// 半透明メッシュ登録
				if (mesh.material->alphaMode == GltfModelCommponennt::AlphaMode::Blend ||
					(mesh.material->baseColor.w > 0.01f && mesh.material->baseColor.w < 0.99f))
				{
					TransparencyDrawInfo& transparencyDrawInfo = transparencyDrawInfos.emplace_back();
					transparencyDrawInfo.shaderId = currentShaderId;
					transparencyDrawInfo.model = drawInfo.model;
					transparencyDrawInfo.mesh = &mesh;
					transparencyDrawInfo.meshIndex = meshIndex_;
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

				const RENDER_PROFILE_DRAW_KIND drawKind_ =
					mesh.material->alphaMode == GltfModelCommponennt::AlphaMode::Mask
					? RENDER_PROFILE_DRAW_KIND::DRAW_KIND_ALPHA_TEST
					: RENDER_PROFILE_DRAW_KIND::DRAW_KIND_OPAQUE;
				addRenderProfileDraw_(category_, mesh, drawKind_, currentShaderId, isSkinnedMeshResult_);

				if (useDeferredRendering_)
				{
					drawGBufferMesh(*drawInfo.model, mesh, meshIndex_, currentShaderId, drawInfo.materialOverlay.get());
				}
				else
				{
					shader->Begin(rc);
					drawMesh(*drawInfo.model, mesh, meshIndex_, shader);
					if (isOutlineTarget(currentShaderId))
					{
						addRenderProfileDraw_(
							category_,
							mesh,
							RENDER_PROFILE_DRAW_KIND::DRAW_KIND_OPAQUE,
							currentShaderId,
							isSkinnedMeshResult_);
						drawOutlineMesh(*drawInfo.model, mesh, meshIndex_);
					}
					shader->End(rc);
				}
			}
		}
	}

	if (useDeferredRendering_ &&
		!depthOnlyMainPass_ &&
		(!investigationEnabled_ || renderInvestigationSettings.enableDeferredPass))
	{
		{
			BS_PROFILE_SCOPE("RenderModels::DeferredLighting");
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Main::DeferredLighting");
			ID3D11ShaderResourceView* nullMaterialSrvs_[5] = {};
			dc->PSSetShaderResources(0, _countof(nullMaterialSrvs_), nullMaterialSrvs_);
			ID3D11ShaderResourceView* nullOverlaySrvs_[2] = {};
			dc->PSSetShaderResources(9, _countof(nullOverlaySrvs_), nullOverlaySrvs_);
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
				cascadeShadowConstantBuffer.Get(),
				toonSteps_,
				toonFeathers_,
				toonPbrSteps_,
				toonPbrFeathers_);
		}

		graphics_.SetSceneRenderTarget();
		graphics_.RenderSkyBox(rc);

		{
			BS_PROFILE_SCOPE("RenderModels::OutlineModels");
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Main::OutlineModels");

			for (size_t drawInfoIndex_ = 0; drawInfoIndex_ < drawInfos.size(); ++drawInfoIndex_)
			{
				DrawInfo& drawInfo = drawInfos[drawInfoIndex_];
				if (!drawInfo.model) continue;

				const RENDER_PROFILE_CATEGORY category_ =
					drawInfoIndex_ < drawInfoCategories_.size()
					? drawInfoCategories_[drawInfoIndex_]
					: RENDER_PROFILE_CATEGORY::OTHER;
				const auto& meshes_ = drawInfo.model->GetMeshes();
				for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(meshes_.size()); ++meshIndex_)
				{
					const GltfModelCommponennt::Mesh& mesh = meshes_[meshIndex_];
					if (mesh.material &&
						mesh.material->alphaMode != GltfModelCommponennt::AlphaMode::Blend &&
						isOutlineTarget(mesh.material->shaderId) &&
						shouldRenderInvestigationMesh_(*drawInfo.model, mesh))
					{
						addRenderProfileDraw_(
							category_,
							mesh,
							RENDER_PROFILE_DRAW_KIND::DRAW_KIND_OPAQUE,
							mesh.material->shaderId,
							isSkinnedMesh_(*drawInfo.model, mesh));
						drawOutlineMesh(*drawInfo.model, mesh, meshIndex_);
					}
				}
			}
		}
	}
	else if (useDeferredRendering_ &&
		!depthOnlyMainPass_ &&
		investigationEnabled_ &&
		!renderInvestigationSettings.enableDeferredPass)
	{
		graphics_.SetSceneRenderTarget();
	}
	drawInfos.clear();


	// ★ デカール描画実行
	// RenderDecals 内では copyDepthStencilSRV (コピーされた方) を使うこと！
	if (!depthOnlyMainPass_)
	{
		RenderDecals(rc);
	}

	// カメラから遠い順にソート
	std::sort(transparencyDrawInfos.begin(), transparencyDrawInfos.end(),
		[](const TransparencyDrawInfo& lhs, const TransparencyDrawInfo& rhs)
		{
			return lhs.distance > rhs.distance;
	});

	// 半透明描画処理
	if (!depthOnlyMainPass_)
	{
		BS_PROFILE_SCOPE("RenderModels::TransparentModels");
		BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Main::TransparentModels");

		for (const TransparencyDrawInfo& transparencyDrawInfo : transparencyDrawInfos)
		{
			if (transparencyDrawInfo.model &&
				transparencyDrawInfo.mesh &&
				!shouldRenderInvestigationMesh_(*transparencyDrawInfo.model, *transparencyDrawInfo.mesh))
			{
				continue;
			}
			ModelShader* shader = shaders[static_cast<int>(transparencyDrawInfo.shaderId)].get();

			shader->Begin(rc);

			if (transparencyDrawInfo.model && transparencyDrawInfo.mesh)
			{
				const RENDER_PROFILE_CATEGORY category_ = ResolveRenderProfileCategory(transparencyDrawInfo.model);
				addRenderProfileDraw_(
					category_,
					*transparencyDrawInfo.mesh,
					RENDER_PROFILE_DRAW_KIND::DRAW_KIND_TRANSPARENT,
					transparencyDrawInfo.shaderId,
					isSkinnedMesh_(*transparencyDrawInfo.model, *transparencyDrawInfo.mesh));
				drawMesh(
					*transparencyDrawInfo.model,
					*transparencyDrawInfo.mesh,
					transparencyDrawInfo.meshIndex,
					shader);
			}

			shader->End(rc);
		}
	}
	BS_PROFILE_PLOT("Render.ModelSubmitCount", static_cast<int64_t>(modelSubmitCount_));
	BS_PROFILE_PLOT("Render.MeshSubmitCount", static_cast<int64_t>(meshSubmitCount_));
	BS_PROFILE_PLOT("Render.SkeletonUploadCount", static_cast<int64_t>(skeletonUploadCount_));
	BS_PROFILE_PLOT("Render.SkeletonBufferUpdateCount", static_cast<int64_t>(renderProfileFrame.total.skeletonBufferUpdateCount));
	BS_PROFILE_PLOT("Render.SkeletonMatrixUploadCount", static_cast<int64_t>(renderProfileFrame.total.skeletonMatrixUploadCount));
	BS_PROFILE_PLOT("Render.TransparentMeshCount", static_cast<int64_t>(transparencyDrawInfos.size()));
	BS_PROFILE_PLOT("Render.DepthOnlyEnabled", depthOnlyMainPass_ ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.SimplePixelShaderEnabled",
		investigationEnabled_ && renderInvestigationSettings.useSimpleGBufferPixelShader ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.DeferredPassEnabled",
		!investigationEnabled_ || renderInvestigationSettings.enableDeferredPass ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.ShadowPassEnabled",
		!investigationEnabled_ || renderInvestigationSettings.enableShadowPass ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.MainStaticModelsEnabled",
		!investigationEnabled_ || renderInvestigationSettings.renderStaticModels ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.MainSkinnedModelsEnabled",
		!investigationEnabled_ || renderInvestigationSettings.renderSkinnedModels ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.ShadowStaticModelsEnabled",
		!investigationEnabled_ || renderInvestigationSettings.renderShadowStaticModels ? 1 : 0);
	BS_PROFILE_PLOT(
		"Render.ShadowSkinnedModelsEnabled",
		!investigationEnabled_ || renderInvestigationSettings.renderShadowSkinnedModels ? 1 : 0);
	if (!investigationEnabled_ || renderInvestigationSettings.enableRenderStatistics)
	{
		PlotRenderProfileStatistics("Render", renderProfileFrame.total);
		for (int cascadeIndex_ = 0; cascadeIndex_ < CascadeShadowMap::CascadeCount; ++cascadeIndex_)
		{
			char plotPrefix_[128]{};
			sprintf_s(plotPrefix_, "Render.Shadow.Cascade%d", cascadeIndex_);
			PlotRenderProfileStatistics(plotPrefix_, renderProfileFrame.shadowCascades[cascadeIndex_]);
		}
		for (int categoryIndex_ = 0; categoryIndex_ < static_cast<int>(RENDER_PROFILE_CATEGORY::COUNT); ++categoryIndex_)
		{
			const RENDER_PROFILE_CATEGORY category_ = static_cast<RENDER_PROFILE_CATEGORY>(categoryIndex_);
			char plotPrefix_[128]{};
			sprintf_s(plotPrefix_, "Render.Category.%s", GetRenderProfileCategoryName(category_));
			PlotRenderProfileStatistics(plotPrefix_, renderProfileFrame.categories[categoryIndex_]);
		}
	}
	transparencyDrawInfos.clear();

	// 定数バッファ設定解除
	ID3D11Buffer* vsConstantBuffers[] = { nullptr, nullptr, nullptr };
	for (ID3D11Buffer*& psConstantBuffer : psConstantBuffers) { psConstantBuffer = nullptr; }
	dc->VSSetConstantBuffers(5, _countof(vsConstantBuffers), vsConstantBuffers);
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
		cb.decalEmissive = decal.emissiveColorAndIntensity;
		cb.decalUvScaleOffset = decal.uvScaleOffset;
		cb.decalDissolveParams = decal.dissolveParams;
		cb.decalDissolveEdgeColor = decal.dissolveEdgeColor;
		cb.decalBlendMode = decal.blendMode;
		cb.decalUseDissolve = decal.useDissolve;
		cb.decalPadding = { 0.0f, 0.0f };

		dc->UpdateSubresource(decalConstantBuffer.Get(), 0, nullptr, &cb, 0, 0);
		dc->VSSetConstantBuffers(10, 1, decalConstantBuffer.GetAddressOf());
		dc->PSSetConstantBuffers(10, 1, decalConstantBuffer.GetAddressOf());

		ID3D11ShaderResourceView* texSRV = decal.texture ? decal.texture.Get() : whiteTextureSRV.Get();
		ID3D11ShaderResourceView* maskSRV = decal.maskTexture ? decal.maskTexture.Get() : whiteTextureSRV.Get();
		ID3D11ShaderResourceView* noiseSRV = decal.noiseTexture ? decal.noiseTexture.Get() : whiteTextureSRV.Get();
		ID3D11ShaderResourceView* decalSRVs_[] = { texSRV, maskSRV, noiseSRV };
		dc->PSSetShaderResources(10, 3, decalSRVs_);

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
	auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
	BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow Pass");
	rc.cascadeShadowMap->UpdateMatrices(*rc.camera, lightDirection);
	rc.cascadeShadowMap->SetViewport(dc);

	ID3D11ShaderResourceView* nullShadowSRVs[CascadeShadowMap::CascadeCount] = {};
	dc->PSSetShaderResources(13, CascadeShadowMap::CascadeCount, nullShadowSRVs);

	dc->OMSetBlendState(rc.renderState->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
	dc->OMSetDepthStencilState(rc.renderState->GetDepthStencilState(DepthState::TestAndWrite), 0);
	dc->RSSetState(rc.renderState->GetRasterizerState(RasterizerState::SolidCullBack));

	auto renderCascade_ = [&](int cascadeIndex_)
		{
			ID3D11DepthStencilView* dsv_ = rc.cascadeShadowMap->GetDSV(cascadeIndex_);
			dc->ClearDepthStencilView(dsv_, D3D11_CLEAR_DEPTH, 1.0f, 0);
			dc->OMSetRenderTargets(0, nullptr, dsv_);
			RenderShadowCasters(rc, cascadeIndex_);
		};

	for (int cascadeIndex = 0; cascadeIndex < CascadeShadowMap::CascadeCount; ++cascadeIndex)
	{
		BS_PROFILE_SCOPE("RenderShadow::Cascade");
		switch (cascadeIndex)
		{
		case 0:
		{
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow::Cascade0");
			renderCascade_(cascadeIndex);
			break;
		}
		case 1:
		{
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow::Cascade1");
			renderCascade_(cascadeIndex);
			break;
		}
		case 2:
		{
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow::Cascade2");
			renderCascade_(cascadeIndex);
			break;
		}
		case 3:
		{
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow::Cascade3");
			renderCascade_(cascadeIndex);
			break;
		}
		default:
		{
			BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow::Cascade");
			renderCascade_(cascadeIndex);
			break;
		}
		}
	}

	GraphicsDeviceManager::Instance().SetSceneRenderTarget();
}

void ModelRenderSystem::RenderShadowCasters(const RenderContext& rc, int cascadeIndex)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	ModelShader* shader = shaders[static_cast<int>(ModelShaderID::ShadowCaster)].get();
	if (!shader || !rc.cascadeShadowMap) return;
	auto tracyGpuContext_ = GraphicsDeviceManager::Instance().GetTracyGpuContext();
	BS_PROFILE_SCOPE("RenderShadow::Casters");
	BS_PROFILE_GPU_SCOPE(tracyGpuContext_, "GPU::Shadow::Casters");
	const bool investigationEnabled_ = renderInvestigationSettings.enabled;

	CbShadowCaster shadowCaster{};
	shadowCaster.lightViewProjection = rc.cascadeShadowMap->GetLightViewProjection(cascadeIndex);
	dc->UpdateSubresource(shadowCasterConstantBuffer.Get(), 0, nullptr, &shadowCaster, 0, 0);
	dc->VSSetConstantBuffers(8, 1, shadowCasterConstantBuffer.GetAddressOf());

	auto uploadSkeletonConstants_ = [&](GltfModelCommponennt& model_, int meshIndex_)
		{
			BS_PROFILE_SCOPE("RenderShadow::UploadSkeletonConstants");

			CbSkeleton cbSkeleton_{};
			const std::vector<DirectX::XMFLOAT4X4>& transforms_ =
				model_.GetSkinningTransforms(meshIndex_);
			const size_t transformCount_ = (std::min)(transforms_.size(), static_cast<size_t>(256));
			if (transformCount_ > 0)
			{
				std::memcpy(
					cbSkeleton_.boneTransforms,
					transforms_.data(),
					sizeof(DirectX::XMFLOAT4X4) * transformCount_);
			}
			else
			{
				DirectX::XMStoreFloat4x4(
					&cbSkeleton_.boneTransforms[0],
					DirectX::XMMatrixIdentity());
			}

			dc->UpdateSubresource(skeletonConstantBuffer.Get(), 0, nullptr, &cbSkeleton_, 0, 0);
			++renderProfileFrame.total.skeletonBufferUpdateCount;
			renderProfileFrame.total.skeletonMatrixUploadCount += transformCount_;
		};

	auto isSkinnedMesh_ = [](
		const GltfModelCommponennt& model_,
		const GltfModelCommponennt::Mesh& mesh_)
		{
			return !model_.IsStaticModel() && !mesh_.bones.empty();
		};

	auto updateStaticMeshConstants_ = [&](const GltfModelCommponennt::Mesh& mesh_)
		{
			CbStaticMesh cbStaticMesh_{};
			if (mesh_.node)
			{
				cbStaticMesh_.world = mesh_.node->worldTransform;
				const DirectX::XMMATRIX worldMatrix_ =
					DirectX::XMLoadFloat4x4(&mesh_.node->worldTransform);
				const DirectX::XMMATRIX normalTransform_ =
					DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, worldMatrix_));
				DirectX::XMStoreFloat4x4(&cbStaticMesh_.normalTransform, normalTransform_);
			}
			else
			{
				DirectX::XMStoreFloat4x4(&cbStaticMesh_.world, DirectX::XMMatrixIdentity());
				DirectX::XMStoreFloat4x4(&cbStaticMesh_.normalTransform, DirectX::XMMatrixIdentity());
			}
			dc->UpdateSubresource(staticMeshConstantBuffer.Get(), 0, nullptr, &cbStaticMesh_, 0, 0);
		};

	auto bindShadowVertexConstants_ = [&](
		GltfModelCommponennt& model_,
		const GltfModelCommponennt::Mesh& mesh_,
		int meshIndex_)
		{
			ID3D11Buffer* nullBuffer_ = nullptr;
			if (isSkinnedMesh_(model_, mesh_))
			{
				uploadSkeletonConstants_(model_, meshIndex_);
				dc->VSSetConstantBuffers(5, 1, &nullBuffer_);
				dc->VSSetConstantBuffers(6, 1, skeletonConstantBuffer.GetAddressOf());
				return true;
			}

			updateStaticMeshConstants_(mesh_);
			dc->VSSetConstantBuffers(5, 1, staticMeshConstantBuffer.GetAddressOf());
			dc->VSSetConstantBuffers(6, 1, &nullBuffer_);
			return false;
		};

	shader->Begin(rc);
	for (const DrawInfo& drawInfo : drawInfos)
	{
		if (!drawInfo.model) continue;

		const auto& meshes_ = drawInfo.model->GetMeshes();
		for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(meshes_.size()); ++meshIndex_)
		{
			const GltfModelCommponennt::Mesh& mesh = meshes_[meshIndex_];
			if (mesh.material && mesh.material->alphaMode == GltfModelCommponennt::AlphaMode::Blend)
			{
				continue;
			}
			const bool isSkinnedMeshResult_ = isSkinnedMesh_(*drawInfo.model, mesh);
			if (investigationEnabled_ &&
				((isSkinnedMeshResult_ && !renderInvestigationSettings.renderShadowSkinnedModels) ||
					(!isSkinnedMeshResult_ && !renderInvestigationSettings.renderShadowStaticModels)))
			{
				++renderProfileFrame.total.skippedMeshCount;
				continue;
			}

			UINT stride = sizeof(GltfModelCommponennt::Vertex);
			UINT offset = 0;
			dc->IASetVertexBuffers(0, 1, mesh.vertexBuffer.GetAddressOf(), &stride, &offset);
			dc->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
			dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			const bool boundSkinnedMesh_ = bindShadowVertexConstants_(*drawInfo.model, mesh, meshIndex_);
			if (ShadowCasterShader* shadowCasterShader_ = dynamic_cast<ShadowCasterShader*>(shader))
			{
				shadowCasterShader_->BindVertexShader(rc, boundSkinnedMesh_);
			}
			const RENDER_PROFILE_CATEGORY category_ = ResolveRenderProfileCategory(drawInfo.model);
			RenderProfileStatistics& totalStatistics_ = renderProfileFrame.total;
			RenderProfileStatistics& categoryStatistics_ =
				renderProfileFrame.categories[static_cast<int>(category_)];
			RenderProfileStatistics& cascadeStatistics_ =
				renderProfileFrame.shadowCascades[cascadeIndex];
			++totalStatistics_.shadowDrawCount;
			totalStatistics_.shadowTriangleCount += mesh.indices.size() / 3;
			++categoryStatistics_.shadowDrawCount;
			categoryStatistics_.shadowTriangleCount += mesh.indices.size() / 3;
			++cascadeStatistics_.shadowDrawCount;
			cascadeStatistics_.shadowTriangleCount += mesh.indices.size() / 3;
			if (isSkinnedMeshResult_)
			{
				++totalStatistics_.shadowSkinnedDrawCount;
				totalStatistics_.shadowSkinnedTriangleCount += mesh.indices.size() / 3;
				++categoryStatistics_.shadowSkinnedDrawCount;
				categoryStatistics_.shadowSkinnedTriangleCount += mesh.indices.size() / 3;
				++cascadeStatistics_.shadowSkinnedDrawCount;
				cascadeStatistics_.shadowSkinnedTriangleCount += mesh.indices.size() / 3;
			}
			else
			{
				++totalStatistics_.shadowStaticDrawCount;
				totalStatistics_.shadowStaticTriangleCount += mesh.indices.size() / 3;
				++categoryStatistics_.shadowStaticDrawCount;
				categoryStatistics_.shadowStaticTriangleCount += mesh.indices.size() / 3;
				++cascadeStatistics_.shadowStaticDrawCount;
				cascadeStatistics_.shadowStaticTriangleCount += mesh.indices.size() / 3;
			}
			if (isSkinnedMeshResult_)
			{
				BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Shadow::SkinnedModels");
				dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
			}
			else
			{
				BS_PROFILE_GPU_DETAILED_SCOPE(tracyGpuContext_, "GPU::Shadow::StaticModels");
				dc->DrawIndexed(static_cast<UINT>(mesh.indices.size()), 0, 0);
			}
		}
	}
	shader->End(rc);

	ID3D11Buffer* nullBuffer = nullptr;
	dc->VSSetConstantBuffers(5, 1, &nullBuffer);
	dc->VSSetConstantBuffers(6, 1, &nullBuffer);
	dc->VSSetConstantBuffers(8, 1, &nullBuffer);
}
