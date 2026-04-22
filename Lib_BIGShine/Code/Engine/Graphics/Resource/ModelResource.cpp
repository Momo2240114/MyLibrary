//#include <stdlib.h>
//#include <functional>
//#include <filesystem>
//#include <fstream>
//#include <cassert>
//#include <algorithm>
//
//#include <cereal/archives/binary.hpp> 
//#include <cereal/types/memory.hpp> 
//#include <cereal/types/vector.hpp> 
//#include <cereal/types/set.hpp> 
//#include <cereal/types/unordered_map.hpp> 
//
//#include "Misc.h"
//#include "ModelResource.h"
//#include "CPUResourceLoader.h"
//#include "GLTFImporter.h"
////
//
//using namespace DirectX;
//
//// static input layout description (matching Vertex in header)
//const std::vector<D3D11_INPUT_ELEMENT_DESC> ModelResource::InputElementDescs =
//{
//    { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//    { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//    { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // header had XMFLOAT3 tangent
//    { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//    { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//    { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//    { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
//};
//
//
//namespace DirectX
//{
//    template<class Archive>
//    void serialize(Archive& archive, XMUINT4& v)
//    {
//        archive(
//            cereal::make_nvp("x", v.x),
//            cereal::make_nvp("y", v.y),
//            cereal::make_nvp("z", v.z),
//            cereal::make_nvp("w", v.w)
//        );
//    }
//
//    template<class Archive>
//    void serialize(Archive& archive, XMFLOAT2& v)
//    {
//        archive(
//            cereal::make_nvp("x", v.x),
//            cereal::make_nvp("y", v.y)
//        );
//    }
//
//    template<class Archive>
//    void serialize(Archive& archive, XMFLOAT3& v)
//    {
//        archive(
//            cereal::make_nvp("x", v.x),
//            cereal::make_nvp("y", v.y),
//            cereal::make_nvp("z", v.z)
//        );
//    }
//
//    template<class Archive>
//    void serialize(Archive& archive, XMFLOAT4& v)
//    {
//        archive(
//            cereal::make_nvp("x", v.x),
//            cereal::make_nvp("y", v.y),
//            cereal::make_nvp("z", v.z),
//            cereal::make_nvp("w", v.w)
//        );
//    }
//
//    template<class Archive>
//    void serialize(Archive& archive, XMFLOAT4X4& m)
//    {
//        archive(
//            cereal::make_nvp("_11", m._11),
//            cereal::make_nvp("_12", m._12),
//            cereal::make_nvp("_13", m._13),
//            cereal::make_nvp("_14", m._14),
//            cereal::make_nvp("_21", m._21),
//            cereal::make_nvp("_22", m._22),
//            cereal::make_nvp("_23", m._23),
//            cereal::make_nvp("_24", m._24),
//            cereal::make_nvp("_31", m._31),
//            cereal::make_nvp("_32", m._32),
//            cereal::make_nvp("_33", m._33),
//            cereal::make_nvp("_34", m._34),
//            cereal::make_nvp("_41", m._41),
//            cereal::make_nvp("_42", m._42),
//            cereal::make_nvp("_43", m._43),
//            cereal::make_nvp("_44", m._44)
//        );
//    }
//}
//
//
//
//// Node
//template<class Archive>
//void ModelResource::Node::serialize(Archive& archive, int /*version*/)
//{
//    archive(name, name, path, parentIndex, scale, rotate, position);
//}
//
//// Material  ※alphaMode は整数として保存
//template<class Archive>
//void ModelResource::Material::serialize(Archive& archive, int /*version*/)
//{
//    int alpha = static_cast<int>(alphaMode);
//
//    archive(name,
//        baseTextureFileName,
//        normalTextureFileName,
//        emissiveTextureFileName,
//        occlusionTextureFileName,
//        metalnessRoughnessTextureFileName,
//        baseColor,
//        emissiveColor,
//        metalness,
//        roughness,
//        occlusionStrength,
//        alphaCutoff,
//        alpha
//    );
//
//    if constexpr (Archive::is_loading::value)
//        alphaMode = static_cast<AlphaMode>(alpha);
//}
//
//// Subset
//template<class Archive>
//void ModelResource::Subset::serialize(Archive& archive, int /*version*/)
//{
//    archive(startIndex, indexCount);
//    // materialIndexは mesh 側に置いてあるので不要
//}
//
//// Vertex
//template<class Archive>
//void ModelResource::Vertex::serialize(Archive& archive, int /*version*/)
//{
//    archive(position, normal, tangent, texcoord, color, boneWeight, boneIndex);
//}
//// Bone
//template<class Archive>
//void ModelResource::Bone::serialize(Archive& archive, int /*version*/)
//{
//    archive(nodeIndex, offsetTransform, SkinNodeIndex);
//}
//// Mesh
//template<class Archive>
//void ModelResource::Mesh::serialize(Archive& archive, int /*version*/)
//{
//    archive(
//        vertices,
//        indices,
//        subsets,
//        bones,
//        materialIndex,
//        nodeIndex,
//        nodeIndices,
//        offsetTransforms,
//        boundsMin,
//        boundsMax
//    );
//    // vertexBuffer / indexBuffer は再構築のため保存しない
//}
//
//// NodeKeyData
//template<class Archive>
//void ModelResource::NodeKeyData::serialize(Archive& archive, int /*version*/)
//{
//    archive(scale, rotate, translate);
//}
//
//// Keyframe
//template<class Archive>
//void ModelResource::Keyframe::serialize(Archive& archive, int /*version*/)
//{
//    archive(seconds, nodeKeys);
//}
//
//// Animation
//template<class Archive>
//void ModelResource::Animation::serialize(Archive& archive, int /*version*/)
//{
//    archive(name, secondsLength, keyframes);
//}
//
//
//
//static std::wstring ToWString(const std::string& s)
//{
//    return std::wstring(s.begin(), s.end());
//}
//
//void ModelResource::Load(ID3D11Device* device_, const char* filename_)
//{
//    namespace fs = std::filesystem;
//    fs::path filepath(filename_);
//    fs::path cerealPath = filepath;
//    cerealPath.replace_extension(".cereal");
//
//    // 1) まず独自バイナリ（.cereal）を探す
//    if (fs::exists(cerealPath))
//    {
//        // デシリアライズして GPU リソースを構築
//        Deserialize(cerealPath.string().c_str());
//        // dirname は元の gltf のあったディレクトリ相当（テクスチャ読み込み用）
//        BuildModel(device_, filepath.parent_path().string().c_str());
//        return;
//    }
//
//    // 2) .cereal が無ければ、もし入力が .gltf/.glb なら、
//    //    この段階では外部の GltfImporter を呼ぶか、ユーザー側で
//    //    事前に meshes/materials を埋めてから BuildModel を呼ぶことを想定。
//    const std::string ext = filepath.extension().string();
//    if (ext == ".gltf" || ext == ".glb")
//    {
//        // GLTF -> ModelResource へ変換するローダは別実装に委任する方針。
//        // ここではエラーではなく、ユーザーに GltfImporter を使って
//        // materials/meshes/nodes を埋めることを期待します。
//        // ただし簡易的に Serialize を作る場合はここで呼び出しても良い。
//        // For now, attempt to deserialize sidecar; if not present, fail assert.
//          // GLTF/GLB インポーターを作成
//        GLTFImporter importer(filename_);
//
//        // ノード読み込み
//        importer.LoadNodes(nodes);
//
//        // メッシュ読み込み
//        importer.LoadMeshes(meshes, nodes);
//
//        // マテリアル読み込み（必要に応じて D3D11Device を渡す）
//        importer.LoadMaterials(materials, device_);
//
//        // アニメーション読み込み
//        importer.LoadAnimations(animations, nodes, 60.0f);
//    }
//    else
//    {
//        // 既に独自フォーマット以外をロードする実装が無ければ失敗
//        assert(false && "ModelResource::Load: unsupported file or missing serialized resource.");
//    }
//}
//
//void ModelResource::BuildModel(ID3D11Device* device_, const char* dirname_)
//{
//    std::string dir(dirname_ ? dirname_ : ".");
//
//    // ===============================
//    // 1) マテリアルの GPU リソース構築
//    // ===============================
//    for (Material& mat : materials)
//    {
//        auto loadTex = [&](const std::string& filename, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv, UINT dummyColor)
//            {
//                if (!filename.empty())
//                {
//                    std::filesystem::path path = std::filesystem::path(dir) / filename;
//                    std::wstring wpath = path.wstring();
//                    HRESULT hr = CPUResourceLoader::LoadTextureFromFile(device_, wpath.c_str(), srv.GetAddressOf());
//                    if (SUCCEEDED(hr)) return;
//                }
//                CPUResourceLoader::CreateDummyTexture(device_, dummyColor, srv.GetAddressOf());
//            };
//
//        loadTex(mat.baseTextureFileName, mat.baseMap, 0xFFFFFFFF);
//        loadTex(mat.normalTextureFileName, mat.normalMap, 0xFFFF7F7F);
//        loadTex(mat.emissiveTextureFileName, mat.emissiveMap, 0x00000000);
//        loadTex(mat.occlusionTextureFileName, mat.occlusionMap, 0xFFFFFFFF);
//        loadTex(mat.metalnessRoughnessTextureFileName, mat.metalnessRoughnessMap, 0x8080FF);
//    }
//
//    // ===============================
//    // 2) ノードの親子関係構築
//    // ===============================
//    for (size_t i = 0; i < nodes.size(); ++i)
//    {
//        Node& node = nodes[i];
//        node.parent = node.parentIndex >= 0 ? &nodes[node.parentIndex] : nullptr;
//        if (node.parent) node.parent->children.push_back(&node);
//    }
//
//     // ====================================
//    // 3) メッシュ構築
//    // ====================================
//    for (Mesh& mesh : meshes)
//    {
//        // 材質・ノード紐付け
//        mesh.material = &materials.at(mesh.materialIndex);
//        mesh.node = &nodes.at(mesh.nodeIndex);
//
//        // 頂点バッファ
//        if (!mesh.vertices.empty())
//        {
//            D3D11_BUFFER_DESC bd{};
//            D3D11_SUBRESOURCE_DATA sd{};
//            bd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * mesh.vertices.size());
//            bd.Usage = D3D11_USAGE_IMMUTABLE;
//            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
//            sd.pSysMem = mesh.vertices.data();
//            device_->CreateBuffer(&bd, &sd, mesh.vertexBuffer.GetAddressOf());
//        }
//
//        // インデックスバッファ
//        if (!mesh.indices.empty())
//        {
//            D3D11_BUFFER_DESC bd{};
//            D3D11_SUBRESOURCE_DATA sd{};
//            bd.ByteWidth = static_cast<UINT>(sizeof(UINT) * mesh.indices.size());
//            bd.Usage = D3D11_USAGE_IMMUTABLE;
//            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
//            sd.pSysMem = mesh.indices.data();
//            device_->CreateBuffer(&bd, &sd, mesh.indexBuffer.GetAddressOf());
//        }
//
//        // ボーン紐付け
//        for (Bone& bone : mesh.bones)
//        {
//            if (bone.nodeIndex >= 0 && bone.nodeIndex < nodes.size())
//                bone.node = &nodes[bone.nodeIndex];
//        }
//
//        // Subset のマテリアル紐付け
//        for (Subset& subset : mesh.subsets)
//        {
//            if (subset.material == nullptr)
//            {
//                subset.material = mesh.material;
//            }
//        }
//
//        // バウンディングボックス計算
//        if (!mesh.vertices.empty())
//        {
//            XMFLOAT3 minv = mesh.vertices[0].position;
//            XMFLOAT3 maxv = mesh.vertices[0].position;
//            for (auto& v : mesh.vertices)
//            {
//                minv.x = (std::min)(minv.x, v.position.x);
//                minv.y = (std::min)(minv.y, v.position.y);
//                minv.z = (std::min)(minv.z, v.position.z);
//
//                maxv.x = (std::max)(maxv.x, v.position.x);
//                maxv.y = (std::max)(maxv.y, v.position.y);
//                maxv.z = (std::max)(maxv.z, v.position.z);
//            }
//            mesh.boundsMin = minv;
//            mesh.boundsMax = maxv;
//        }
//    }
//
//
//}
//
//void ModelResource::Serialize(const char* filename_)
//{
//    std::ofstream os(filename_, std::ios::binary);
//    if (!os.is_open())
//    {
//        assert(false && "Failed to open file for serialize");
//        return;
//    }
//
//    cereal::BinaryOutputArchive archive(os);
//    archive(nodes, materials, meshes, animations);
//}
//
//void ModelResource::Deserialize(const char* filename_)
//{
//    std::ifstream is(filename_, std::ios::binary);
//    if (!is.is_open())
//    {
//        assert(false && "Failed to open file for deserialize");
//        return;
//    }
//
//    cereal::BinaryInputArchive archive(is);
//    archive(nodes, materials, meshes, animations);
//}
