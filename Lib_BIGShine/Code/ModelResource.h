#pragma once
//
//#include <string>
//#include <vector>
//#include <wrl.h>
//#include <d3d11.h>
//#include <DirectXMath.h>
//
//class ModelResource
//{
//public:
//	ModelResource() {}
//	virtual ~ModelResource() {}
//
//	static const std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDescs;
//
//	using NodeId = UINT64;
//
//	struct Node
//	{
//		NodeId				id;
//		std::string			name;
//		std::string			path;
//		int					parentIndex;
//		DirectX::XMFLOAT3	scale;
//		DirectX::XMFLOAT4	rotate;
//		DirectX::XMFLOAT3	position;
//
//		// BuildModel 用に追加
//		Node* parent = nullptr;
//		std::vector<Node*> children;
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};
//
//	enum class AlphaMode
//	{
//		Opaque,
//		Mask,
//		Blend
//	};
//
//	struct Material
//	{
//		std::string			name;
//		std::string baseTextureFileName;
//		std::string normalTextureFileName;
//		std::string emissiveTextureFileName;
//		std::string occlusionTextureFileName;
//		std::string metalnessRoughnessTextureFileName;
//		DirectX::XMFLOAT4 baseColor = { 1,1,1,1 };
//		DirectX::XMFLOAT3 emissiveColor = { 1,1,1 };
//		float metalness = 0.0f;
//		float roughness = 0.0f;
//		float occlusionStrength = 0.0f;
//		float alphaCutoff = 0.5f;
//		AlphaMode			alphaMode = AlphaMode::Opaque;
//
//		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	baseMap;
//		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	normalMap;
//		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	emissiveMap;
//		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	occlusionMap;
//		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	metalnessRoughnessMap;
//
//		template<class Archive>
//		void serialize(Archive& archive, int version);
//	};
//
//	struct Subset
//	{
//		UINT		startIndex = 0;
//		UINT		indexCount = 0;
//
//		Material* material = nullptr;
//
//		template<class Archive>
//		void serialize(Archive& archive, int version);
//	};
//
//	struct Vertex
//	{
//		DirectX::XMFLOAT3	position = { 0, 0, 0 };
//		DirectX::XMFLOAT3	normal = { 0, 0, 0 };
//		DirectX::XMFLOAT4	tangent = { 0, 0, 0, 1 };
//		DirectX::XMFLOAT2	texcoord = { 0, 0 };
//		DirectX::XMFLOAT4	color = { 1, 1, 1, 1 };
//		DirectX::XMFLOAT4	boneWeight = { 1, 0, 0, 0 };
//		DirectX::XMUINT4	boneIndex = { 0, 0, 0, 0 };
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};
//
//	struct Bone
//	{
//		int nodeIndex;
//		DirectX::XMFLOAT4X4 offsetTransform;
//		Node* node = nullptr;
//		int SkinNodeIndex = -1;
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};
//
//
//
//	struct Mesh
//	{
//		std::vector<Vertex>						vertices;
//		std::vector<UINT>						indices;
//		std::vector<Subset>						subsets;
//		std::vector<Bone>bones;
//		int			materialIndex = 0;
//		int										nodeIndex;
//		std::vector<int>						nodeIndices;
//		std::vector<DirectX::XMFLOAT4X4>		offsetTransforms;
//
//		DirectX::XMFLOAT3						boundsMin;
//		DirectX::XMFLOAT3						boundsMax;
//
//		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
//		Microsoft::WRL::ComPtr<ID3D11Buffer>	indexBuffer;
//
//		Material* material = nullptr;
//		Node* node = nullptr;
//
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};
//
//	struct NodeKeyData
//	{
//		DirectX::XMFLOAT3	scale;
//		DirectX::XMFLOAT4	rotate;
//		DirectX::XMFLOAT3	translate;
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};
//
//	struct Keyframe
//	{
//		float						seconds;
//		std::vector<NodeKeyData>	nodeKeys;
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};
//
//	struct Animation
//	{
//		std::string					name;
//		float						secondsLength;
//		std::vector<Keyframe>		keyframes;
//
//		template<class Archive>
//		void serialize(Archive& archive_, int version_);
//	};		
//
//
//	struct NodePose
//	{
//		DirectX::XMFLOAT3 position = { 0,0,0 };
//		DirectX::XMFLOAT4 rotation = { 0,0,0,1 };
//		DirectX::XMFLOAT3 scale = { 1,1,1 };
//	};
//
//	// 各種データ取得
//	const std::vector<Mesh>& GetMeshes() const { return meshes; }
//	const std::vector<Node>& GetNodes() const { return nodes; }
//	const std::vector<Animation>& GetAnimations() const { return animations; }
//	const std::vector<Material>& GetMaterials() const { return materials; }
//
//	// 読み込み
//	void Load(ID3D11Device* device_, const char* filename_);
//
//	DirectX::XMMATRIX GetWorldMatrix(int nodeIndex) const
//	{
//		const Node& node = nodes[nodeIndex];
//		DirectX::XMMATRIX local =
//			DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z) *
//			DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotate)) *
//			DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
//
//		if (node.parentIndex >= 0)
//			return local * GetWorldMatrix(node.parentIndex);
//		else
//			return local;
//	}
//protected:
//	// モデルセットアップ
//	void BuildModel(ID3D11Device* device_, const char* dirname_);
//
//	// シリアライズ
//	void Serialize(const char* filename_);
//
//	// デシリアライズ
//	void Deserialize(const char* filename_);
//
//protected:
//	std::vector<Node>		nodes;
//	std::vector<Material>	materials;
//	std::vector<Mesh>		meshes;
//	std::vector<Animation>	animations;
//};
