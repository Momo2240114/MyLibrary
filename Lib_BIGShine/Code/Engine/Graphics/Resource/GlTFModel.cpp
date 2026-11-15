#include "PrecompiledHeader.h"

#include "GltfModel.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include <filesystem>
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "Engine/Common/Misc.h"
#include "GLTFImporter.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "ModelResourceManager.h"
#include "Model.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

#include "Engine/Platform/WindowsDialogHelper.h"

#include "Engine/Debug/TracyProfiler.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <utility>


const std::vector<D3D11_INPUT_ELEMENT_DESC> GltfModelCommponennt::InputElementDescs =
{
	{ "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

namespace DirectX
{
	template<class Archive>
	void serialize(Archive& archive, XMUINT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT2& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT3& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4& v)
	{
		archive(
			cereal::make_nvp("x", v.x),
			cereal::make_nvp("y", v.y),
			cereal::make_nvp("z", v.z),
			cereal::make_nvp("w", v.w)
		);
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4X4& m)
	{
		archive(
			cereal::make_nvp("_11", m._11),
			cereal::make_nvp("_12", m._12),
			cereal::make_nvp("_13", m._13),
			cereal::make_nvp("_14", m._14),
			cereal::make_nvp("_21", m._21),
			cereal::make_nvp("_22", m._22),
			cereal::make_nvp("_23", m._23),
			cereal::make_nvp("_24", m._24),
			cereal::make_nvp("_31", m._31),
			cereal::make_nvp("_32", m._32),
			cereal::make_nvp("_33", m._33),
			cereal::make_nvp("_34", m._34),
			cereal::make_nvp("_41", m._41),
			cereal::make_nvp("_42", m._42),
			cereal::make_nvp("_43", m._43),
			cereal::make_nvp("_44", m._44)
		);
	}
}

template<class Archive>
void GltfModelCommponennt::Node::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(parentIndex),
		CEREAL_NVP(position),
		CEREAL_NVP(rotation),
		CEREAL_NVP(scale)
	);
}

// 材質のセル調反射、影色、輪郭光をJSONへ保存する。
void GltfModelCommponennt::Material::SerializeStyleSettings(nlohmann::json& json_) const
{
    json_["specularToonBlend"] = specularToonBlend;
    json_["specularStrength"] = specularStrength;
    json_["rimStrength"] = rimStrength;
    json_["shadowTint"] = { shadowTint.x, shadowTint.y, shadowTint.z };
}

// 古い保存データの互換性を保ち、Shaderへ渡す値を有効範囲に収める。
void GltfModelCommponennt::Material::DeserializeStyleSettings(const nlohmann::json& json_)
{
    specularToonBlend = std::clamp(json_.value("specularToonBlend", specularToonBlend), 0.0f, 1.0f);
    specularStrength = std::clamp(json_.value("specularStrength", specularStrength), 0.0f, 4.0f);
    rimStrength = std::clamp(json_.value("rimStrength", rimStrength), 0.0f, 1.0f);
    if (json_.contains("shadowTint") && json_["shadowTint"].is_array() && json_["shadowTint"].size() == 3)
    {
        const auto& tint_ = json_["shadowTint"];
        shadowTint = { std::clamp(tint_[0].get<float>(), 0.0f, 1.0f),
            std::clamp(tint_[1].get<float>(), 0.0f, 1.0f),
            std::clamp(tint_[2].get<float>(), 0.0f, 1.0f) };
    }
}

template<class Archive>
void GltfModelCommponennt::Material::serialize(Archive& archive)
{
	// enumを明示的にintにキャストして扱うのが最も安全です
	int tempAlphaMode = static_cast<int>(alphaMode);
	int tempShaderId = static_cast<int>(shaderId);
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(baseTextureFileName),
		CEREAL_NVP(normalTextureFileName),
		CEREAL_NVP(emissiveTextureFileName),
		CEREAL_NVP(occlusionTextureFileName),
		CEREAL_NVP(metalnessRoughnessTextureFileName),
		// XMFLOAT4 (16byte)
		cereal::make_nvp("baseColor.x", baseColor.x),
		cereal::make_nvp("baseColor.y", baseColor.y),
		cereal::make_nvp("baseColor.z", baseColor.z),
		cereal::make_nvp("baseColor.w", baseColor.w),
		// XMFLOAT3 (12byte)
		cereal::make_nvp("emissiveColor.x", emissiveColor.x),
		cereal::make_nvp("emissiveColor.y", emissiveColor.y),
		cereal::make_nvp("emissiveColor.z", emissiveColor.z),
		// 以降のfloat
		CEREAL_NVP(metalness),
		CEREAL_NVP(roughness),
		CEREAL_NVP(occlusionStrength),
		CEREAL_NVP(alphaCutoff),
		CEREAL_NVP(toonPbrBlend),
		CEREAL_NVP(outlineThicknessOffset),
		CEREAL_NVP(normalScale),
		cereal::make_nvp("alphaMode", tempAlphaMode),
		cereal::make_nvp("shaderId", tempShaderId),
		CEREAL_NVP(disableOutline)
	);

	if (Archive::is_loading::value)
	{
		alphaMode = static_cast<AlphaMode>(tempAlphaMode);
	}
	if (Archive::is_loading::value)
	{
		shaderId = static_cast<ModelShaderID>(tempShaderId);
	}
}

template<class Archive>
void GltfModelCommponennt::Vertex::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(position),
		CEREAL_NVP(boneWeight),
		CEREAL_NVP(boneIndex),
		CEREAL_NVP(texcoord),
		CEREAL_NVP(normal),
		CEREAL_NVP(tangent)
	);
}

template<class Archive>
void GltfModelCommponennt::Bone::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(offsetTransform)
	);
}

template<class Archive>
void GltfModelCommponennt::Mesh::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(vertices),
		CEREAL_NVP(indices),
		CEREAL_NVP(bones),
		CEREAL_NVP(nodeIndex),
		CEREAL_NVP(materialIndex)
	);
}

template<class Archive>
void GltfModelCommponennt::VectorKeyframe::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void GltfModelCommponennt::QuaternionKeyframe::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(seconds),
		CEREAL_NVP(value)
	);
}

template<class Archive>
void GltfModelCommponennt::NodeAnim::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(positionKeyframes),
		CEREAL_NVP(rotationKeyframes),
		CEREAL_NVP(scaleKeyframes)
	);
}

template<class Archive>
void GltfModelCommponennt::Animation::serialize(Archive& archive)
{
	archive(
		CEREAL_NVP(name),
		CEREAL_NVP(secondsLength),
		CEREAL_NVP(nodeAnims)
	);
}


void GltfModelCommponennt::LoadModel(ID3D11Device* device, const char* filename, float sampleRate)
{
	BS_PROFILE_SCOPE("GltfModelCommponennt::LoadModel");
	Clear();
	asyncLoadRequested = false;
	asyncLoadApplied = false;
	modelLoadHandle = ModelLoadHandle();

	// 1. 名前の記憶
	std::filesystem::path filePath(filename);
	modelFullPass = filename;
	modelName = filePath.stem().string();

	// 2. 神殿からすでにGPUバッファ等も構築済みのリソースを得る
	std::shared_ptr<ModelResource> resource;
	{
		BS_PROFILE_SCOPE("GltfModel::GetOrCreateResource");
		resource = ModelResourceManager::Instance().GetOrCreateResource(device, filename);
	}
	if (!resource) return;

	// 3. データを引き写す（アニメーション再生などで個別に書き換えるためコピーする）
	this->materials = resource->materials;
	this->nodes = resource->nodes;
	this->meshes = resource->meshes;
	this->animations = resource->animations;

	int materialIdCounter = 0;
	for (auto& material : this->materials) {
		material.id = materialIdCounter++;
	}


	// 4. コピーしたことによってポインタが古い場所を指しているので、
	// 「自分自身の vector」を指すように再結線する
	{
		BS_PROFILE_SCOPE("GltfModel::RebindInternalPointers");
		RebindInternalPointers();
	}

    bindNodePoses.resize(nodes.size());
    for (size_t nodeIndex_ = 0; nodeIndex_ < nodes.size(); ++nodeIndex_)
    {
        bindNodePoses[nodeIndex_].position = nodes[nodeIndex_].position;
        bindNodePoses[nodeIndex_].rotation = nodes[nodeIndex_].rotation;
        bindNodePoses[nodeIndex_].scale = nodes[nodeIndex_].scale;
    }

	// 5. 行列の確定
	{
		BS_PROFILE_SCOPE("GltfModel::InitialUpdateTransform");
		UpdateTransform();
	}
}

void GltfModelCommponennt::RequestLoadModelAsync(const char* filename_, int priority_)
{
	BS_PROFILE_SCOPE("GltfModelCommponennt::RequestLoadModelAsync");
	Clear();

	std::filesystem::path filePath_(filename_);
	modelFullPass = filename_;
	modelName = filePath_.stem().string();
	asyncLoadRequested = true;
	asyncLoadApplied = false;
	modelLoadHandle = ModelResourceManager::Instance().RequestResourceAsync(filename_, priority_);
}

void GltfModelCommponennt::ApplyLoadedResource(const std::shared_ptr<ModelResource>& resource_)
{
    BS_PROFILE_SCOPE("GltfModelCommponennt::ApplyLoadedResource");
    if (!resource_) return;

    {
        BS_PROFILE_SCOPE("GltfModel::CopyLoadedResource");
        materials = resource_->materials;
        nodes = resource_->nodes;
        meshes = resource_->meshes;
        animations = resource_->animations;
    }

    {
        BS_PROFILE_SCOPE("GltfModel::AssignMaterialIds");
        int materialIdCounter_ = 0;
        for (auto& material_ : materials)
        {
            material_.id = materialIdCounter_++;
        }
    }

    {
        BS_PROFILE_SCOPE("GltfModel::RebindLoadedPointers");
        RebindInternalPointers();
    }

    {
        BS_PROFILE_SCOPE("GltfModel::BuildBindNodePoses");
        bindNodePoses.resize(nodes.size());
        for (size_t nodeIndex_ = 0; nodeIndex_ < nodes.size(); ++nodeIndex_)
        {
            bindNodePoses[nodeIndex_].position = nodes[nodeIndex_].position;
            bindNodePoses[nodeIndex_].rotation = nodes[nodeIndex_].rotation;
            bindNodePoses[nodeIndex_].scale = nodes[nodeIndex_].scale;
        }
    }

    {
        BS_PROFILE_SCOPE("GltfModel::ApplyPendingMaterialOverrides");
        for (const nlohmann::json& item_ : pendingMaterialOverrides)
        {
            const int id_ = item_.value("id", -1);
            for (auto& mat_ : materials)
            {
                if (mat_.id != id_) continue;

                if (item_.contains("shaderId")) mat_.shaderId = static_cast<ModelShaderID>(item_["shaderId"].get<int>());
                if (item_.contains("baseColor"))
                {
                    const auto& color_ = item_["baseColor"];
                    mat_.baseColor.x = color_.at(0).get<float>();
                    mat_.baseColor.y = color_.at(1).get<float>();
                    mat_.baseColor.z = color_.at(2).get<float>();
                    mat_.baseColor.w = color_.at(3).get<float>();
                }
                if (item_.contains("emissiveColor"))
                {
                    const auto& color_ = item_["emissiveColor"];
                    mat_.emissiveColor.x = color_.at(0).get<float>();
                    mat_.emissiveColor.y = color_.at(1).get<float>();
                    mat_.emissiveColor.z = color_.at(2).get<float>();
                }
                if (item_.contains("metalness")) mat_.metalness = item_["metalness"].get<float>();
                if (item_.contains("roughness")) mat_.roughness = item_["roughness"].get<float>();
                if (item_.contains("occlusionStrength")) mat_.occlusionStrength = item_["occlusionStrength"].get<float>();
                if (item_.contains("alphaCutoff")) mat_.alphaCutoff = item_["alphaCutoff"].get<float>();
                if (item_.contains("normalScale")) mat_.normalScale = item_["normalScale"].get<float>();
                mat_.DeserializeStyleSettings(item_);
                if (item_.contains("uvScale"))
                {
                    const auto& uvScale_ = item_["uvScale"];
                    mat_.uvScale.x = uvScale_.at(0).get<float>();
                    mat_.uvScale.y = uvScale_.at(1).get<float>();
                }
                if (item_.contains("uvOffset"))
                {
                    const auto& uvOffset_ = item_["uvOffset"];
                    mat_.uvOffset.x = uvOffset_.at(0).get<float>();
                    mat_.uvOffset.y = uvOffset_.at(1).get<float>();
                }
                if (item_.contains("uvRotation")) mat_.uvRotation = item_["uvRotation"].get<float>();
                if (item_.contains("uvAddressMode")) mat_.uvAddressMode = static_cast<UV_ADDRESS_MODE>(item_["uvAddressMode"].get<int>());
                if (item_.contains("alphaMode")) mat_.alphaMode = static_cast<AlphaMode>(item_["alphaMode"].get<int>());
                if (item_.contains("toonPbrBlend")) mat_.toonPbrBlend = item_["toonPbrBlend"].get<float>();
                if (item_.contains("disableOutline")) mat_.disableOutline = item_["disableOutline"].get<bool>();
                if (item_.contains("outlineThicknessOffset")) mat_.outlineThicknessOffset = item_["outlineThicknessOffset"].get<float>();
            }
        }
    }

    {
        BS_PROFILE_SCOPE("GltfModel::InitialUpdateTransform");
        UpdateTransform();
    }
    asyncLoadApplied = true;
}

void GltfModelCommponennt::UpdateLoadState()
{
	if (!asyncLoadRequested || asyncLoadApplied) return;
	if (!modelLoadHandle.IsReady()) return;
	ApplyLoadedResource(modelLoadHandle.GetResource());
}

bool GltfModelCommponennt::IsLoadReady() const
{
	if (!asyncLoadRequested) return true;
	return asyncLoadApplied || modelLoadHandle.IsFailed();
}

bool GltfModelCommponennt::IsRuntimeReady() const
{
	if (modelFullPass.empty()) return true;
	if (!asyncLoadRequested) return !nodes.empty() && !meshes.empty();
	return asyncLoadApplied && !nodes.empty() && !meshes.empty();
}

bool GltfModelCommponennt::HasRuntimeFailure() const
{
	return asyncLoadRequested && modelLoadHandle.IsFailed();
}

bool GltfModelCommponennt::IsModelReady() const
{
	return !modelName.empty() && !nodes.empty() && IsLoadReady();
}

bool GltfModelCommponennt::IsModelLoading() const
{
	return asyncLoadRequested && !asyncLoadApplied && !modelLoadHandle.IsFailed();
}

void GltfModelCommponennt::SetPresentationMaterials(std::vector<Material> materials_)
{
    if (materials_.size() != meshes.size()) return;
    presentationMaterials = std::move(materials_);
    for (std::size_t index_ = 0; index_ < meshes.size(); ++index_)
        meshes[index_].material = &presentationMaterials[index_];
}

void GltfModelCommponennt::ClearPresentationMaterials()
{
    if (presentationMaterials.empty()) return;
    for (auto& mesh_ : meshes) mesh_.material = &materials.at(mesh_.materialIndex);
    presentationMaterials.clear();
}

void GltfModelCommponennt::RebindInternalPointers()
{
    presentationMaterials.clear();
	// ノードの親子関係を再結線
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		Node& node = nodes.at(nodeIndex);
		node.parent = node.parentIndex >= 0 ? &nodes.at(node.parentIndex) : nullptr;
		if (node.parent != nullptr)
		{
			node.parent->children.emplace_back(&node);
		}
	}

	// メッシュとマテリアル・ノード・ボーンの再結線
	for (Mesh& mesh : meshes)
	{
		mesh.material = &materials.at(mesh.materialIndex);
		mesh.node = &nodes.at(mesh.nodeIndex);

		for (Bone& bone : mesh.bones)
		{
			bone.node = &nodes.at(bone.nodeIndex);
		}
	}
}

void GltfModelCommponennt::Gui()
{
	if (ImGui::Checkbox("Static Model", &isStaticModel))
	{
		MarkTransformDirty();
	}
	ImGui::TextDisabled("Static models update nodes only when their transform or pose changes.");

	if (ImGui::DragFloat3("localScale XYZ", &localScaleXYZ.x, 0.01f, 0.0f, 100.0f))
	{
		localScale = localScaleXYZ.x;
		MarkTransformDirty();
	}
	ImGui::Checkbox("usedNaviMeshSystemFlg", &usedNaviMeshSystemFlg);

	ImGui::Separator();
	ImGui::Text("Root Rotation Offset");

	// 四元数を float 4つとして編集する
	// Quaternion は {x, y, z, w} の順で並んでおるため、そのように扱う
	float rot[4] = { _RootRotationOffset.x, _RootRotationOffset.y, _RootRotationOffset.z, _RootRotationOffset.w };

	if (ImGui::DragFloat4("##RootOffset", rot, 0.01f))
	{
		_RootRotationOffset.x = rot[0];
		_RootRotationOffset.y = rot[1];
		_RootRotationOffset.z = rot[2];
		_RootRotationOffset.w = rot[3];
		// 念のため正規化しておく
		_RootRotationOffset.Normalize();
		MarkTransformDirty();
	}



	// 表示用の名前リスト
	static const char* shaderNames[] = { "Basic" ,"PBR","Toon","ToonPBR"};

	// 現在のIDをint型として扱う
	int currentIdx = static_cast<int>(shaderId);

	// コンボボックスの作成
	if (ImGui::Combo("Shader ID", &currentIdx, shaderNames, IM_ARRAYSIZE(shaderNames)))
	{
		// 選択が変更されたら列挙型に戻して代入
		shaderId = static_cast<ModelShaderID>(currentIdx);
	}
	ImGui::Separator();
	ImGui::Text("Model Selector");

	// --- モデル選択UI ---
	const std::string rootPath = "Data/Model";

	// static変数で選択状態を保持
	static std::string selectedDir = "";
	static std::string selectedFile = "";

	// 1. Data/Model 内のフォルダを一覧表示
	if (ImGui::BeginCombo("Select Folder", selectedDir.empty() ? "Choose a folder..." : selectedDir.c_str()))
	{
		if (std::filesystem::exists(rootPath) && std::filesystem::is_directory(rootPath)) {
			for (const auto& entry : std::filesystem::directory_iterator(rootPath)) {
				if (entry.is_directory()) {
					std::string dirName = entry.path().filename().string();
					if (ImGui::Selectable(dirName.c_str(), selectedDir == dirName)) {
						selectedDir = dirName;
						selectedFile = ""; // フォルダを変えたらファイル選択をリセット
					}
				}
			}
		}
		ImGui::EndCombo();
	}

	// 2. 選択されたフォルダ内の .gltf / .glb を表示
	if (!selectedDir.empty()) {
		std::string targetDirPath = rootPath + "/" + selectedDir;

		if (ImGui::BeginCombo("Select Model", selectedFile.empty() ? "Choose a file..." : selectedFile.c_str())) {
			if (std::filesystem::exists(targetDirPath)) {
				for (const auto& entry : std::filesystem::recursive_directory_iterator(targetDirPath)) {
					std::string ext = entry.path().extension().string();
					if (ext == ".gltf" || ext == ".glb") {
						// フォルダ内からの相対パスを表示
						std::string relPath = std::filesystem::relative(entry.path(), targetDirPath).string();
						if (ImGui::Selectable(relPath.c_str(), selectedFile == relPath)) {
							selectedFile = relPath;
						}
					}
				}
			}
			ImGui::EndCombo();
		}
	}

	// 3. ロードボタン
	if (!selectedFile.empty()) {
		if (ImGui::Button("Load Selected Model", ImVec2(-1, 0))) {
			std::string fullPath = rootPath + "/" + selectedDir + "/" + selectedFile;

			// ロード前に既存データをクリア
			this->Clear();

			// ロード実行 (deviceはどこからか取得する必要があります。例: Graphics::GetDevice())
			// ここでは仮に extern やグローバルから取得する想定です
			auto device = GraphicsDeviceManager::Instance().GetDevice();
			LoadModel(device, fullPath.c_str());
		}
	}

	// --- 追加：ダイアログでのファイル選択 ---
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// エクスプローラーから直接モデルを読みこむボタン
	if (ImGui::Button("Browse via Dialog...", ImVec2(-1, 0)))
	{
		// Windowsのダイアログを開き、glTF/glbファイルを選択させる
		// 初期ディレクトリは "Data/Model" を起点とする
		std::string filePath = WindowsDialogHelper::OpenGltfDialog(L"モデルファイルを選択してください", rootPath);

		if (!filePath.empty())
		{
			// 例: "Data/Model/Hero/Knight/knight.gltf" というパスが返ってきた場合の処理
			std::filesystem::path selectedPath(filePath);
			std::filesystem::path base(rootPath);

			// rootPath ("Data/Model") からの相対パスを計算する
			// 例: "Hero/Knight/knight.gltf"
			std::filesystem::path relativeToRoot = std::filesystem::relative(selectedPath, base);

			if (!relativeToRoot.empty() && relativeToRoot.string().find("..") == std::string::npos)
			{
				// 最初の階層を selectedDir とする (例: "Hero")
				auto it = relativeToRoot.begin();
				selectedDir = it->string();

				// 残りの部分（サブフォルダを含むファイルパス）を selectedFile とする (例: "Knight/knight.gltf")
				std::filesystem::path remainingPath;
				for (++it; it != relativeToRoot.end(); ++it)
				{
					remainingPath /= *it;
				}
				// ImGuiの既存ロジックに合わせて、区切り文字を汎用的なフォーマットに統一
				selectedFile = remainingPath.generic_string();
			}
			else
			{
				// 万が一 Data/Model 外のファイルが選ばれた場合の安全弁
				selectedDir = "";
				selectedFile = "";
			}
		}
	}


	ImGui::Text("Current: %s", modelName.c_str());

	ImGui::Text("RootMotion Selector");

	// 現在のRootMotion名を保持するインデックスを特定
	int currentRootMotionIdx = -1;
	for (int i = 0; i < nodes.size(); ++i) {
		if (nodes[i].name == m_RootMotionName) {
			currentRootMotionIdx = i;
			break;
		}
	}

	// 選択用のコンボボックスを作成
	const char* previewValue = (currentRootMotionIdx != -1) ? nodes[currentRootMotionIdx].name.c_str() : "None";

	if (ImGui::BeginCombo("RootMotion Node", previewValue))
	{
		// Noneを選択できるようにする
		if (ImGui::Selectable("None", currentRootMotionIdx == -1)) {
			m_RootMotionName = "";
		}

		// すべてのノードをリスト化
		for (int i = 0; i < nodes.size(); ++i)
		{
			bool isSelected = (i == currentRootMotionIdx);
			if (ImGui::Selectable(nodes[i].name.c_str(), isSelected))
			{
				m_RootMotionName = nodes[i].name;
			}

			// 選択されたら自動的にスクロールする機能を付与
			if (isSelected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ShowMeshDetailsUI();
}

// アニメーション追加読み込み
void GltfModelCommponennt::AppendAnimations(const char* filename)
{
	BS_PROFILE_SCOPE("GltfModelCommponennt::AppendAnimations");
	std::filesystem::path filepath(filename);
	std::filesystem::path dirpath(filepath.parent_path());

	if (filepath.extension() == ".gltf" ||
		filepath.extension() == ".glb")
	{
		// 汎用モデルファイルの読み込み
		GLTFImporter importer(filename);

		// アニメーションデータ読み取り
		importer.LoadAnimations(animations, nodes);
	}
	else
	{
		_ASSERT_EXPR_A(false, "found not model file");
	}
}

// アニメーションインデックス取得
int GltfModelCommponennt::GetAnimationIndex(const char* name) const
{
	for (size_t animationIndex = 0; animationIndex < animations.size(); ++animationIndex)
	{
		if (animations.at(animationIndex).name == name)
		{
			return static_cast<int>(animationIndex);
		}
	}
	return -1;
}

// ノードインデックス取得
int GltfModelCommponennt::GetNodeIndex(const char* name) const
{
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		if (nodes.at(nodeIndex).name == name)
		{
			return static_cast<int>(nodeIndex);
		}
	}
	return -1;
}

GltfModelCommponennt::Node& GltfModelCommponennt::GetNodes(std::string nodeName)
{
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		if (nodes.at(nodeIndex).name == nodeName)
		{
			return nodes.at(nodeIndex);
		}
	}
	assert(L"ノードが見つかりません");
}

void GltfModelCommponennt::UpdateNodeRecursive(
	Node& node_,
	const DirectX::XMMATRIX& parentGlobalTransform_,
	const DirectX::XMMATRIX& parentWorldTransform_,
	const DirectX::XMMATRIX& modelScaleMat_,
	bool isRoot_,
	bool updateChildren_)
{
	const DirectX::XMMATRIX scaleTransform_ =
		DirectX::XMMatrixScaling(node_.scale.x, node_.scale.y, node_.scale.z);

	const DirectX::XMMATRIX rotationTransform_ =
		DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node_.rotation));

	const DirectX::XMMATRIX translationTransform_ =
		DirectX::XMMatrixTranslation(node_.position.x, node_.position.y, node_.position.z);

	const DirectX::XMMATRIX localTransform_ =
		scaleTransform_ * rotationTransform_ * translationTransform_;

	const DirectX::XMMATRIX globalTransform_ = isRoot_
		? localTransform_ * modelScaleMat_
		: localTransform_ * parentGlobalTransform_;

	const DirectX::XMMATRIX worldTransform_ =
		globalTransform_ * parentWorldTransform_;

	DirectX::XMStoreFloat4x4(&node_.localTransform, localTransform_);
	DirectX::XMStoreFloat4x4(&node_.globalTransform, globalTransform_);
	DirectX::XMStoreFloat4x4(&node_.worldTransform, worldTransform_);

	if (!updateChildren_) return;

	for (Node* child_ : node_.children)
	{
		if (!child_) continue;

		UpdateNodeRecursive(
			*child_,
			globalTransform_,
			parentWorldTransform_,
			modelScaleMat_,
			false);
	}
}

void GltfModelCommponennt::UpdateNodeTransformOnly(int nodeIndex_)
{
	if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes.size())) return;

	const std::shared_ptr<GameObject> owner_ = GetGameObject();
	if (!owner_ || !owner_->transform) return;

	Node& node_ = nodes[nodeIndex_];
	const bool isRoot_ = node_.parent == nullptr;
	const DirectX::XMMATRIX parentGlobalTransform_ = isRoot_
		? DirectX::XMMatrixIdentity()
		: DirectX::XMLoadFloat4x4(&node_.parent->globalTransform);
	const DirectX::XMMATRIX parentWorldTransform_ = owner_->transform->GetWorldMatrix();
	const DirectX::XMMATRIX modelScaleMat_ = MakeModelLocalTransform();

	UpdateNodeRecursive(
		node_,
		parentGlobalTransform_,
		parentWorldTransform_,
		modelScaleMat_,
		isRoot_,
		false);

	if (node_.parent == nullptr)
	{
		DirectX::XMStoreFloat4x4(&cachedOwnerWorldTransform, parentWorldTransform_);
		hasCachedOwnerWorldTransform = true;
	}
	MarkSkinningTransformsDirty();
}

void GltfModelCommponennt::MarkPoseDirty()
{
	poseDirty = true;
	MarkSkinningTransformsDirty();
}

void GltfModelCommponennt::MarkTransformDirty()
{
	transformDirty = true;
	MarkSkinningTransformsDirty();
}

void GltfModelCommponennt::MarkSkinningTransformsDirty()
{
	for (Mesh& mesh_ : meshes)
	{
		mesh_.skinningTransformDirty = true;
	}
}

void GltfModelCommponennt::SetLocalRoll(float degrees_)
{
	if (localRoll == degrees_) return;
	localRoll = degrees_;
	MarkTransformDirty();
}

DirectX::XMMATRIX GltfModelCommponennt::MakeModelLocalTransform() const
{
	const auto scale_ = DirectX::XMMatrixScaling(localScaleXYZ.x, localScaleXYZ.y, localScaleXYZ.z);
	// 本体Transformより手前で傾け、移動やColliderの基準を保つ。
	return localRoll == 0.0f ? scale_ : scale_ * DirectX::XMMatrixRotationZ(DirectX::XMConvertToRadians(localRoll));
}

void GltfModelCommponennt::UpdateTransformIfDirty()
{
	const std::shared_ptr<GameObject> owner_ = GetGameObject();
	if (owner_ && owner_->transform)
	{
		DirectX::XMFLOAT4X4 currentOwnerWorldTransform_{};
		DirectX::XMStoreFloat4x4(
			&currentOwnerWorldTransform_,
			owner_->transform->GetWorldMatrix());

		if (!hasCachedOwnerWorldTransform ||
			std::memcmp(
				&cachedOwnerWorldTransform,
				&currentOwnerWorldTransform_,
				sizeof(DirectX::XMFLOAT4X4)) != 0)
		{
			transformDirty = true;
		}
	}

	if (!poseDirty && !transformDirty)
	{
		return;
	}

	UpdateTransform();

	poseDirty = false;
	transformDirty = false;
}

void GltfModelCommponennt::UpdateNodeRecursive(
	Node& node, 
	const DirectX::XMMATRIX& ParentWorldTransform, 
	const DirectX::XMMATRIX& ModelScaleMat)
{
	// アニメーション適用後のSRTからローカル行列を作成
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
	DirectX::XMMATRIX LocalTransform = S * R * T;

	DirectX::XMMATRIX GlobalTransform;
	if (node.parent != nullptr)
	{
		// 子ノード：親の「今フレームの最新の」グローバル行列を継承
		DirectX::XMMATRIX ParentGlobal = DirectX::XMLoadFloat4x4(&node.parent->globalTransform);
		GlobalTransform = LocalTransform * ParentGlobal;
	}
	else
	{
		// ルートノード：ここでモデル全体のスケールを一度だけ適用
		GlobalTransform = LocalTransform * ModelScaleMat;
	}

	// 最終的なワールド行列
	DirectX::XMMATRIX WorldTransform = GlobalTransform * ParentWorldTransform;

	// 行列を保存（これで子が最新の親を参照できる）
	DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);
	DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);
	DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);

	// 子ノードを再帰的に更新
	for (Node* child : node.children)
	{
		UpdateNodeRecursive(*child, ParentWorldTransform, ModelScaleMat);
	}
}

// トランスフォーム更新処理
void GltfModelCommponennt::UpdateTransform()
{
	BS_PROFILE_SCOPE("GltfModelCommponennt::UpdateTransform");

	auto owner_ = this->GetGameObject();
	if (!owner_ || !owner_->transform)
	{
		return;
	}

	size_t boneCount_ = 0;
	size_t rootNodeCount_ = 0;
	for (const Mesh& mesh_ : meshes)
	{
		boneCount_ += mesh_.bones.size();
	}
	for (const Node& node_ : nodes)
	{
		if (node_.parent == nullptr)
		{
			++rootNodeCount_;
		}
	}

#if BS_PROFILE_DETAILED_ENABLED
	{
		char profileText_[256]{};
		const int textLength_ = std::snprintf(
			profileText_,
			sizeof(profileText_),
			"Model: %s\nNodeCount: %zu\nMeshCount: %zu\nRootNodeCount: %zu\nBoneCount: %zu",
			modelName.c_str(),
			nodes.size(),
			meshes.size(),
			rootNodeCount_,
			boneCount_);
		if (textLength_ > 0)
		{
			BS_PROFILE_TEXT(
				profileText_,
				static_cast<size_t>((std::min)(textLength_, static_cast<int>(sizeof(profileText_) - 1))));
		}
	}
#endif

	BS_PROFILE_PLOT("GltfModel.Nodes", static_cast<int64_t>(nodes.size()));
	BS_PROFILE_PLOT("Animation.UpdatedModelCount", 1);
	BS_PROFILE_PLOT("Animation.UpdatedBoneCount", static_cast<int64_t>(boneCount_));

	DirectX::XMMATRIX parentWorldTransform_ = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX modelScaleMat_ = DirectX::XMMatrixIdentity();
	{
		BS_PROFILE_SCOPE("GltfModel::PrepareRootTransform");
		parentWorldTransform_ = owner_->transform->GetWorldMatrix();
		modelScaleMat_ = MakeModelLocalTransform();
	}

	{
		BS_PROFILE_SCOPE("GltfModel::UpdateNodeHierarchy");
		BS_PROFILE_PLOT("GltfModel.RootNodeCount", static_cast<int64_t>(rootNodeCount_));

		{
			BS_PROFILE_SCOPE("GltfModel::CalculateGlobalTransforms");
			for (Node& node_ : nodes)
			{
				if (node_.parent == nullptr)
				{
					DirectX::XMMATRIX rootParentGlobal_ = DirectX::XMMatrixIdentity();
					UpdateNodeRecursive(node_, rootParentGlobal_, parentWorldTransform_, modelScaleMat_, true);
				}
			}
		}
	}

	{
		BS_PROFILE_SCOPE("GltfModel::FinalizeTransformUpdate");
		DirectX::XMStoreFloat4x4(&cachedOwnerWorldTransform, parentWorldTransform_);
		hasCachedOwnerWorldTransform = true;
		poseDirty = false;
		transformDirty = false;
		MarkSkinningTransformsDirty();
	}
}

const std::vector<DirectX::XMFLOAT4X4>& GltfModelCommponennt::GetSkinningTransforms(int meshIndex_)
{
	if (meshIndex_ < 0 || meshIndex_ >= static_cast<int>(meshes.size()))
	{
		static const std::vector<DirectX::XMFLOAT4X4> emptyTransforms_;
		return emptyTransforms_;
	}

	UpdateTransformIfDirty();
	UpdateSkinningTransforms(meshIndex_);
	return meshes[meshIndex_].cachedSkinningTransforms;
}

void GltfModelCommponennt::CollectWorldVertexSamples(std::vector<DirectX::XMFLOAT3>& positions_, std::size_t maximumCount_)
{
    BS_PROFILE_SCOPE("GltfModel::CollectWorldVertexSamples");
    positions_.clear();
    if (!IsModelReady() || maximumCount_ == 0) return;
    std::size_t count_ = 0;
    for (const auto& mesh_ : meshes) count_ += mesh_.vertices.size();
    const auto stride_ = (std::max)(std::size_t{1}, (count_ + maximumCount_ - 1) / maximumCount_);
    std::size_t index_ = 0;
    for (int meshIndex_ = 0; meshIndex_ < static_cast<int>(meshes.size()); ++meshIndex_)
    {
        const auto& mesh_ = meshes[meshIndex_];
        const auto& matrices_ = GetSkinningTransforms(meshIndex_);
        for (const auto& vertex_ : mesh_.vertices)
        {
            if (index_++ % stride_ != 0) continue;
            const auto local_ = DirectX::XMLoadFloat3(&vertex_.position);
            auto world_ = DirectX::XMVectorZero();
            if (!mesh_.bones.empty())
            {
                const float weights_[] = {vertex_.boneWeight.x, vertex_.boneWeight.y, vertex_.boneWeight.z, vertex_.boneWeight.w};
                const uint32_t indices_[] = {vertex_.boneIndex.x, vertex_.boneIndex.y, vertex_.boneIndex.z, vertex_.boneIndex.w};
                for (int influence_ = 0; influence_ < 4; ++influence_)
                    if (weights_[influence_] > 0.0f && indices_[influence_] < matrices_.size())
                        world_ = DirectX::XMVectorAdd(world_, DirectX::XMVectorScale(
                            DirectX::XMVector3TransformCoord(local_, DirectX::XMLoadFloat4x4(&matrices_[indices_[influence_]])), weights_[influence_]));
            }
            else world_ = matrices_.empty() ? local_ : DirectX::XMVector3TransformCoord(local_, DirectX::XMLoadFloat4x4(&matrices_[0]));
            DirectX::XMFLOAT3 position_;
            DirectX::XMStoreFloat3(&position_, world_);
            positions_.push_back(position_);
            if (positions_.size() >= maximumCount_) return;
        }
    }
}

void GltfModelCommponennt::UpdateSkinningTransforms(int meshIndex_)
{
	Mesh& mesh_ = meshes[meshIndex_];
	if (!mesh_.skinningTransformDirty && !mesh_.cachedSkinningTransforms.empty())
	{
		return;
	}

	BS_PROFILE_SCOPE("GltfModel::CalculateSkinningMatrices");
	//BS_PROFILE_PLOT("GltfModel.SkinningCacheRebuild", 1);

	const size_t transformCount_ = mesh_.bones.empty()
		? 1
		: (std::min)(mesh_.bones.size(), static_cast<size_t>(256));
	BS_PROFILE_PLOT("GltfModel.SkinningTransformCount", static_cast<int64_t>(transformCount_));
	mesh_.cachedSkinningTransforms.resize(transformCount_);

	if (!mesh_.bones.empty())
	{
		BS_PROFILE_SCOPE("Skinning::BuildBoneMatrices");
		BS_PROFILE_PLOT("Skinning.BoneCount", static_cast<int64_t>(mesh_.bones.size()));
		for (size_t boneIndex_ = 0; boneIndex_ < transformCount_; ++boneIndex_)
		{
			const Bone& bone_ = mesh_.bones[boneIndex_];
			if (!bone_.node) continue;

			const DirectX::XMMATRIX worldTransform_ =
				DirectX::XMLoadFloat4x4(&bone_.node->worldTransform);
			const DirectX::XMMATRIX offsetTransform_ =
				DirectX::XMLoadFloat4x4(&bone_.offsetTransform);
			DirectX::XMStoreFloat4x4(
				&mesh_.cachedSkinningTransforms[boneIndex_],
				offsetTransform_ * worldTransform_);
		}
	}
	else if (mesh_.node)
	{
		BS_PROFILE_SCOPE("Skinning::WriteSkinningBuffer");
		mesh_.cachedSkinningTransforms[0] = mesh_.node->worldTransform;
	}
	else
	{
		BS_PROFILE_SCOPE("Skinning::WriteSkinningBuffer");
		DirectX::XMStoreFloat4x4(
			&mesh_.cachedSkinningTransforms[0],
			DirectX::XMMatrixIdentity());
	}

	{
		BS_PROFILE_SCOPE("Skinning::WriteSkinningBuffer");
		BS_PROFILE_PLOT(
			"Skinning.UploadByteSize",
			static_cast<int64_t>(mesh_.cachedSkinningTransforms.size() * sizeof(DirectX::XMFLOAT4X4)));
	}

	mesh_.skinningTransformDirty = false;
}

void GltfModelCommponennt::UpdateNodeTransformFrom(int nodeIndex_)
{
	if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes.size()))
	{
		return;
	}

	auto owner_ = this->GetGameObject();
	if (!owner_ || !owner_->transform)
	{
		return;
	}

	DirectX::XMMATRIX parentWorldTransform_ =
		owner_->transform->GetWorldMatrix();

	DirectX::XMMATRIX modelScaleMat_ = MakeModelLocalTransform();

	UpdateNodeRecursive(nodes[nodeIndex_], parentWorldTransform_, modelScaleMat_);
	MarkSkinningTransformsDirty();
}

void GltfModelCommponennt::UpdateNodeTransformFrom(int nodeIndex_, const DirectX::XMMATRIX& parentWorldTransform_, const DirectX::XMMATRIX& modelScaleMat_)
{
	if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes.size()))
	{
		return;
	}

	UpdateNodeRecursive(nodes[nodeIndex_], parentWorldTransform_, modelScaleMat_);
	MarkSkinningTransformsDirty();
}

//{
//	DirectX::XMMATRIX ParentWorldTransform = this->GetGameObject()->transform->worldMatrix;
//
//	// floatのlocalScaleからスケーリング行列を作成
//	DirectX::XMMATRIX ModelScaleMat = DirectX::XMMatrixScaling(localScale, localScale, localScale);
//
//	for (Node& node : nodes)
//	{
//		// ノード自身のローカル行列
//		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
//		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
//		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
//		DirectX::XMMATRIX LocalTransform = S * R * T;
//
//		DirectX::XMMATRIX GlobalTransform;
//		if (node.parent != nullptr)
//		{
//			// 子ノード：親のグローバル行列を継承
//			DirectX::XMMATRIX ParentGlobal = DirectX::XMLoadFloat4x4(&node.parent->globalTransform);
//			GlobalTransform = LocalTransform * ParentGlobal;
//		}
//		else
//		{
//			// ルートノード：自身のローカル行列に「モデル全体の補正スケール」を適用
//			GlobalTransform = LocalTransform * ModelScaleMat;
//		}
//
//		// ワールド行列算出（ゲーム世界の座標へ）
//		DirectX::XMMATRIX WorldTransform = GlobalTransform * ParentWorldTransform;
//
//		// 格納
//		DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);
//		DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);
//		DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);
//	}
//}

void GltfModelCommponennt::ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const
{
	const Animation& animation = animations.at(animationIndex);
	const NodeAnim& nodeAnim = animation.nodeAnims.at(nodeIndex);

	// 位置
	for (size_t index = 0; index < nodeAnim.positionKeyframes.size() - 1; ++index)
	{
		// 現在の時間がどのキーフレームの間にいるか判定する
		const VectorKeyframe& keyframe0 = nodeAnim.positionKeyframes.at(index);
		const VectorKeyframe& keyframe1 = nodeAnim.positionKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			// 再生時間とキーフレームの時間から補完率を算出する
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			// 前のキーフレームと次のキーフレームの姿勢を補完
			DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);
			DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);
			DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);
			// 計算結果をノードに格納
			DirectX::XMStoreFloat3(&nodePose.position, V);
		}
	}
	// 回転
	for (size_t index = 0; index < nodeAnim.rotationKeyframes.size() - 1; ++index)
	{
		// 現在の時間がどのキーフレームの間にいるか判定する
		const QuaternionKeyframe& keyframe0 = nodeAnim.rotationKeyframes.at(index);
		const QuaternionKeyframe& keyframe1 = nodeAnim.rotationKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			// 再生時間とキーフレームの時間から補完率を算出する
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			// 前のキーフレームと次のキーフレームの姿勢を補完
			DirectX::XMVECTOR Q0 = DirectX::XMLoadFloat4(&keyframe0.value);
			DirectX::XMVECTOR Q1 = DirectX::XMLoadFloat4(&keyframe1.value);
			DirectX::XMVECTOR Q = DirectX::XMQuaternionSlerp(Q0, Q1, rate);
			// 計算結果をノードに格納
			DirectX::XMStoreFloat4(&nodePose.rotation, Q);
		}
	}
	// スケール
	for (size_t index = 0; index < nodeAnim.scaleKeyframes.size() - 1; ++index)
	{
		// 現在の時間がどのキーフレームの間にいるか判定する
		const VectorKeyframe& keyframe0 = nodeAnim.scaleKeyframes.at(index);
		const VectorKeyframe& keyframe1 = nodeAnim.scaleKeyframes.at(index + 1);
		if (time >= keyframe0.seconds && time <= keyframe1.seconds)
		{
			// 再生時間とキーフレームの時間から補完率を算出する
			float rate = (time - keyframe0.seconds) / (keyframe1.seconds - keyframe0.seconds);

			// 前のキーフレームと次のキーフレームの姿勢を補完
			DirectX::XMVECTOR V0 = DirectX::XMLoadFloat3(&keyframe0.value);
			DirectX::XMVECTOR V1 = DirectX::XMLoadFloat3(&keyframe1.value);
			DirectX::XMVECTOR V = DirectX::XMVectorLerp(V0, V1, rate);
			// 計算結果をノードに格納
			DirectX::XMStoreFloat3(&nodePose.scale, V);
		}
	}
}

// アニメーション計算
void GltfModelCommponennt::ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const
{
	if (nodePoses.size() != nodes.size())
	{
		nodePoses.resize(nodes.size());
	}
	for (size_t nodeIndex = 0; nodeIndex < nodePoses.size(); ++nodeIndex)
	{
		ComputeAnimation(animationIndex, static_cast<int>(nodeIndex), time, nodePoses.at(nodeIndex));
	}
}

// ノードポーズ設定
void GltfModelCommponennt::SetNodePoses(const std::vector<NodePose>& nodePoses)
{
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		const NodePose& pose = nodePoses.at(nodeIndex);
		Node& node = nodes.at(nodeIndex);

		node.position = pose.position;
		node.rotation = pose.rotation;
		node.scale = pose.scale;
	}
	MarkPoseDirty();
}

// ノードポーズ取得
void GltfModelCommponennt::GetNodePoses(std::vector<NodePose>& nodePoses) const
{
	if (nodePoses.size() != nodes.size())
	{
		nodePoses.resize(nodes.size());
	}
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		const Node& node = nodes.at(nodeIndex);
		NodePose& pose = nodePoses.at(nodeIndex);

		pose.position = node.position;
		pose.rotation = node.rotation;
		pose.scale = node.scale;
	}
}


const GltfModelCommponennt::NodePose& GltfModelCommponennt::GetBindNodePose(int nodeIndex_) const
{
    if (nodeIndex_ >= 0 && nodeIndex_ < static_cast<int>(bindNodePoses.size()))
    {
        return bindNodePoses[nodeIndex_];
    }

    static const NodePose fallbackPose_{};
    return fallbackPose_;
}

float GltfModelCommponennt::GetAnimationDuration(int animationIndex) const
{
	const Animation& animation = animations.at(animationIndex);

	float maxDuration = 0.0f;

	for (const NodeAnim& nodeAnim : animation.nodeAnims)
	{
		if (!nodeAnim.positionKeyframes.empty())
		{
			float end = nodeAnim.positionKeyframes.back().seconds;
			maxDuration = std::max(maxDuration, end);
		}
		if (!nodeAnim.rotationKeyframes.empty())
		{
			float end = nodeAnim.rotationKeyframes.back().seconds;
			maxDuration = std::max(maxDuration, end);
		}
		if (!nodeAnim.scaleKeyframes.empty())
		{
			float end = nodeAnim.scaleKeyframes.back().seconds;
			maxDuration = std::max(maxDuration, end);
		}
	}

	return maxDuration;
}

void GltfModelCommponennt::ShowModelNodePositionsUI()
{
	const std::vector<Node>& nodes = GetNodes();

	if (ImGui::CollapsingHeader("Model Node Positions"))
	{
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const Node& node = nodes[i];

			ImGui::Text("[%zu] %s", i, node.name.c_str());
			ImGui::Indent();
			ImGui::Text("Position: X: %.3f, Y: %.3f, Z: %.3f",
				node.position.x,
				node.position.y,
				node.position.z);
			ImGui::Unindent();
		}
	}
}

void GltfModelCommponennt::ShowNodeDetailsUI(const Node* node)
{
	if (ImGui::CollapsingHeader(("Node: " + node->name).c_str()))
	{
		ImGui::Indent();

		// 親ノード名表示（あれば）
		if (node->parent)
		{
			ImGui::Text("Parent: %s", node->parent->name.c_str());
		}
		else
		{
			ImGui::Text("Parent: (none)");
		}

		// 基本情報
		ImGui::Text("Position: X: %.3f, Y: %.3f, Z: %.3f",
			node->position.x, node->position.y, node->position.z);

		ImGui::Text("Rotation (quat): X: %.3f, Y: %.3f, Z: %.3f, W: %.3f",
			node->rotation.x, node->rotation.y, node->rotation.z, node->rotation.w);

		ImGui::Text("Scale: X: %.3f, Y: %.3f, Z: %.3f",
			node->scale.x, node->scale.y, node->scale.z);

		// 行列を4x4で表示
		auto ShowMatrix4x4 = [](const DirectX::XMFLOAT4X4& mat)
			{
				for (int i = 0; i < 4; ++i)
				{
					ImGui::Text("%.3f\t%.3f\t%.3f\t%.3f",
						mat.m[i][0], mat.m[i][1], mat.m[i][2], mat.m[i][3]);
				}
			};

		ImGui::Separator();
		ImGui::Text("Local Transform:");
		ShowMatrix4x4(node->localTransform);

		ImGui::Separator();
		ImGui::Text("Global Transform:");
		ShowMatrix4x4(node->globalTransform);

		ImGui::Separator();
		ImGui::Text("World Transform:");
		ShowMatrix4x4(node->worldTransform);

		ImGui::Unindent();
	}
}

void GltfModelCommponennt::Serialize(nlohmann::json& j) const 
{
	// 1. パスを相対形式に整える
	std::string pathForSave = modelFullPass;

	// もしパスが空でなく、かつ "./" で始まっていない場合
	if (!pathForSave.empty()) {
		// バックスラッシュ '\' を '/' に統一 (JSONやクロスプラットフォームで安全)
		std::replace(pathForSave.begin(), pathForSave.end(), '\\', '/');

		// すでに "./" や "../" で始まっていないなら "./" を付与
		if (pathForSave.find("./") != 0 && pathForSave.find("../") != 0) {
			pathForSave = "./" + pathForSave;
		}
	}

	nlohmann::json materialSettings = nlohmann::json::array();
	for (const auto& mat : materials) {
		materialSettings.push_back({
	{"id", mat.id},
	{"name", mat.name},
	{"shaderId", static_cast<int>(mat.shaderId)},

	{"baseColor", {mat.baseColor.x, mat.baseColor.y, mat.baseColor.z, mat.baseColor.w}},
	{"emissiveColor", {mat.emissiveColor.x, mat.emissiveColor.y, mat.emissiveColor.z}},

	{"metalness", mat.metalness},
	{"roughness", mat.roughness},
	{"occlusionStrength", mat.occlusionStrength},
	{"alphaCutoff", mat.alphaCutoff},
	{"normalScale", mat.normalScale},
	{"uvScale", {mat.uvScale.x, mat.uvScale.y}},
	{"uvOffset", {mat.uvOffset.x, mat.uvOffset.y}},
	{"uvRotation", mat.uvRotation},
	{"uvAddressMode", static_cast<int>(mat.uvAddressMode)},

	{"alphaMode", static_cast<int>(mat.alphaMode)},

	{"toonPbrBlend", mat.toonPbrBlend},
	{"disableOutline", mat.disableOutline},
	{"outlineThicknessOffset", mat.outlineThicknessOffset}
			});
        mat.SerializeStyleSettings(materialSettings.back());
	}
	j["MaterialOverrides"] = materialSettings;

	j["ModelName"] = pathForSave;
	j["ShaderID"] = static_cast<int>(shaderId);
	j["LocalScale"] = localScale;
	j["LocalScaleXYZ"] = { localScaleXYZ.x, localScaleXYZ.y, localScaleXYZ.z };
	j["StaticModel"] = isStaticModel;
	j["UsedNaviMeshSystemFlg"] = usedNaviMeshSystemFlg;
	j["RootMotionName"] = m_RootMotionName;
	// クォータニオンをx, y, z, wの成分に分解して保存する
	j["_RootRotationOffset"] = {
		{ "x", _RootRotationOffset.x },
		{ "y", _RootRotationOffset.y },
		{ "z", _RootRotationOffset.z },
		{ "w", _RootRotationOffset.w }
	};
}


void GltfModelCommponennt::Deserialize(const nlohmann::json& j) 
{
	BS_PROFILE_SCOPE("GltfModelCommponennt::DeserializeJson");
	isStaticModel = j.value("StaticModel", false);
	hasCachedOwnerWorldTransform = false;
	pendingMaterialOverrides.clear();
	MarkTransformDirty();

	if (j.contains("ShaderID")) {
		shaderId = static_cast<ModelShaderID>(j["ShaderID"].get<int>());
	}

	if (j.contains("ModelName")) {
		std::string name = j["ModelName"];
		if (!name.empty()) {
			// パス正規化（重要）
			if (name.substr(0, 2) == "./") name = name.substr(2);
			std::replace(name.begin(), name.end(), '/', '\\');


			// 1. モデルをロード
			RequestLoadModelAsync(name.c_str(), static_cast<int>(PRELOAD_PRIORITY::NORMAL));

			// 2. このコンポーネントが持っているモデルデータを 
			//    現在の Transform（Loadで復元済み）に強制同期させる
			UpdateLoadState();
		}
	}

	if (j.contains("MaterialOverrides")) {
		for (const auto& item_ : j["MaterialOverrides"])
		{
			pendingMaterialOverrides.push_back(item_);
		}
		for (const auto& item : j["MaterialOverrides"]) {
			int id = item["id"];
			// IDをキーにして、現在のmaterials配列から該当するものを探して上書きする
			for (auto& mat : materials) {
				if (mat.id == id) {
					if (item.contains("shaderId"))
					{
						mat.shaderId = static_cast<ModelShaderID>(item["shaderId"].get<int>());
					}

					if (item.contains("baseColor"))
					{
						const auto& color_ = item["baseColor"];
						mat.baseColor.x = color_.at(0).get<float>();
						mat.baseColor.y = color_.at(1).get<float>();
						mat.baseColor.z = color_.at(2).get<float>();
						mat.baseColor.w = color_.at(3).get<float>();
					}

					if (item.contains("emissiveColor"))
					{
						const auto& color_ = item["emissiveColor"];
						mat.emissiveColor.x = color_.at(0).get<float>();
						mat.emissiveColor.y = color_.at(1).get<float>();
						mat.emissiveColor.z = color_.at(2).get<float>();
					}

					if (item.contains("metalness"))
					{
						mat.metalness = item["metalness"].get<float>();
					}

					if (item.contains("roughness"))
					{
						mat.roughness = item["roughness"].get<float>();
					}

					if (item.contains("occlusionStrength"))
					{
						mat.occlusionStrength = item["occlusionStrength"].get<float>();
					}

					if (item.contains("alphaCutoff"))
					{
						mat.alphaCutoff = item["alphaCutoff"].get<float>();
					}

					if (item.contains("normalScale"))
					{
						mat.normalScale = item["normalScale"].get<float>();
					}
					mat.DeserializeStyleSettings(item);

					if (item.contains("uvScale"))
					{
						const auto& uvScale_ = item["uvScale"];
						mat.uvScale.x = uvScale_.at(0).get<float>();
						mat.uvScale.y = uvScale_.at(1).get<float>();
					}

					if (item.contains("uvOffset"))
					{
						const auto& uvOffset_ = item["uvOffset"];
						mat.uvOffset.x = uvOffset_.at(0).get<float>();
						mat.uvOffset.y = uvOffset_.at(1).get<float>();
					}

					if (item.contains("uvRotation"))
					{
						mat.uvRotation = item["uvRotation"].get<float>();
					}

					if (item.contains("uvAddressMode"))
					{
						mat.uvAddressMode = static_cast<UV_ADDRESS_MODE>(item["uvAddressMode"].get<int>());
					}

					if (item.contains("alphaMode"))
					{
						mat.alphaMode = static_cast<AlphaMode>(item["alphaMode"].get<int>());
					}

					if (item.contains("toonPbrBlend"))
					{
						mat.toonPbrBlend = item["toonPbrBlend"].get<float>();
					}

					if (item.contains("disableOutline"))
					{
						mat.disableOutline = item["disableOutline"].get<bool>();
					}

					if (item.contains("outlineThicknessOffset"))
					{
						mat.outlineThicknessOffset = item["outlineThicknessOffset"].get<float>();
					}
				}
			}
		}
	}

	if (j.contains("LocalScale")) {
		localScale = static_cast<float>(j["LocalScale"].get<float>());
		localScaleXYZ = { localScale, localScale, localScale };
	}
	if (j.contains("LocalScaleXYZ")) {
		const auto& scale_ = j["LocalScaleXYZ"];
		if (scale_.is_array() && scale_.size() >= 3)
		{
			localScaleXYZ.x = scale_.at(0).get<float>();
			localScaleXYZ.y = scale_.at(1).get<float>();
			localScaleXYZ.z = scale_.at(2).get<float>();
		}
		else if (scale_.is_object())
		{
			localScaleXYZ.x = scale_.value("x", localScaleXYZ.x);
			localScaleXYZ.y = scale_.value("y", localScaleXYZ.y);
			localScaleXYZ.z = scale_.value("z", localScaleXYZ.z);
		}
		localScale = localScaleXYZ.x;
	}
	if (j.contains("UsedNaviMeshSystemFlg")) {
		usedNaviMeshSystemFlg = static_cast<bool>(j["UsedNaviMeshSystemFlg"].get<bool>());
	}

	if (j.contains("RootMotionName"))
	{
		m_RootMotionName = j["RootMotionName"];
	}

	// 保存された成分からクォータニオンを再構築する
	if (j.contains("_RootRotationOffset"))
	{
		auto& rot = j["_RootRotationOffset"];
		_RootRotationOffset.x = rot.value("x", 0.0f);
		_RootRotationOffset.y = rot.value("y", 0.0f);
		_RootRotationOffset.z = rot.value("z", 0.0f);
		_RootRotationOffset.w = rot.value("w", 1.0f); // wはデフォルトで1（単位クォータニオン）にするのが無難だ
	}

	// LocalScaleを含む全項目の復元後にノード行列を確定する。
	// MeshColliderなど、Start時にglobalTransformを参照するコンポーネントへ正しい形状を渡すために必要。
	UpdateTransform();

}
void GltfModelCommponennt::ShowMeshDetailsUI()
{
	if (!ImGui::Begin("Mesh & Material Editor"))
	{
		ImGui::End();
		return;
	}

	for (auto& material : materials)
	{
		ImGui::PushID(material.id);

		std::string headerName_ =
			"Material [" + std::to_string(material.id) + "] : " + material.name;

		if (ImGui::CollapsingHeader(headerName_.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent();

			ImGui::SeparatorText("Shader");

			const char* shaderItems_[] =
			{
				"Basic",
				"PBR",
				"Toon",
				"ToonPBR"
			};

			int currentShaderIndex_ = static_cast<int>(material.shaderId);

			if (ImGui::Combo("Shader", &currentShaderIndex_, shaderItems_, IM_ARRAYSIZE(shaderItems_)))
			{
				material.shaderId = static_cast<ModelShaderID>(currentShaderIndex_);
			}

			ImGui::SeparatorText("Color");

			ImGui::ColorEdit4("Base Color", &material.baseColor.x);
			ImGui::ColorEdit3("Emissive Color", &material.emissiveColor.x);

			ImGui::SeparatorText("UV Transform");

			ImGui::DragFloat2("UV Scale", &material.uvScale.x, 0.01f);
			ImGui::DragFloat2("UV Offset", &material.uvOffset.x, 0.01f);
			ImGui::DragFloat("UV Rotation", &material.uvRotation, 0.01f);

			const char* uvAddressModeItems_[] = { "Wrap", "Clamp" };
			int currentUvAddressMode_ = static_cast<int>(material.uvAddressMode);
			if (ImGui::Combo("UV Address Mode", &currentUvAddressMode_, uvAddressModeItems_, IM_ARRAYSIZE(uvAddressModeItems_)))
			{
				material.uvAddressMode = static_cast<UV_ADDRESS_MODE>(currentUvAddressMode_);
			}

			ImGui::SeparatorText("PBR");

			ImGui::DragFloat("Metalness", &material.metalness, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Roughness", &material.roughness, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Occlusion Strength", &material.occlusionStrength, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Alpha Cutoff", &material.alphaCutoff, 0.001f, 0.0f, 1.0f);
			ImGui::DragFloat("Normal Scale", &material.normalScale, 0.001f, 0.0f, 3.0f);

			material.metalness = std::clamp(material.metalness, 0.0f, 1.0f);
			material.roughness = std::clamp(material.roughness, 0.0f, 1.0f);
			material.occlusionStrength = std::clamp(material.occlusionStrength, 0.0f, 1.0f);
			material.alphaCutoff = std::clamp(material.alphaCutoff, 0.0f, 1.0f);
			material.normalScale = std::clamp(material.normalScale, 0.0f, 3.0f);

			ImGui::SeparatorText("Alpha Mode");

			const char* alphaModeItems_[] =
			{
				"Opaque",
				"Mask",
				"Blend"
			};

			int currentAlphaModeIndex_ = static_cast<int>(material.alphaMode);

			if (ImGui::Combo("Alpha Mode", &currentAlphaModeIndex_, alphaModeItems_, IM_ARRAYSIZE(alphaModeItems_)))
			{
				material.alphaMode = static_cast<AlphaMode>(currentAlphaModeIndex_);
			}

			if (material.shaderId == ModelShaderID::ToonPBR)
			{
				ImGui::SeparatorText("Toon PBR");

				ImGui::DragFloat("Toon PBR Blend", &material.toonPbrBlend, 0.001f, 0.0f, 1.0f);
				material.toonPbrBlend = std::clamp(material.toonPbrBlend, 0.0f, 1.0f);
                // 拡散光と反射のToon化を分け、金属の光沢を残す。
                ImGui::SliderFloat("Specular Toon Blend", &material.specularToonBlend, 0.0f, 1.0f);
                ImGui::SliderFloat("Specular Strength", &material.specularStrength, 0.0f, 4.0f);
                ImGui::SliderFloat("Rim Strength", &material.rimStrength, 0.0f, 1.0f);
                ImGui::ColorEdit3("Shadow Tint", &material.shadowTint.x);
                // テクスチャや物性値を保ち、反射と影色の調整開始点だけを切り替える。
                if (ImGui::Button("Soft Toon"))
                {
                    material.toonPbrBlend = 1.0f;
                    material.specularToonBlend = 0.6f;
                    material.specularStrength = 0.35f;
                    material.rimStrength = 0.04f;
                    material.shadowTint = { 0.45f, 0.40f, 0.50f };
                }
                ImGui::SameLine();
                if (ImGui::Button("Metal"))
                {
                    material.toonPbrBlend = 1.0f;
                    material.specularToonBlend = 0.15f;
                    material.specularStrength = 1.2f;
                    material.rimStrength = 0.04f;
                    material.shadowTint = { 0.38f, 0.42f, 0.50f };
                }
                ImGui::SameLine();
                if (ImGui::Button("Scales"))
                {
                    material.toonPbrBlend = 0.9f;
                    material.specularToonBlend = 0.35f;
                    material.specularStrength = 0.8f;
                    material.rimStrength = 0.08f;
                    material.shadowTint = { 0.38f, 0.42f, 0.48f };
                }
                ImGui::SameLine();
                if (ImGui::Button("Wet"))
                {
                    material.toonPbrBlend = 0.85f;
                    material.specularToonBlend = 0.05f;
                    material.specularStrength = 1.4f;
                    material.rimStrength = 0.03f;
                    material.shadowTint = { 0.32f, 0.28f, 0.38f };
                }
			}

			if (material.shaderId == ModelShaderID::ToonPBR ||
				material.shaderId == ModelShaderID::Toon)
			{
				ImGui::SeparatorText("Outline");

				ImGui::Checkbox("Disable Outline", &material.disableOutline);
				ImGui::DragFloat("Outline Thickness Offset", &material.outlineThicknessOffset, 0.001f, 0.0f, 2.0f);

				material.outlineThicknessOffset =
					std::clamp(material.outlineThicknessOffset, 0.0f, 2.0f);
			}

			ImGui::SeparatorText("Texture Files");

			ImGui::Text("Base: %s", material.baseTextureFileName.c_str());
			ImGui::Text("Normal: %s", material.normalTextureFileName.c_str());
			ImGui::Text("Emissive: %s", material.emissiveTextureFileName.c_str());
			ImGui::Text("Occlusion: %s", material.occlusionTextureFileName.c_str());
			ImGui::Text("MetalRough: %s", material.metalnessRoughnessTextureFileName.c_str());

			ImGui::Unindent();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PopID();
	}

	ImGui::End();
}

void GltfModelCommponennt::ShowNodeTransformEditor()
{
	if (ImGui::Begin("Node Transform Editor"))
	{
		for (auto& node : nodes)
		{
			if (ImGui::TreeNode(node.name.c_str()))
			{
				// ノードのスケールを直接いじる（鎧のメッシュが属するノードを探せ）
				ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&node.scale), 0.01f, 0.1f, 2.0f);
				ImGui::DragFloat3("Position", reinterpret_cast<float*>(&node.position), 0.01f);

				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
}
// シリアライズ
void GltfModelCommponennt::Serialize(const char* filename)
{
    BS_PROFILE_SCOPE("GltfModelCommponennt::SerializeCerealFile");
	std::ofstream ostream(filename, std::ios::binary);
	if (ostream.is_open())
	{
		cereal::BinaryOutputArchive archive(ostream);

		try
		{
			archive(
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);
			// 既存バイナリの末尾へ追加し、古いモデルキャッシュの読み込みを維持する。
			nlohmann::json styleSettings_ = nlohmann::json::array();
			for (const auto& material_ : materials)
			{
			    nlohmann::json style_;
			    material_.SerializeStyleSettings(style_);
			    styleSettings_.push_back(style_);
			}
			const std::string styleText_ = styleSettings_.dump();
			archive(styleText_);
		}
		catch (...)
		{
			_ASSERT_EXPR_A(false, "Model serialize failed.");
		}
	}
}

// デシリアライズ
void GltfModelCommponennt::Deserialize(const char* filename)
{
	BS_PROFILE_SCOPE("GltfModelCommponennt::DeserializeCerealFile");
	std::ifstream istream(filename, std::ios::binary);
	if (istream.is_open())
	{
		cereal::BinaryInputArchive archive(istream);

		try
		{
			archive(
				CEREAL_NVP(nodes),
				CEREAL_NVP(materials),
				CEREAL_NVP(meshes),
				CEREAL_NVP(animations)
			);
			// 末尾設定のない旧形式ではMaterialの初期値をそのまま使う。
			if (istream.peek() != std::char_traits<char>::eof())
			{
			    std::string styleText_;
			    archive(styleText_);
			    const auto styleSettings_ = nlohmann::json::parse(styleText_);
			    if (styleSettings_.is_array())
			    {
			        for (size_t index_ = 0; index_ < materials.size() && index_ < styleSettings_.size(); ++index_)
			        {
			            materials[index_].DeserializeStyleSettings(styleSettings_[index_]);
			        }
			    }
			}
		}
		catch (...)
		{
			_ASSERT_EXPR_A(false, "Model deserialize failed.");
		}
	}
	else
	{
		_ASSERT_EXPR_A(false, "Model File not found.");
	}
}
