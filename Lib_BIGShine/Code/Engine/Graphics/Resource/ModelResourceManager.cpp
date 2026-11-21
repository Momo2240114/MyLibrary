#include "PrecompiledHeader.h"

#include "ModelResourceManager.h"
#include "GLTFImporter.h"
#include "Engine/Graphics/Core/CPUResourceLoader.h"
#include "Engine/Common/Misc.h"
#include "Engine/Common/DebugLog.h"
#include "Engine/Debug/TracyProfiler.h"
#include <fstream>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <functional>

namespace
{
#if defined(_DEBUG) || defined(BS_DEVELOPMENT_BUILD)
    constexpr MODEL_LOAD_PROFILE_MODE MODEL_LOAD_PROFILE_MODE_VALUE =
        MODEL_LOAD_PROFILE_MODE::READ_WRITE;
#else
    constexpr MODEL_LOAD_PROFILE_MODE MODEL_LOAD_PROFILE_MODE_VALUE =
        MODEL_LOAD_PROFILE_MODE::READ_ONLY;
#endif

    double GetProfileMilliseconds()
    {
        const auto now_ = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double, std::milli>(now_).count();
    }

    double GetCurrentThreadPlotValue()
    {
        return static_cast<double>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFF);
    }
}

ModelLoadHandle::ModelLoadHandle(const std::shared_ptr<ModelLoadEntry>& entry_)
    : entry(entry_)
{
}

bool ModelLoadHandle::IsValid() const
{
    return !entry.expired();
}

bool ModelLoadHandle::IsReady() const
{
    return GetState() == MODEL_LOAD_STATE::READY;
}

bool ModelLoadHandle::IsFailed() const
{
    return GetState() == MODEL_LOAD_STATE::FAILED;
}

MODEL_LOAD_STATE ModelLoadHandle::GetState() const
{
    const std::shared_ptr<ModelLoadEntry> entry_ = entry.lock();
    if (!entry_)
    {
        return MODEL_LOAD_STATE::UNLOADED;
    }
    return entry_->state.load();
}

std::shared_ptr<ModelResource> ModelLoadHandle::GetResource() const
{
    const std::shared_ptr<ModelLoadEntry> entry_ = entry.lock();
    if (!entry_)
    {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock_(entry_->mutex);
    return entry_->resource;
}

std::string ModelLoadHandle::GetErrorMessage() const
{
    const std::shared_ptr<ModelLoadEntry> entry_ = entry.lock();
    if (!entry_)
    {
        return {};
    }
    std::lock_guard<std::mutex> lock_(entry_->mutex);
    return entry_->errorMessage;
}

PreloadHandle::PreloadHandle(
    const std::shared_ptr<PreloadGroupState>& group_,
    ModelResourceManager* manager_)
    : group(group_),
    manager(manager_)
{
}

bool PreloadHandle::IsValid() const
{
    return !group.expired() && manager;
}

bool PreloadHandle::IsReady() const
{
    const std::shared_ptr<PreloadGroupState> group_ = group.lock();
    if (!group_)
    {
        return false;
    }
    for (const ModelLoadHandle& handle_ : group_->handles)
    {
        if (!handle_.IsReady())
        {
            return false;
        }
    }
    return !group_->handles.empty();
}

bool PreloadHandle::HasFailed() const
{
    const std::shared_ptr<PreloadGroupState> group_ = group.lock();
    if (!group_)
    {
        return false;
    }
    for (const ModelLoadHandle& handle_ : group_->handles)
    {
        if (handle_.IsFailed())
        {
            return true;
        }
    }
    return false;
}

float PreloadHandle::GetProgress() const
{
    const std::shared_ptr<PreloadGroupState> group_ = group.lock();
    if (!group_ || group_->handles.empty())
    {
        return 0.0f;
    }

    int readyCount_ = 0;
    for (const ModelLoadHandle& handle_ : group_->handles)
    {
        if (handle_.IsReady() || handle_.IsFailed())
        {
            ++readyCount_;
        }
    }
    return static_cast<float>(readyCount_) / static_cast<float>(group_->handles.size());
}

void PreloadHandle::PromotePriority(PRELOAD_PRIORITY priority_)
{
    const std::shared_ptr<PreloadGroupState> group_ = group.lock();
    if (!group_ || !manager)
    {
        return;
    }
    manager->PromotePreloadGroup(group_->name, priority_);
}

// 記憶の神殿からリソースを乞う、あるいは新たに紡ぐ関数
std::shared_ptr<ModelResource> ModelResourceManager::GetOrCreateResource(ID3D11Device* device, const std::string& filename, float sampleRate)
{
    BS_PROFILE_SCOPE("ModelResourceManager::GetOrCreateResource");

    // 1. パスの深淵から、純粋なる「真名（Model名）」を抽出する
    std::filesystem::path filePath(filename);
    std::string pureModelName = filePath.stem().string();
    const std::string resourceKey = NormalizePath(filename);

    // 2. すでにその名が記憶に刻まれているか確認
    auto it = m_resources.find(resourceKey);
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
        BS_PROFILE_SCOPE("ModelResource::LoadCereal");
        isLoaded = LoadFromCereal(cerealPath.string(), newResource);
    }

    // 独自形式がない、または読み込みに失敗した場合はGLTFから紡ぎ出す
    if (!isLoaded && (extension == ".gltf" || extension == ".glb"))
    {
        BS_PROFILE_SCOPE("ModelResource::ImportGltf");
        GLTFImporter importer(filename.c_str());

        // 汎用データ読み取り
        {
            BS_PROFILE_SCOPE("ModelResource::LoadMaterials");
            importer.LoadMaterials(newResource->materials, device);
        }
        {
            BS_PROFILE_SCOPE("ModelResource::LoadNodes");
            importer.LoadNodes(newResource->nodes);
        }
        {
            BS_PROFILE_SCOPE("ModelResource::LoadMeshes");
            importer.LoadMeshes(newResource->meshes, newResource->nodes);
        }
        {
            BS_PROFILE_SCOPE("ModelResource::LoadAnimations");
            importer.LoadAnimations(newResource->animations, newResource->nodes, sampleRate);
        }

        // 独自形式のモデルファイルを保存して未来に備える
        {
            BS_PROFILE_SCOPE("ModelResource::SaveCereal");
            SaveToCereal(cerealPath.string(), newResource);
        }
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
                CPUResourceLoader::CreateDummyTexture(device, 0xFFFFFFFF, material.metalnessRoughnessMap.GetAddressOf());
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
    m_resources[resourceKey] = newResource;

    printf("新たにモデルリソースを神殿に奉納せり: %s\n", pureModelName.c_str());
    return newResource;
}

std::string ModelResourceManager::NormalizePath(const std::string& filename_) const
{
    std::filesystem::path path_(filename_);
    std::string normalized_ = path_.lexically_normal().generic_string();
    while (!normalized_.empty() && normalized_.starts_with("./"))
    {
        normalized_.erase(0, 2);
    }
#ifdef _WIN32
    std::transform(normalized_.begin(), normalized_.end(), normalized_.begin(),
        [](unsigned char c_)
    {
        return static_cast<char>(std::tolower(c_));
    });
#endif
    return normalized_;
}

std::shared_ptr<ModelResource> ModelResourceManager::LoadOrBuildCpuResource(
    const std::string& filename_,
    float sampleRate_)
{
    BS_PROFILE_SCOPE("AsyncModel::LoadOrBuild");
    BS_PROFILE_PLOT("ModelLoad.CpuBuildThread", GetCurrentThreadPlotValue());

    std::filesystem::path filePath_(filename_);
    std::filesystem::path extension_ = filePath_.extension();
    std::filesystem::path cerealPath_ = filePath_;
    cerealPath_.replace_extension(".cereal");

    auto resource_ = std::make_shared<ModelResource>();
    resource_->modelName = filePath_.stem().string();

    bool isLoaded_ = false;
    bool cacheExists_ = false;
    {
        BS_PROFILE_SCOPE("AsyncModel::CheckCacheExists");
        cacheExists_ = std::filesystem::exists(cerealPath_);
    }

    if (cacheExists_)
    {
        BS_PROFILE_SCOPE("AsyncModel::LoadCache");
        BS_PROFILE_PLOT("ModelLoad.LoadFromCacheThread", GetCurrentThreadPlotValue());
        {
            BS_PROFILE_SCOPE("AsyncModel::LoadFromCache");
            isLoaded_ = LoadFromCereal(cerealPath_.string(), resource_);
        }
    }

    if (!isLoaded_ && (extension_ == ".gltf" || extension_ == ".glb"))
    {
        BS_PROFILE_SCOPE("AsyncModel::LoadFromOriginal");
        BS_PROFILE_PLOT("ModelLoad.LoadFromOriginalThread", GetCurrentThreadPlotValue());
        BS_PROFILE_PLOT("ModelLoad.GLTFImporterThread", GetCurrentThreadPlotValue());
        GLTFImporter importer_(filename_.c_str());
        {
            BS_PROFILE_SCOPE("AsyncModel::ParseMaterials");
            BS_PROFILE_PLOT("ModelLoad.GLTFImporterThread", GetCurrentThreadPlotValue());
            importer_.LoadMaterials(resource_->materials, nullptr);
        }
        {
            BS_PROFILE_SCOPE("AsyncModel::ParseNodes");
            BS_PROFILE_PLOT("ModelLoad.GLTFImporterThread", GetCurrentThreadPlotValue());
            importer_.LoadNodes(resource_->nodes);
        }
        {
            BS_PROFILE_SCOPE("AsyncModel::BuildMeshes");
            BS_PROFILE_PLOT("ModelLoad.CpuBuildThread", GetCurrentThreadPlotValue());
            importer_.LoadMeshes(resource_->meshes, resource_->nodes);
        }
        {
            BS_PROFILE_SCOPE("AsyncModel::BuildAnimations");
            BS_PROFILE_PLOT("ModelLoad.CpuBuildThread", GetCurrentThreadPlotValue());
            importer_.LoadAnimations(resource_->animations, resource_->nodes, sampleRate_);
        }

        {
            BS_PROFILE_SCOPE("AsyncModel::WriteCache");
            SaveToCereal(cerealPath_.string(), resource_);
        }
        isLoaded_ = true;
    }

    if (!isLoaded_)
    {
        return nullptr;
    }
    return resource_;
}

bool ModelResourceManager::CreateGpuResource(
    ID3D11Device* device_,
    const std::string& filename_,
    const std::shared_ptr<ModelResource>& resource_,
    ModelLoadEntry& progress_, bool& completed_, float budgetMs_)
{
    BS_PROFILE_SCOPE("AsyncModel::CreateGpuResource");
    if (!device_ || !resource_)
    {
        return false;
    }
    completed_ = false;
    const auto uploadStart_ = std::chrono::steady_clock::now();
    const auto budgetExpired_ = [&]()
    {
        return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - uploadStart_).count() >= budgetMs_;
    };

    std::filesystem::path filePath_(filename_);
    std::filesystem::path dirpath_(filePath_.parent_path());

    {
        BS_PROFILE_SCOPE("AsyncModel::CreateMaterialGpuResources");
        for (; progress_.gpuMaterialCursor < resource_->materials.size(); )
        {
            auto& material_ = resource_->materials[progress_.gpuMaterialCursor];
            auto loadFileTexture_ = [&](const std::string& fileName_, ID3D11ShaderResourceView** srv_, TextureType type_)
            {
                BS_PROFILE_SCOPE("AsyncModel::LoadTextureFile");
                std::filesystem::path texturePath_(dirpath_ / fileName_);
                return CPUResourceLoader::LoadTextureFromFile(device_, texturePath_.wstring().c_str(), srv_, nullptr, type_);
            };

            HRESULT hr_ = S_OK;
            if (material_.baseMap == nullptr)
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateBaseTexture");
                hr_ = material_.baseTextureFileName.empty()
                    ? CPUResourceLoader::CreateDummyTexture(device_, 0xFFFFFFFF, material_.baseMap.GetAddressOf())
                    : CPUResourceLoader::LoadTextureFromFile(device_, (dirpath_ / material_.baseTextureFileName).wstring().c_str(), material_.baseMap.GetAddressOf());
                if (FAILED(hr_))
                {
                    return false;
                }
            }
            if (material_.normalMap == nullptr)
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateNormalTexture");
                hr_ = material_.normalTextureFileName.empty()
                    ? CPUResourceLoader::CreateDummyTexture(device_, 0xFFFF7F7F, material_.normalMap.GetAddressOf())
                    : loadFileTexture_(material_.normalTextureFileName, material_.normalMap.GetAddressOf(), TextureType::Data);
                if (FAILED(hr_))
                {
                    return false;
                }
            }
            if (material_.emissiveMap == nullptr)
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateEmissiveTexture");
                hr_ = material_.emissiveTextureFileName.empty()
                    ? CPUResourceLoader::CreateDummyTexture(device_, 0x00000000, material_.emissiveMap.GetAddressOf())
                    : loadFileTexture_(material_.emissiveTextureFileName, material_.emissiveMap.GetAddressOf(), TextureType::Color);
                if (FAILED(hr_))
                {
                    return false;
                }
            }
            if (material_.occlusionMap == nullptr)
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateOcclusionTexture");
                hr_ = material_.occlusionTextureFileName.empty()
                    ? CPUResourceLoader::CreateDummyTexture(device_, 0xFFFFFFFF, material_.occlusionMap.GetAddressOf())
                    : loadFileTexture_(material_.occlusionTextureFileName, material_.occlusionMap.GetAddressOf(), TextureType::Data);
                if (FAILED(hr_))
                {
                    return false;
                }
            }
            if (material_.metalnessRoughnessMap == nullptr)
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateMetalnessRoughnessTexture");
                hr_ = material_.metalnessRoughnessTextureFileName.empty()
                    ? CPUResourceLoader::CreateDummyTexture(device_, 0xFFFFFFFF, material_.metalnessRoughnessMap.GetAddressOf())
                    : loadFileTexture_(material_.metalnessRoughnessTextureFileName, material_.metalnessRoughnessMap.GetAddressOf(), TextureType::Data);
                if (FAILED(hr_))
                {
                    return false;
                }
            }
            ++progress_.gpuMaterialCursor;
            if (budgetExpired_())
            {
                return true;
            }
        }
    }

    if (!progress_.gpuNodesBound)
    {
        BS_PROFILE_SCOPE("AsyncModel::RebindGpuNodes");
        for (size_t nodeIndex_ = 0; nodeIndex_ < resource_->nodes.size(); ++nodeIndex_)
        {
            GltfModelCommponennt::Node& node_ = resource_->nodes.at(nodeIndex_);
            node_.children.clear();
            node_.parent = node_.parentIndex >= 0 ? &resource_->nodes.at(node_.parentIndex) : nullptr;
            if (node_.parent != nullptr)
            {
                node_.parent->children.emplace_back(&node_);
            }
        }
        progress_.gpuNodesBound = true;
    }

    {
        BS_PROFILE_SCOPE("AsyncModel::CreateMeshGpuResources");
        for (; progress_.gpuMeshCursor < resource_->meshes.size(); )
        {
            auto& mesh_ = resource_->meshes[progress_.gpuMeshCursor];
            mesh_.material = &resource_->materials.at(mesh_.materialIndex);
            mesh_.node = &resource_->nodes.at(mesh_.nodeIndex);

            D3D11_BUFFER_DESC bufferDesc_ = {};
            D3D11_SUBRESOURCE_DATA subresourceData_ = {};

            bufferDesc_.ByteWidth = static_cast<UINT>(sizeof(GltfModelCommponennt::Vertex) * mesh_.vertices.size());
            bufferDesc_.Usage = D3D11_USAGE_IMMUTABLE;
            bufferDesc_.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            subresourceData_.pSysMem = mesh_.vertices.data();
            HRESULT hr_ = S_OK;
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateVertexBuffer");
                hr_ = device_->CreateBuffer(&bufferDesc_, &subresourceData_, mesh_.vertexBuffer.GetAddressOf());
            }
            if (FAILED(hr_))
            {
                return false;
            }

            bufferDesc_ = {};
            subresourceData_ = {};
            bufferDesc_.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * mesh_.indices.size());
            bufferDesc_.Usage = D3D11_USAGE_IMMUTABLE;
            bufferDesc_.BindFlags = D3D11_BIND_INDEX_BUFFER;
            subresourceData_.pSysMem = mesh_.indices.data();
            {
                BS_PROFILE_SCOPE("AsyncModel::CreateIndexBuffer");
                hr_ = device_->CreateBuffer(&bufferDesc_, &subresourceData_, mesh_.indexBuffer.GetAddressOf());
            }
            if (FAILED(hr_))
            {
                return false;
            }

            {
                BS_PROFILE_SCOPE("AsyncModel::RebindBones");
                for (GltfModelCommponennt::Bone& bone_ : mesh_.bones)
                {
                    bone_.node = &resource_->nodes.at(bone_.nodeIndex);
                }
            }
            ++progress_.gpuMeshCursor;
            if (budgetExpired_())
            {
                return true;
            }
        }
    }

    completed_ = true;
    return true;
}

std::shared_ptr<ModelLoadEntry> ModelResourceManager::FindOrCreateEntry(
    const std::string& filename_,
    int priority_)
{
    const std::string normalizedPath_ = NormalizePath(filename_);
    const double estimatedLoadMilliseconds_ =
        loadProfileManager.GetEstimatedLoadMilliseconds(normalizedPath_);
    const double relativeLoadWeight_ =
        loadProfileManager.GetRelativeLoadWeight(normalizedPath_);
    std::lock_guard<std::mutex> lock_(asyncMutex);
    auto found_ = loadEntries.find(normalizedPath_);
    if (found_ != loadEntries.end())
    {
        found_->second->priority = (std::max)(found_->second->priority, priority_);
        found_->second->estimatedLoadMilliseconds =
            (std::max)(found_->second->estimatedLoadMilliseconds, estimatedLoadMilliseconds_);
        found_->second->relativeLoadWeight =
            (std::max)(found_->second->relativeLoadWeight, relativeLoadWeight_);
        return found_->second;
    }

    auto entry_ = std::make_shared<ModelLoadEntry>();
    entry_->normalizedPath = normalizedPath_;
    entry_->priority = priority_;
    entry_->requestOrder = nextRequestOrder++;
    entry_->estimatedLoadMilliseconds = estimatedLoadMilliseconds_;
    entry_->relativeLoadWeight = relativeLoadWeight_;
    auto resourceFound_ = m_resources.find(normalizedPath_);
    if (resourceFound_ != m_resources.end())
    {
        entry_->resource = resourceFound_->second;
        entry_->state = MODEL_LOAD_STATE::READY;
    }
    loadEntries[normalizedPath_] = entry_;
    return entry_;
}

ModelLoadHandle ModelResourceManager::RequestResourceAsync(
    const std::string& filename_,
    int priority_)
{
    BS_PROFILE_SCOPE("AsyncModel::RequestResourceAsync");
    BS_PROFILE_PLOT("ModelLoad.RequestResourceAsyncTime", GetProfileMilliseconds());
    EnsureLoadProfileLoaded();

    std::shared_ptr<ModelLoadEntry> entry_;
    {
        BS_PROFILE_SCOPE("AsyncModel::FindOrCreateEntry");
        entry_ = FindOrCreateEntry(filename_, priority_);
    }
    {
        BS_PROFILE_SCOPE("AsyncModel::EnqueueRequest");
        std::lock_guard<std::mutex> lock_(asyncMutex);
        const MODEL_LOAD_STATE state_ = entry_->state.load();
        if (state_ == MODEL_LOAD_STATE::UNLOADED || state_ == MODEL_LOAD_STATE::FAILED)
        {
            BS_PROFILE_SCOPE("AsyncModel::EnqueueTask");
            std::lock_guard<std::mutex> entryLock_(entry_->mutex);
            {
                BS_PROFILE_SCOPE_DYNAMIC(entry_->normalizedPath.c_str());
                entry_->state = MODEL_LOAD_STATE::WAITING;
                entry_->errorMessage.clear();
                entry_->completedLoadMilliseconds = 0.0;
                entry_->gpuMaterialCursor = 0;
                entry_->gpuMeshCursor = 0;
                entry_->gpuNodesBound = false;
                entry_->loadStartTime = std::chrono::steady_clock::now();
                pendingJobs.push_back(
                    {
                        priority_,
                        entry_->estimatedLoadMilliseconds,
                        entry_->relativeLoadWeight,
                        entry_->requestOrder,
                        entry_
                    });
            }
            const uint64_t enqueuedCount_ = ++enqueuedTaskCount;
            BS_PROFILE_PLOT("ModelLoad.EnqueuedTaskCount", static_cast<double>(enqueuedCount_));
            BS_PROFILE_PLOT("ModelLoad.PendingTaskCount", static_cast<double>(pendingJobs.size()));
        }
    }
    StartAsyncLoader();
    {
        BS_PROFILE_SCOPE("AsyncModel::NotifyWorkers");
        BS_PROFILE_PLOT("ModelLoad.NotifyAllTime", GetProfileMilliseconds());
        asyncCondition.notify_all();
    }
    return ModelLoadHandle(entry_);
}

void ModelResourceManager::PromotePriority(const std::string& filename_, int priority_)
{
    const std::string normalizedPath_ = NormalizePath(filename_);
    std::lock_guard<std::mutex> lock_(asyncMutex);
    auto found_ = loadEntries.find(normalizedPath_);
    if (found_ == loadEntries.end())
    {
        return;
    }

    found_->second->priority = (std::max)(found_->second->priority, priority_);
    for (ModelLoadJob& job_ : pendingJobs)
    {
        if (job_.entry == found_->second)
        {
            job_.priority = found_->second->priority;
        }
    }
}

void ModelResourceManager::StartAsyncLoader()
{
    BS_PROFILE_SCOPE("AsyncModel::StartAsyncLoader");
    EnsureLoadProfileLoaded();

    std::lock_guard<std::mutex> lock_(asyncMutex);
    if (workerRunning)
    {
        return;
    }
    workerStopRequested = false;
    workerRunning = true;

    const unsigned int workerCount_ = GetWorkerCount(pendingJobs.size());
    activeModelLoadWorkerCount = workerCount_;
    BS_PROFILE_PLOT("ModelLoad.WorkerPoolSize", static_cast<double>(activeModelLoadWorkerCount));
    BS_PROFILE_PLOT("ModelLoad.ActiveWorkerCount", static_cast<double>(executingWorkerCount.load()));
    BS_PROFILE_PLOT("ModelLoad.WaitingWorkerCount", static_cast<double>(waitingWorkerCount.load()));
    workerThreads.reserve(workerCount_);
    for (unsigned int workerIndex_ = 0; workerIndex_ < workerCount_; ++workerIndex_)
    {
        workerThreads.emplace_back(
            &ModelResourceManager::WorkerLoop,
            this,
            workerIndex_);
    }
}

void ModelResourceManager::ShutdownAsyncLoader()
{
    BS_PROFILE_SCOPE("AsyncModel::ShutdownAsyncLoader");
    {
        BS_PROFILE_SCOPE("AsyncModel::ShutdownAsyncLoader::RequestStop");
        std::lock_guard<std::mutex> lock_(asyncMutex);
        if (!workerRunning)
        {
            return;
        }
        workerStopRequested = true;
    }
    asyncCondition.notify_all();
    for (std::thread& workerThread_ : workerThreads)
    {
        BS_PROFILE_SCOPE("AsyncModel::ShutdownAsyncLoader::JoinWorker");
        if (workerThread_.joinable())
        {
            workerThread_.join();
        }
    }
    {
        BS_PROFILE_SCOPE("AsyncModel::ShutdownAsyncLoader::ClearQueues");
        std::lock_guard<std::mutex> lock_(asyncMutex);
        workerRunning = false;
        activeModelLoadWorkerCount = 0;
        workerThreads.clear();
        pendingJobs.clear();
        pendingGpuUploads.clear();
    }
    FlushLoadProfile();
}

void ModelResourceManager::FlushLoadProfile()
{
    bool shouldSave_ = false;
    {
        std::lock_guard<std::mutex> lock_(asyncMutex);
        shouldSave_ = loadProfileDirty;
        loadProfileDirty = false;
    }

    if (MODEL_LOAD_PROFILE_MODE_VALUE != MODEL_LOAD_PROFILE_MODE::READ_WRITE)
    {
        return;
    }

    bool saved_ = true;
    if (shouldSave_)
    {
        loadProfileManager.GenerateProfileFromHistory();
        saved_ = loadProfileManager.SaveHistory(loadHistoryPath) &&
            loadProfileManager.SaveProfile(loadProfilePath);
    }

    if (!saved_)
    {
        std::lock_guard<std::mutex> lock_(asyncMutex);
        loadProfileDirty = true;
    }
}

void ModelResourceManager::EnsureLoadProfileLoaded()
{
    {
        std::lock_guard<std::mutex> lock_(asyncMutex);
        if (loadProfileLoaded)
        {
            return;
        }
        loadProfileLoaded = true;
    }

    if (MODEL_LOAD_PROFILE_MODE_VALUE == MODEL_LOAD_PROFILE_MODE::READ_WRITE)
    {
        loadProfileManager.LoadHistory(loadHistoryPath);
    }
    loadProfileManager.LoadProfile(loadProfilePath);
    loadProfileManager.RemoveInvalidEntries();
}

unsigned int ModelResourceManager::GetWorkerCount(size_t loadTaskCount_) const
{
    const unsigned int hardwareThreadCount_ =
        (std::max)(1u, std::thread::hardware_concurrency());

    const unsigned int halfThreadCount_ =
        (std::max)(1u, hardwareThreadCount_ / 2u);

    if (loadTaskCount_ == 0)
    {
        return std::clamp(
            halfThreadCount_,
            minimumModelLoadWorkerCount,
            (std::max)(minimumModelLoadWorkerCount, maximumModelLoadWorkerCount));
    }

    const unsigned int taskLimitedCount_ =
        static_cast<unsigned int>(
            (std::min)(
                loadTaskCount_,
                static_cast<size_t>(halfThreadCount_)));

    return std::clamp(
        taskLimitedCount_,
        minimumModelLoadWorkerCount,
        (std::max)(minimumModelLoadWorkerCount, maximumModelLoadWorkerCount));
}

void ModelResourceManager::WorkerLoop(unsigned int workerIndex_)
{
    for (;;)
    {
        BS_PROFILE_SCOPE("AsyncModel::WorkerLoop");
        ModelLoadJob job_;
        {
            BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::WaitForTask");
            std::unique_lock<std::mutex> lock_(asyncMutex);
            {
                BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::ConditionWait");
                ++waitingWorkerCount;
                BS_PROFILE_PLOT("ModelLoad.WaitingWorkerCount", static_cast<double>(waitingWorkerCount.load()));
                asyncCondition.wait(lock_, [this]
                {
                    return workerStopRequested || !pendingJobs.empty();
                });
                --waitingWorkerCount;
                BS_PROFILE_PLOT("ModelLoad.WaitingWorkerCount", static_cast<double>(waitingWorkerCount.load()));
                BS_PROFILE_PLOT("ModelLoad.ConditionWaitEndTime", GetProfileMilliseconds());
            }
            if (workerStopRequested && pendingJobs.empty())
            {
                return;
            }

            {
                BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::DequeueTask");
                BS_PROFILE_PLOT("ModelLoad.DequeueTaskStartTime", GetProfileMilliseconds());
                auto best_ = std::max_element(pendingJobs.begin(), pendingJobs.end(),
                    [](const ModelLoadJob& lhs_, const ModelLoadJob& rhs_)
                    {
                        if (lhs_.priority != rhs_.priority)
                    {
                        return lhs_.priority < rhs_.priority;
                    }
                    if (lhs_.relativeLoadWeight != rhs_.relativeLoadWeight)
                        {
                            return lhs_.relativeLoadWeight < rhs_.relativeLoadWeight;
                        }
                        if (lhs_.estimatedLoadMilliseconds != rhs_.estimatedLoadMilliseconds)
                        {
                            return lhs_.estimatedLoadMilliseconds < rhs_.estimatedLoadMilliseconds;
                        }
                        return lhs_.requestOrder > rhs_.requestOrder;
                    });
                job_ = *best_;
                pendingJobs.erase(best_);
                BS_PROFILE_PLOT("ModelLoad.PendingTaskCount", static_cast<double>(pendingJobs.size()));
                BS_PROFILE_PLOT("ModelLoad.DequeueTaskEndTime", GetProfileMilliseconds());
            }
        }

        if (!job_.entry)
        {
            continue;
        }
        try
        {
            BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::ExecuteTask");
            ++executingWorkerCount;
            BS_PROFILE_PLOT("ModelLoad.ActiveWorkerCount", static_cast<double>(executingWorkerCount.load()));
            BS_PROFILE_PLOT("ModelLoad.ExecuteTaskStartTime", GetProfileMilliseconds());
            {
                BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::SetLoadingCpu");
                job_.entry->state = MODEL_LOAD_STATE::LOADING_CPU;
            }
            std::shared_ptr<ModelResource> resource_;
            {
                BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::LoadOrBuild");
                const std::string modelPath_ = job_.entry->normalizedPath;
                {
                    BS_PROFILE_SCOPE_DYNAMIC(modelPath_.c_str());
                    resource_ = LoadOrBuildCpuResource(job_.entry->normalizedPath, 60.0f);
                }
            }
            if (!resource_)
            {
                throw std::runtime_error("CPU model load failed.");
            }
            const uint64_t completedCpuCount_ = ++completedCpuTaskCount;
            BS_PROFILE_PLOT("ModelLoad.CompletedCpuTaskCount", static_cast<double>(completedCpuCount_));

            {
                BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::RegisterCpuResult");
                std::lock_guard<std::mutex> entryLock_(job_.entry->mutex);
                job_.entry->cpuResource = resource_;
            }
            {
                BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::NotifyTaskCompletion");
                QueueGpuUpload(job_.entry);
                job_.entry->state = MODEL_LOAD_STATE::WAITING_GPU_UPLOAD;
            }
            BS_PROFILE_PLOT("ModelLoad.ExecuteTaskEndTime", GetProfileMilliseconds());
            --executingWorkerCount;
            BS_PROFILE_PLOT("ModelLoad.ActiveWorkerCount", static_cast<double>(executingWorkerCount.load()));
        }
        catch (const std::exception& exception_)
        {
            BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::HandleTaskError");
            std::lock_guard<std::mutex> entryLock_(job_.entry->mutex);
            job_.entry->errorMessage =
                "Worker " + std::to_string(workerIndex_) + ": " + exception_.what();
            job_.entry->state = MODEL_LOAD_STATE::FAILED;
            if (executingWorkerCount.load() > 0)
            {
                --executingWorkerCount;
            }
            BS_PROFILE_PLOT("ModelLoad.ActiveWorkerCount", static_cast<double>(executingWorkerCount.load()));
        }
        catch (...)
        {
            BS_PROFILE_SCOPE("AsyncModel::WorkerLoop::HandleUnknownTaskError");
            std::lock_guard<std::mutex> entryLock_(job_.entry->mutex);
            job_.entry->errorMessage =
                "Worker " + std::to_string(workerIndex_) + ": Unknown async model load error.";
            job_.entry->state = MODEL_LOAD_STATE::FAILED;
            if (executingWorkerCount.load() > 0)
            {
                --executingWorkerCount;
            }
            BS_PROFILE_PLOT("ModelLoad.ActiveWorkerCount", static_cast<double>(executingWorkerCount.load()));
        }
    }
}

void ModelResourceManager::QueueGpuUpload(const std::shared_ptr<ModelLoadEntry>& entry_)
{
    BS_PROFILE_SCOPE("AsyncModel::QueueGpuUpload");
    std::lock_guard<std::mutex> lock_(asyncMutex);
    pendingGpuUploads.push_back(entry_);
    BS_PROFILE_PLOT("ModelLoad.PendingGpuTaskCount", static_cast<double>(pendingGpuUploads.size()));
}

void ModelResourceManager::UpdateAsyncLoads(
    ID3D11Device* device_,
    float gpuUploadBudgetMs_)
{
    BS_PROFILE_SCOPE("AsyncModel::UpdateAsyncLoads");
    const auto startTime_ = std::chrono::steady_clock::now();
    {
        BS_PROFILE_SCOPE("AsyncModel::PlotLoadCounters");
        std::lock_guard<std::mutex> lock_(asyncMutex);
        BS_PROFILE_PLOT("ModelLoad.PendingTaskCount", static_cast<double>(pendingJobs.size()));
        BS_PROFILE_PLOT("ModelLoad.PendingGpuTaskCount", static_cast<double>(pendingGpuUploads.size()));
    }
    BS_PROFILE_PLOT("ModelLoad.EnqueuedTaskCount", static_cast<double>(enqueuedTaskCount.load()));
    BS_PROFILE_PLOT("ModelLoad.ActiveWorkerCount", static_cast<double>(executingWorkerCount.load()));
    BS_PROFILE_PLOT("ModelLoad.WaitingWorkerCount", static_cast<double>(waitingWorkerCount.load()));
    BS_PROFILE_PLOT("ModelLoad.CompletedCpuTaskCount", static_cast<double>(completedCpuTaskCount.load()));

    for (;;)
    {
        std::shared_ptr<ModelLoadEntry> entry_;
        {
            BS_PROFILE_SCOPE("AsyncModel::GpuUpload::Dequeue");
            std::lock_guard<std::mutex> lock_(asyncMutex);
            if (pendingGpuUploads.empty())
            {
                break;
            }
            entry_ = pendingGpuUploads.front();
            pendingGpuUploads.pop_front();
            BS_PROFILE_PLOT("ModelLoad.PendingGpuTaskCount", static_cast<double>(pendingGpuUploads.size()));
        }

        std::shared_ptr<ModelResource> cpuResource_;
        {
            BS_PROFILE_SCOPE("AsyncModel::GpuUpload::ReadCpuResource");
            std::lock_guard<std::mutex> entryLock_(entry_->mutex);
            cpuResource_ = entry_->cpuResource;
        }

        bool uploadCompleted_ = false;
        {
            BS_PROFILE_SCOPE("AsyncModel::GpuUpload::CreateGpuResource");
            BS_PROFILE_PLOT("ModelLoad.GpuCreateThread", GetCurrentThreadPlotValue());
            const std::string modelPath_ = entry_->normalizedPath;
            bool createdGpuResource_ = false;
            {
                BS_PROFILE_SCOPE_DYNAMIC(modelPath_.c_str());
                try
                {
                    createdGpuResource_ = CreateGpuResource(device_, entry_->normalizedPath, cpuResource_,
                        *entry_, uploadCompleted_, gpuUploadBudgetMs_);
                }
                catch (const std::exception& exception_)
                {
                    std::lock_guard<std::mutex> entryLock_(entry_->mutex);
                    entry_->errorMessage = exception_.what();
                    entry_->state = MODEL_LOAD_STATE::FAILED;
                    continue;
                }
            }
            if (!createdGpuResource_)
            {
                std::lock_guard<std::mutex> entryLock_(entry_->mutex);
                entry_->errorMessage = "GPU model upload failed.";
                entry_->state = MODEL_LOAD_STATE::FAILED;
                continue;
            }
        }

        if (!uploadCompleted_)
        {
            // CPU側データはEntryが保持したまま再投入し、次フレームも描画へ制御を返す。
            std::lock_guard<std::mutex> lock_(asyncMutex);
            pendingGpuUploads.push_front(entry_);
            break;
        }
        {
            BS_PROFILE_SCOPE("AsyncModel::GpuUpload::RegisterModelCache");
            std::lock_guard<std::mutex> lock_(asyncMutex);
            m_resources[entry_->normalizedPath] = cpuResource_;
        }
        {
            BS_PROFILE_SCOPE("AsyncModel::GpuUpload::CompleteEntry");
            std::lock_guard<std::mutex> entryLock_(entry_->mutex);
            entry_->resource = cpuResource_;
            entry_->cpuResource.reset();
            entry_->completedLoadMilliseconds =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - entry_->loadStartTime).count();
            entry_->state = MODEL_LOAD_STATE::READY;
        }

        if (MODEL_LOAD_PROFILE_MODE_VALUE == MODEL_LOAD_PROFILE_MODE::READ_WRITE)
        {
            BS_PROFILE_SCOPE("AsyncModel::GpuUpload::RecordLoadProfile");
            loadProfileManager.RecordLoadResult(
                entry_->normalizedPath,
                entry_->completedLoadMilliseconds,
                activeModelLoadWorkerCount);
            {
                std::lock_guard<std::mutex> lock_(asyncMutex);
                loadProfileDirty = true;
            }
        }

        const auto now_ = std::chrono::steady_clock::now();
        const float elapsedMs_ = std::chrono::duration<float, std::milli>(now_ - startTime_).count();
        if (elapsedMs_ >= gpuUploadBudgetMs_)
        {
            break;
        }
    }

    if (!HasPendingLoads())
    {
        FlushLoadProfile();
    }
}

bool ModelResourceManager::HasPendingLoads() const
{
    std::lock_guard<std::mutex> lock_(asyncMutex);
    if (!pendingJobs.empty() || !pendingGpuUploads.empty())
    {
        return true;
    }
    for (const auto& pair_ : loadEntries)
    {
        const MODEL_LOAD_STATE state_ = pair_.second->state.load();
        if (state_ != MODEL_LOAD_STATE::READY && state_ != MODEL_LOAD_STATE::FAILED)
        {
            return true;
        }
    }
    return false;
}

float ModelResourceManager::GetLoadProgress() const
{
    std::lock_guard<std::mutex> lock_(asyncMutex);
    if (loadEntries.empty())
    {
        return 1.0f;
    }

    double completedEstimatedMilliseconds_ = 0.0;
    double totalEstimatedMilliseconds_ = 0.0;
    for (const auto& pair_ : loadEntries)
    {
        const double estimatedMilliseconds_ =
            (std::max)(1.0, pair_.second->estimatedLoadMilliseconds);
        totalEstimatedMilliseconds_ += estimatedMilliseconds_;

        const MODEL_LOAD_STATE state_ = pair_.second->state.load();
        if (state_ == MODEL_LOAD_STATE::READY || state_ == MODEL_LOAD_STATE::FAILED)
        {
            completedEstimatedMilliseconds_ += estimatedMilliseconds_;
        }
    }

    if (totalEstimatedMilliseconds_ <= 0.0)
    {
        return 1.0f;
    }
    return static_cast<float>(
        std::clamp(
            completedEstimatedMilliseconds_ / totalEstimatedMilliseconds_,
            0.0,
            1.0));
}

void ModelResourceManager::LoadPreloadSettings(const std::string& filename_)
{
    if (preloadSettingsLoaded)
    {
        return;
    }
    preloadSettingsLoaded = true;

    std::ifstream file_(filename_);
    if (!file_.is_open())
    {
        return;
    }

    nlohmann::json json_;
    file_ >> json_;
    for (const nlohmann::json& groupJson_ : json_.value("Groups", nlohmann::json::array()))
    {
        PreloadGroupSetting setting_;
        setting_.name = groupJson_.value("Name", "");
        if (setting_.name.empty())
        {
            continue;
        }

        for (const nlohmann::json& assetJson_ : groupJson_.value("Assets", nlohmann::json::array()))
        {
            PreloadAsset asset_;
            asset_.type = assetJson_.value("Type", "");
            asset_.path = assetJson_.value("Path", "");
            asset_.priority = assetJson_.value("Priority", static_cast<int>(PRELOAD_PRIORITY::NORMAL));
            if (!asset_.path.empty())
            {
                setting_.assets.push_back(asset_);
            }
        }
        preloadSettings[setting_.name] = setting_;
    }
}

bool ModelResourceManager::LoadStartupResourceSettings(const std::string& filename_)
{
    if (startupResourceSettingsLoaded)
    {
        return true;
    }
    startupResourceSettingsLoaded = true;

    const bool loaded_ = startupResourceConfig.Load(filename_);
    if (!loaded_)
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[StartupResource] Startup resource config was not loaded: " + filename_);
    }
    return loaded_;
}

PreloadHandle ModelResourceManager::RequestStartupCriticalModels()
{
    BS_PROFILE_SCOPE("StartupResource::RequestCriticalModels");
    LoadStartupResourceSettings();

    auto group_ = std::make_shared<PreloadGroupState>();
    group_->name = "STARTUP_CRITICAL";

    for (const StartupModelEntry& entry_ : startupResourceConfig.GetCriticalModels())
    {
        if (entry_.filePath.empty())
        {
            continue;
        }
        group_->handles.push_back(
            RequestResourceAsync(entry_.filePath, static_cast<int>(PRELOAD_PRIORITY::CRITICAL)));
    }

    if (group_->handles.empty())
    {
        return PreloadHandle();
    }

    preloadGroups[group_->name] = group_;
    return PreloadHandle(group_, this);
}

void ModelResourceManager::RequestStartupHighPriorityModels()
{
    BS_PROFILE_SCOPE("StartupResource::RequestHighPriorityModels");
    LoadStartupResourceSettings();

    for (const StartupModelEntry& entry_ : startupResourceConfig.GetHighPriorityModels())
    {
        if (entry_.filePath.empty())
        {
            continue;
        }
        RequestResourceAsync(entry_.filePath, static_cast<int>(PRELOAD_PRIORITY::HIGH));
    }
}

PreloadHandle ModelResourceManager::RequestPreloadGroup(
    const std::string& groupName_,
    PRELOAD_PRIORITY priority_)
{
    BS_PROFILE_SCOPE("Preload::RequestGroup");
    LoadPreloadSettings();

    auto group_ = std::make_shared<PreloadGroupState>();
    group_->name = groupName_;

    auto setting_ = preloadSettings.find(groupName_);
    if (setting_ != preloadSettings.end())
    {
        for (const PreloadAsset& asset_ : setting_->second.assets)
        {
            if (asset_.type != "MODEL")
            {
                continue;
            }
            const int priorityValue_ = (std::max)(asset_.priority, static_cast<int>(priority_));
            group_->handles.push_back(RequestResourceAsync(asset_.path, priorityValue_));
        }
    }

    preloadGroups[groupName_] = group_;
    return PreloadHandle(group_, this);
}

void ModelResourceManager::PromotePreloadGroup(
    const std::string& groupName_,
    PRELOAD_PRIORITY priority_)
{
    BS_PROFILE_SCOPE("Preload::PromoteGroup");
    auto group_ = preloadGroups.find(groupName_);
    if (group_ == preloadGroups.end())
    {
        return;
    }

    for (ModelLoadHandle& handle_ : group_->second->handles)
    {
        const std::shared_ptr<ModelLoadEntry> entry_ = handle_.entry.lock();
        if (!entry_)
        {
            continue;
        }
        PromotePriority(entry_->normalizedPath, static_cast<int>(priority_));
    }
}

bool ModelResourceManager::IsPreloadGroupReady(const std::string& groupName_) const
{
    auto found_ = preloadGroups.find(groupName_);
    if (found_ == preloadGroups.end())
    {
        return false;
    }
    for (const ModelLoadHandle& handle_ : found_->second->handles)
    {
        if (!handle_.IsReady())
        {
            return false;
        }
    }
    return !found_->second->handles.empty();
}

float ModelResourceManager::GetPreloadGroupProgress(const std::string& groupName_) const
{
    auto found_ = preloadGroups.find(groupName_);
    if (found_ == preloadGroups.end() || found_->second->handles.empty())
    {
        return 0.0f;
    }

    int completeCount_ = 0;
    for (const ModelLoadHandle& handle_ : found_->second->handles)
    {
        if (handle_.IsReady() || handle_.IsFailed())
        {
            ++completeCount_;
        }
    }
    return static_cast<float>(completeCount_) / static_cast<float>(found_->second->handles.size());
}

// シリアライズ（cerealの書記）
bool ModelResourceManager::SaveToCereal(const std::string& cerealPath, const std::shared_ptr<ModelResource>& resource)
{
    BS_PROFILE_SCOPE("ModelResourceManager::SaveToCereal");
    std::ofstream ostream(cerealPath, std::ios::binary);
    if (!ostream.is_open())
    {
        return false;
    }

    cereal::BinaryOutputArchive archive(ostream);
    try {
        BS_PROFILE_SCOPE("ModelResourceManager::SerializeCereal");
        archive(
            CEREAL_NVP(resource->nodes),
            CEREAL_NVP(resource->materials),
            CEREAL_NVP(resource->meshes),
            CEREAL_NVP(resource->animations)
        );
        // 既存バイナリの末尾へ追加し、古いモデルキャッシュの読み込みを維持する。
        nlohmann::json styleSettings_ = nlohmann::json::array();
        for (const auto& material_ : resource->materials)
        {
            nlohmann::json style_;
            material_.SerializeStyleSettings(style_);
            styleSettings_.push_back(style_);
        }
        const std::string styleText_ = styleSettings_.dump();
        archive(styleText_);
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
    BS_PROFILE_SCOPE("ModelResourceManager::LoadFromCereal");

    std::ifstream istream(cerealPath, std::ios::binary);
    if (!istream.is_open())
    {
        return false;
    }

    cereal::BinaryInputArchive archive(istream);
    try {
        BS_PROFILE_SCOPE("ModelResourceManager::DeserializeCereal");
        archive(
            resource->nodes,
            resource->materials,
            resource->meshes,
            resource->animations
        );
        // 末尾設定のない旧形式ではMaterialの初期値をそのまま使う。
        if (istream.peek() != std::char_traits<char>::eof())
        {
            std::string styleText_;
            archive(styleText_);
            const auto styleSettings_ = nlohmann::json::parse(styleText_);
            if (styleSettings_.is_array())
            {
                for (size_t index_ = 0; index_ < resource->materials.size() && index_ < styleSettings_.size(); ++index_)
                {
                    resource->materials[index_].DeserializeStyleSettings(styleSettings_[index_]);
                }
            }
        }
        return true;
    }
    catch (...) {
        _ASSERT_EXPR_A(false, "cerealによる解読の最中に例外が発生しました");
        return false;
    }
}
