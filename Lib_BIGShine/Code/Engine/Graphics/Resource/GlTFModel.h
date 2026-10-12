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
#include "Engine/Graphics/Resource/ModelLoadTypes.h"

// 前方宣言だけでよいもの
class ModelResource;

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
	void RequestLoadModelAsync(const char* filename_, int priority_ = static_cast<int>(PRELOAD_PRIORITY::NORMAL));
	void UpdateLoadState() override;
	bool IsLoadReady() const override;
	bool IsRuntimeReady() const override;
	bool HasRuntimeFailure() const override;
    // 影渡りなどの一時的な非表示を、アニメーション更新と保存設定から分離する。
    void SetRuntimeVisible(bool visible_) { runtimeVisible = visible_; }
    bool IsRuntimeVisible() const { return runtimeVisible; }
	bool IsModelReady() const;
	bool IsModelLoading() const;

	void Gui()override;

	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;

	std::string GetModelName()
	{
		return modelName;
	};

	void Update(float elapsedTime_) override
	{
		(void)elapsedTime_;
		UpdateLoadState();
		if (!IsLoadReady()) return;
		if (transformUpdateDrivenExternally) return;

		if (isStaticModel)
		{
			UpdateTransformIfDirty();
			return;
		}

		UpdateTransform();
	}

	std::string& GetRootMotionName()
	{
		return m_RootMotionName;
	}

	void RebindInternalPointers();
	void ApplyLoadedResource(const std::shared_ptr<ModelResource>& resource_);

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

	enum class UV_ADDRESS_MODE
	{
		WRAP,
		CLAMP
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
		float				normalScale = 1.0f;
		DirectX::XMFLOAT2	uvScale = { 1.0f, 1.0f };
		DirectX::XMFLOAT2	uvOffset = { 0.0f, 0.0f };
		float				uvRotation = 0.0f;
		UV_ADDRESS_MODE		uvAddressMode = UV_ADDRESS_MODE::WRAP;

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
		std::vector<DirectX::XMFLOAT4X4> cachedSkinningTransforms;
		bool skinningTransformDirty = true;

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
	void UpdateNodeRecursive(
		Node& node_,
		const DirectX::XMMATRIX& parentGlobalTransform_,
		const DirectX::XMMATRIX& parentWorldTransform_,
		const DirectX::XMMATRIX& modelScaleMat_,
		bool isRoot_,
		bool updateChildren_ = true);

	void MarkPoseDirty();
	void MarkTransformDirty();
	void UpdateTransformIfDirty();
	const std::vector<DirectX::XMFLOAT4X4>& GetSkinningTransforms(int meshIndex_);
    // 現在のSkinning行列から少量のワールド頂点を採取し、構図と泥の発生位置へ共有する。
    void CollectWorldVertexSamples(std::vector<DirectX::XMFLOAT3>& positions_, std::size_t maximumCount_ = 1024);


	// トランスフォーム更新処理
	void UpdateTransform();

	void UpdateNodeTransformFrom(int nodeIndex_);
	void UpdateNodeTransformOnly(int nodeIndex_);
	void UpdateNodeTransformFrom(
		int nodeIndex_,
		const DirectX::XMMATRIX& parentWorldTransform_,
		const DirectX::XMMATRIX& modelScaleMat_);

	// アニメーション計算
	void ComputeAnimation(int animationIndex, int nodeIndex, float time, NodePose& nodePose) const;
	void ComputeAnimation(int animationIndex, float time, std::vector<NodePose>& nodePoses) const;

	// ノードポーズ設定
	void SetNodePoses(const std::vector<NodePose>& nodePoses);

	// ノードポーズ取得
	void GetNodePoses(std::vector<NodePose>& nodePoses) const;
        const NodePose& GetBindNodePose(int nodeIndex_) const;

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
                bindNodePoses.clear();
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

	void SetLocalScale(float scale_)
	{
		if (localScale == scale_ &&
			localScaleXYZ.x == scale_ &&
			localScaleXYZ.y == scale_ &&
			localScaleXYZ.z == scale_) return;
		localScale = scale_;
		localScaleXYZ = { scale_, scale_, scale_ };
		MarkTransformDirty();
	}
	void SetLocalScale(const DirectX::XMFLOAT3& scale_)
	{
		if (localScaleXYZ.x == scale_.x &&
			localScaleXYZ.y == scale_.y &&
			localScaleXYZ.z == scale_.z) return;
		localScaleXYZ = scale_;
		localScale = scale_.x;
		MarkTransformDirty();
	}
	bool IsStaticModel() const { return isStaticModel; }
	void SetStaticModel(bool isStaticModel_)
	{
		if (isStaticModel == isStaticModel_) return;
		isStaticModel = isStaticModel_;
		MarkTransformDirty();
	}
	void SetTransformUpdateDrivenExternally(bool drivenExternally_)
	{
		transformUpdateDrivenExternally = drivenExternally_;
	}

	void ShowMeshDetailsUI();
	void ShowNodeTransformEditor();
	float GetLocalScale() {return localScale;}
	const DirectX::XMFLOAT3& GetLocalScaleXYZ() const { return localScaleXYZ; }
	DirectX::SimpleMath::Quaternion GetRootRotationOffset() { return _RootRotationOffset; }
private:
	// シリアライズ
	void Serialize(const char* filename);

	// デシリアライズ
	void Deserialize(const char* filename);

private:
	DirectX::XMMATRIX MakeLocalScaleMatrix() const;
	ModelShaderID shaderId = ModelShaderID::PBR;
	std::string modelName = "";
	std::string modelFullPass = "";
	ModelLoadHandle modelLoadHandle;
	std::vector<nlohmann::json> pendingMaterialOverrides;
	bool asyncLoadRequested = false;
	bool asyncLoadApplied = false;
	bool usedNaviMeshSystemFlg = false;
	std::vector<Material>	materials;
	std::vector<Mesh>		meshes;
	std::vector<Node>		nodes;
	std::vector<NodePose>	bindNodePoses;
	std::vector<Animation>	animations;

    bool runtimeVisible = true; // 実行中だけの描画状態。Serializeの対象にしない。
	bool poseDirty = true;
	bool transformDirty = true;
	bool isStaticModel = false;
	bool transformUpdateDrivenExternally = false;
	bool hasCachedOwnerWorldTransform = false;
	DirectX::XMFLOAT4X4 cachedOwnerWorldTransform{};

	float localScale = 1.0;
	DirectX::XMFLOAT3 localScaleXYZ = { 1.0f, 1.0f, 1.0f };
	DirectX::SimpleMath::Quaternion _RootRotationOffset;
	std::string m_RootMotionName;

	void MarkSkinningTransformsDirty();
	void UpdateSkinningTransforms(int meshIndex_);

};
