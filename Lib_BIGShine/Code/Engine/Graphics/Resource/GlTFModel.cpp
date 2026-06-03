#include "GltfModel.h"
#include "Engine/Graphics/RenderSystem/ModelRenderSystem.h"
#include <filesystem>
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include "Misc.h"
#include "GLTFImporter.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "ModelResourceManager.h"
#include "Model.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"

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
		cereal::make_nvp("alphaMode", tempAlphaMode),
		cereal::make_nvp("shaderId", tempShaderId)
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
	Clear();

	// 1. 名前の記憶
	std::filesystem::path filePath(filename);
	modelFullPass = filename;
	modelName = filePath.stem().string();

	// 2. 神殿からすでにGPUバッファ等も構築済みのリソースを得る
	auto resource = ModelResourceManager::Instance().GetOrCreateResource(device, filename);
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
	RebindInternalPointers();

	// 5. 行列の確定
	UpdateTransform();
}

void GltfModelCommponennt::RebindInternalPointers()
{
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
	ImGui::DragFloat("localScale", &localScale, 0.01f,0.0f,100.0f);
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
	ShowNodeTransformEditor();
}

// アニメーション追加読み込み
void GltfModelCommponennt::AppendAnimations(const char* filename)
{
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

void GltfModelCommponennt::UpdateNodeRecursive(Node& node, const DirectX::XMMATRIX& ParentWorldTransform, const DirectX::XMMATRIX& ModelScaleMat)
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
	// 1. モデルが置かれているGameObjectのワールド行列
	DirectX::XMMATRIX ParentWorldTransform = this->GetGameObject()->transform->worldMatrix;

	// 2. モデル全体の補正スケール（一箇所だけで適用するように注意）
	DirectX::XMMATRIX ModelScaleMat = DirectX::XMMatrixScaling(localScale, localScale, localScale);

	// 3. 全てのルートノード（親がいないノード）から再帰を開始
	for (Node& node : nodes)
	{
		if (node.parent == nullptr)
		{
			UpdateNodeRecursive(node, ParentWorldTransform, ModelScaleMat);
		}
	}
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

float GltfModelCommponennt::GetAnimationDuration(int animationIndex) const
{
	const Animation& animation = animations.at(animationIndex);

	float maxDuration = 0.0f;

	for (const NodeAnim& nodeAnim : animation.nodeAnims)
	{
		if (!nodeAnim.positionKeyframes.empty())
		{
			float end = nodeAnim.positionKeyframes.back().seconds;
			maxDuration = max(maxDuration, end);
		}
		if (!nodeAnim.rotationKeyframes.empty())
		{
			float end = nodeAnim.rotationKeyframes.back().seconds;
			maxDuration = max(maxDuration, end);
		}
		if (!nodeAnim.scaleKeyframes.empty())
		{
			float end = nodeAnim.scaleKeyframes.back().seconds;
			maxDuration = max(maxDuration, end);
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
			{"shaderId", static_cast<int>(mat.shaderId)},
			{"baseColor", {mat.baseColor.x, mat.baseColor.y, mat.baseColor.z, mat.baseColor.w}}
			});
	}
	j["MaterialOverrides"] = materialSettings;

	j["ModelName"] = pathForSave;
	j["ShaderID"] = static_cast<int>(shaderId);
	j["LocalScale"] = localScale;
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
	if (j.contains("ShaderID")) {
		shaderId = static_cast<ModelShaderID>(j["ShaderID"].get<int>());
	}

	if (j.contains("ModelName")) {
		std::string name = j["ModelName"];
		if (!name.empty()) {
			// パス正規化（重要）
			if (name.substr(0, 2) == "./") name = name.substr(2);
			std::replace(name.begin(), name.end(), '/', '\\');

			auto device = GraphicsDeviceManager::Instance().GetDevice();

			// 1. モデルをロード
			LoadModel(device, name.c_str());

			// 2. このコンポーネントが持っているモデルデータを 
			//    現在の Transform（Loadで復元済み）に強制同期させる
			UpdateTransform();
		}
	}

	if (j.contains("MaterialOverrides")) {
		for (const auto& item : j["MaterialOverrides"]) {
			int id = item["id"];
			// IDをキーにして、現在のmaterials配列から該当するものを探して上書きする
			for (auto& mat : materials) {
				if (mat.id == id) {
					mat.shaderId = static_cast<ModelShaderID>(item["shaderId"]);
					// 色情報なども同様に反映
				}
			}
		}
	}

	if (j.contains("LocalScale")) {
		localScale = static_cast<float>(j["LocalScale"].get<float>());
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

}

void GltfModelCommponennt::ShowMeshDetailsUI()
{
	if (ImGui::Begin("Mesh & Material Editor"))
	{
		// 1. ユニークなマテリアルを特定する（または事前に用意したマテリアル配列を使う）
			// もしマテリアルが配列として管理されているなら、それを使うのが一番じゃ
		for (auto& material : materials)
		{
			ImGui::PushID(material.id); // マテリアル固有のIDで区別

			ImGui::Text("Material: %s", material.name.c_str());

			// シェーダーの選択用コンボボックス
			const char* shaderItems[] = { "Basic", "PBR", "Toon" ,"ToonPBR"};
			int currentShaderIdx = static_cast<int>(material.shaderId);

			if (ImGui::Combo("Shader", &currentShaderIdx, shaderItems, IM_ARRAYSIZE(shaderItems)))
			{
				material.shaderId = static_cast<ModelShaderID>(currentShaderIdx);
			}

			ImGui::Separator();
			ImGui::PopID();
		}
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
