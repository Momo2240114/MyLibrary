#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <wrl.h>
#include <d3d11.h>
#include <imgui.h>
#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/Math/Vector.h"
#include "Engine/Graphics/Shader/ModelShaderID.h"
#include "Engine/Graphics/Core/RenderContext.h"

// 前方宣言だけでよいもの
//class ModelResource;

class GltfModelCommponennt : public Component
{
public:
	GltfModelCommponennt() = default;

	const std::string& GetName() const override
	{
		static std::string name = "GltfModel";
		return name;
	}

	void LoadModel(ID3D11Device* device, const char* filename, float sampleRate = 60);

	void Gui()override;

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;

	std::string GetModelName()
	{
		return modelName;
	};

	void Update(float ETime)
	{
		UpdateTransform();
	}

	std::string& GetRootMotionName()
	{
		return m_RootMotionName;
	}

	void RebindInternalPointers();

	struct Node
	{
		std::string			name;
		int					parentIndex = -1;
		DirectX::XMFLOAT3	position = { 0, 0, 0 };
		DirectX::XMFLOAT4	rotation = { 0, 0, 0, 1 };
		DirectX::XMFLOAT3	scale = { 1, 1, 1 };

		DirectX::XMFLOAT4X4	localTransform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		DirectX::XMFLOAT4X4	globalTransform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		DirectX::XMFLOAT4X4	worldTransform = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

		Node* parent = nullptr;
		std::vector<Node*>	children;


		template<class Archive>
		void serialize(Archive& archive);
	};

	enum class AlphaMode
	{
		Opaque,
		Mask,
		Blend
	};

	struct Material
	{
		int id; //マテリアルを一意に識別するためのID
		std::string			name;
		std::string			baseTextureFileName;
		std::string			normalTextureFileName;
		std::string			emissiveTextureFileName;
		std::string			occlusionTextureFileName;
		std::string			metalnessRoughnessTextureFileName;
		DirectX::XMFLOAT4	baseColor = { 1, 1, 1, 1 };
		DirectX::XMFLOAT3	emissiveColor = { 1, 1, 1 };
		float				metalness = 0.0f;
		float				roughness = 0.0f;
		float				occlusionStrength = 0.0f;
		float				alphaCutoff = 0.5f;
		float				toonPbrBlend = 0.5f;
		AlphaMode			alphaMode = AlphaMode::Opaque;
		ModelShaderID shaderId = ModelShaderID::ToonPBR;


		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	baseMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	normalMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	emissiveMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	occlusionMap;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	metalnessRoughnessMap;

		// ★追加するトゥーン・アウトライン用パラメータ
		bool disableOutline = false;    // 目などでアウトラインを消すためのフラグ
		float outlineThicknessOffset = 1.0f; // マテリアルごとの太さ倍率

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Vertex
	{
		DirectX::XMFLOAT3		position = { 0, 0, 0 };
		DirectX::XMFLOAT3		normal = { 0, 0, 0 };
		DirectX::XMFLOAT4		tangent = { 0, 0, 0, 1 };
		DirectX::XMFLOAT2		texcoord = { 0, 0 };
		DirectX::XMFLOAT4		boneWeight = { 1, 0, 0, 0 };
		DirectX::XMUINT4		boneIndex = { 0, 0, 0, 0 };

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Bone
	{
		int						nodeIndex;
		DirectX::XMFLOAT4X4		offsetTransform;
		Node* node = nullptr;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Mesh
	{
		std::vector<Vertex>		vertices;
		std::vector<uint32_t>	indices;
		std::vector<Bone>		bones;
		int			nodeIndex = 0;
		int			materialIndex = 0;
		Material* material = nullptr;
		Node* node = nullptr;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct VectorKeyframe
	{
		float					seconds;
		DirectX::XMFLOAT3		value;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct QuaternionKeyframe
	{
		float					seconds;
		DirectX::XMFLOAT4		value;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct NodeAnim
	{
		std::vector<VectorKeyframe>		positionKeyframes;
		std::vector<QuaternionKeyframe>	rotationKeyframes;
		std::vector<VectorKeyframe>		scaleKeyframes;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct Animation
	{
		std::string					name;
		float						secondsLength;
		std::vector<NodeAnim>		nodeAnims;

		template<class Archive>
		void serialize(Archive& archive);
	};

	struct NodePose
	{
		DirectX::XMFLOAT3	position = { 0, 0, 0 };
		Vector4	rotation = { 0, 0, 0, 1 };
		DirectX::XMFLOAT3	scale = { 1, 1, 1 };
	};

	// アニメーション追加読み込み
	void AppendAnimations(const char* filename);

	// マテリアルデータ取得
	const std::vector<Material>& GetMaterials() const { return materials; }

	// メッシュデータ取得
	const std::vector<Mesh>& GetMeshes() const { return meshes; }

	// アニメーションデータ取得
	const std::vector<Animation>& GetAnimations() const { return animations; }

	// アニメーションインデックス取得
	int GetAnimationIndex(const char* name) const;

	// ノードデータ取得
	const std::vector<Node>& GetNodes() const { return nodes; }


	std::vector<Node>& GetNodes() { return nodes; }

	Node& GetNodes(std::string nodeName);

	// ルートノード取得
	Node* GetRootNode() { return nodes.data(); }

	// ノードインデックス取得
	int GetNodeIndex(const char* name) const;

	void UpdateNodeRecursive(Node& node, const DirectX::XMMATRIX& ParentWorldTransform, const DirectX::XMMATRIX& ModelScaleMat);
	// トランスフォーム更新処理
	void UpdateTransform();

	void UpdateNodeTransformFrom(int nodeIndex_);

	// アニメーション計算
	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const;

	// ノードポーズ設定
	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	// ノードポーズ取得
	void GetNodePoses(std::vector<NodePose>& nodePoses) const;

	float GetAnimationDuration(int animationIndex) const;

	DirectX::XMMATRIX ToMatrix(const NodePose& pose)
	{
		using namespace DirectX;
		XMVECTOR scale = XMLoadFloat3(&pose.scale);
		XMVECTOR rotation = XMLoadFloat4(&pose.rotation);
		XMVECTOR translation = XMLoadFloat3(&pose.position);

		return XMMatrixScalingFromVector(scale)
			* XMMatrixRotationQuaternion(rotation)
			* XMMatrixTranslationFromVector(translation);
	}
	// 既存のモデルデータをすべて解放する
	void Clear() {
		materials.clear();
		meshes.clear();
		nodes.clear();
		animations.clear();
		modelName = "";
	}

	void ShowModelNodePositionsUI();

	void ShowNodePoseEditor(NodePose& pose)
	{
		ImGui::Text("Position");
		ImGui::DragFloat3("##Position", reinterpret_cast<float*>(&pose.position), 0.1f);

		ImGui::Text("Rotation (Quat)");
		ImGui::DragFloat4("##Rotation", reinterpret_cast<float*>(&pose.rotation), 0.1f);

		ImGui::Text("Scale");
		ImGui::DragFloat3("##Scale", reinterpret_cast<float*>(&pose.scale), 0.1f);
	}

	void ShowNodeDetailsUI(const Node* node);

	ModelShaderID  GetShaderID() { return shaderId; };
	void SetShaderID(ModelShaderID id_) { shaderId = id_; }

	void Serialize(nlohmann::json& j) const override;

	void Deserialize(const nlohmann::json& j) override;

	bool GetUsedNaviMeshSystemFlg() { return usedNaviMeshSystemFlg; }

	void SetLocalScale(float scale)
	{
		localScale = scale;
	}

	void ShowMeshDetailsUI();
	void ShowNodeTransformEditor();
	float GetLocalScale() {return localScale;}
	DirectX::SimpleMath::Quaternion GetRootRotationOffset() { return _RootRotationOffset; }
private:
	// シリアライズ
	void Serialize(const char* filename);

	// デシリアライズ
	void Deserialize(const char* filename);

private:
	ModelShaderID shaderId = ModelShaderID::PBR;
	std::string modelName = "";
	std::string modelFullPass = "";
	bool usedNaviMeshSystemFlg = false;
	std::vector<Material>	materials;
	std::vector<Mesh>		meshes;
	std::vector<Node>		nodes;
	std::vector<Animation>	animations;


	float localScale = 1.0;
	DirectX::SimpleMath::Quaternion _RootRotationOffset;
	std::string m_RootMotionName;
};
