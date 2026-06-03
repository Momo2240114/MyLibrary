#include "ModelResourceManager.h"
#include "GLTFImporter.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Misc.h"
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

// 記憶の神殿からリソースを乞う、あるいは新たに紡ぐ関数
std::shared_ptr<ModelResource> ModelResourceManager::GetOrCreateResource(ID3D11Device* device, const std::string& filename, float sampleRate)
{
    // 1. パスの深淵から、純粋なる「真名（Model名）」を抽出する
    std::filesystem::path filePath(filename);
    std::string pureModelName = filePath.stem().string();

    // 2. すでにその名が記憶に刻まれているか確認
    auto it = m_resources.find(pureModelName);
    if (it != m_resources.end()) {
        // 「おお、そなたの探す記憶（GPUバッファ等も構築済み）はここにある！」
        return it->second;
    }

    // 3. 記憶になければ、新たなリソースの器を錬成する
    auto newResource = std::make_shared<ModelResource>();
    newResource->modelName = pureModelName;

    std::filesystem::path dirpath(filePath.parent_path());
    std::filesystem::path extension = filePath.extension();

    // 独自形式 (.cereal) のパスを生成
    std::filesystem::path cerealPath = filePath;
    cerealPath.replace_extension(".cereal");

    bool isLoaded = false;

    // 4. 独自形式のモデルファイルの存在確認と読み込み
    if (std::filesystem::exists(cerealPath))
    {
        isLoaded = LoadFromCereal(cerealPath.string(), newResource);
    }

    // 独自形式がない、または読み込みに失敗した場合はGLTFから紡ぎ出す
    if (!isLoaded && (extension == ".gltf" || extension == ".glb"))
    {
        GLTFImporter importer(filename.c_str());

        // 汎用データ読み取り
        importer.LoadMaterials(newResource->materials, device);
        importer.LoadNodes(newResource->nodes);
        importer.LoadMeshes(newResource->meshes, newResource->nodes);
        importer.LoadAnimations(newResource->animations, newResource->nodes, sampleRate);

        // 独自形式のモデルファイルを保存して未来に備える
        SaveToCereal(cerealPath.string(), newResource);
        isLoaded = true;
    }

    if (!isLoaded)
    {
        _ASSERT_EXPR_A(false, "対応するモデルファイルが見つかりませんでした。");
        return nullptr;
    }

    // ========================================================
    // ★ ここからコンポーネントから移籍せし「聖なる構築儀式」
    // ========================================================

    // --- [A] マテリアル構築（テクスチャのロード・ダミー生成） ---
    for (GltfModelCommponennt::Material& material : newResource->materials)
    {
        // ベースマップ
        if (material.baseMap == nullptr) {
            if (material.baseTextureFileName.empty()) {
                HRESULT hr = CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFF, material.baseMap.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            }
            else {
                std::filesystem::path diffuseTexturePath(dirpath / material.baseTextureFileName);
                HRESULT hr = CPUResourceLoader::LoadTextureFromFile(device, diffuseTexturePath.wstring().c_str(), material.baseMap.GetAddressOf());
                _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
            }
        }
        // 法線マップ：光を操る「データ」である
        if (material.normalMap == nullptr) {
            if (material.normalTextureFileName.empty()) {
                CPUResourceLoader::CreateDummyTexture(device, 0xFFFF7F7F, material.normalMap.GetAddressOf());
            }
            else {
                std::filesystem::path texturePath(dirpath / material.normalTextureFileName);
                CPUResourceLoader::LoadTextureFromFile(device, texturePath.wstring().c_str(),
                    material.normalMap.GetAddressOf(), nullptr,
                    TextureType::Data); // データの理
            }
        }

        // エミッシブマップ：発光の「色」である
        if (material.emissiveMap == nullptr) {
            if (material.emissiveTextureFileName.empty()) {
                CPUResourceLoader::CreateDummyTexture(device, 0x00000000, material.emissiveMap.GetAddressOf());
            }
            else {
                std::filesystem::path path = dirpath / material.emissiveTextureFileName;
                CPUResourceLoader::LoadTextureFromFile(device, path.wstring().c_str(),
                    material.emissiveMap.GetAddressOf(), nullptr,
                    TextureType::Color); // 色の理
            }
        }

        // オクルージョンマップ：遮蔽の「データ」である
        if (material.occlusionMap == nullptr) {
            if (material.occlusionTextureFileName.empty()) {
                CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFF, material.occlusionMap.GetAddressOf());
            }
            else {
                std::filesystem::path path = dirpath / material.occlusionTextureFileName;
                CPUResourceLoader::LoadTextureFromFile(device, path.wstring().c_str(),
                    material.occlusionMap.GetAddressOf(), nullptr,
                    TextureType::Data); // データの理
            }
        }

        // MetalnessRoughnessマップ：材質の特性を決める極めて重要な「データ」である
        if (material.metalnessRoughnessMap == nullptr) {
            if (material.metalnessRoughnessTextureFileName.empty()) {
                CPUResourceLoader::CreateDummyTexture(device, 0x008080FF, material.metalnessRoughnessMap.GetAddressOf());
            }
            else {
                std::filesystem::path path = dirpath / material.metalnessRoughnessTextureFileName;
                CPUResourceLoader::LoadTextureFromFile(device, path.wstring().c_str(),
                    material.metalnessRoughnessMap.GetAddressOf(), nullptr,
                    TextureType::Data); // データの理
            }
        }
    }

    // --- [B] ノード構築（神殿内での親子ポインタ結線） ---
    for (size_t nodeIndex = 0; nodeIndex < newResource->nodes.size(); ++nodeIndex)
    {
        GltfModelCommponennt::Node& node = newResource->nodes.at(nodeIndex);
        node.parent = node.parentIndex >= 0 ? &newResource->nodes.at(node.parentIndex) : nullptr;
        if (node.parent != nullptr) {
            node.parent->children.emplace_back(&node);
        }
    }

    // --- [C] メッシュ構築） ---
    for (GltfModelCommponennt::Mesh& mesh : newResource->meshes)
    {
        mesh.material = &newResource->materials.at(mesh.materialIndex);
        mesh.node = &newResource->nodes.at(mesh.nodeIndex);

        // 頂点バッファの構築
        {
            D3D11_BUFFER_DESC bufferDesc = {};
            D3D11_SUBRESOURCE_DATA subresourceData = {};

            bufferDesc.ByteWidth = static_cast<UINT>(sizeof(GltfModelCommponennt::Vertex) * mesh.vertices.size());
            bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;
            bufferDesc.StructureByteStride = 0;
            subresourceData.pSysMem = mesh.vertices.data();

            HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.vertexBuffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

        // インデックスバッファの構築
        {
            D3D11_BUFFER_DESC bufferDesc = {};
            D3D11_SUBRESOURCE_DATA subresourceData = {};

            bufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());
            bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
            bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            bufferDesc.CPUAccessFlags = 0;
            bufferDesc.MiscFlags = 0;
            bufferDesc.StructureByteStride = 0;
            subresourceData.pSysMem = mesh.indices.data();

            HRESULT hr = device->CreateBuffer(&bufferDesc, &subresourceData, mesh.indexBuffer.GetAddressOf());
            _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
        }

        // ボーン構築
        for (GltfModelCommponennt::Bone& bone : mesh.bones) {
            bone.node = &newResource->nodes.at(bone.nodeIndex);
        }
    }

    // 5. 完成した結晶を神殿リソースを書き留める
    m_resources[pureModelName] = newResource;

    printf("新たにモデルリソースを神殿に奉納せり: %s\n", pureModelName.c_str());
    return newResource;
}

// シリアライズ（cerealの書記）
bool ModelResourceManager::SaveToCereal(const std::string& cerealPath, const std::shared_ptr<ModelResource>& resource)
{
    std::ofstream ostream(cerealPath, std::ios::binary);
    if (!ostream.is_open()) return false;

    cereal::BinaryOutputArchive archive(ostream);
    try {
        archive(
            CEREAL_NVP(resource->nodes),
            CEREAL_NVP(resource->materials),
            CEREAL_NVP(resource->meshes),
            CEREAL_NVP(resource->animations)
        );
        return true;
    }
    catch (...) {
        _ASSERT_EXPR_A(false, "cerealによる保存の最中例外が発生しました");
        return false;
    }
}

// デシリアライズ（cerealの解読）
bool ModelResourceManager::LoadFromCereal(const std::string& cerealPath, std::shared_ptr<ModelResource>& resource)
{
    std::ifstream istream(cerealPath, std::ios::binary);
    if (!istream.is_open()) return false;

    cereal::BinaryInputArchive archive(istream);
    try {
        archive(
            resource->nodes,
            resource->materials,
            resource->meshes,
            resource->animations
        );
        return true;
    }
    catch (...) {
        _ASSERT_EXPR_A(false, "cerealによる解読の最中に例外が発生しました");
        return false;
    }
}