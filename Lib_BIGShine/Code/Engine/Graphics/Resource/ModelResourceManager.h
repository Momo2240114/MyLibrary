#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <d3d11.h>
#include "ModelResource.h"
#include "GLTFImporter.h"
#include <filesystem>

class ModelResourceManager
{
public:
    // 世界にただ一つのなのでアクセスするための関数
    static ModelResourceManager& Instance()
    {
        static ModelResourceManager instance;
        return instance;
    }

    // リソースを獲得、あるいは新たに作製する関数
    std::shared_ptr<ModelResource> GetOrCreateResource(ID3D11Device* device, const std::string& filename, float sampleRate = 60.0f);

    // すべての記憶をリセットしてリソースをclear
    void Clear()
    {
        for (auto& pair : m_resources) {
            if (pair.second) pair.second->Clear();
        }
        m_resources.clear();
    }

private:
    ModelResourceManager() = default;
    ~ModelResourceManager() { Clear(); }
    ModelResourceManager(const ModelResourceManager&) = delete;
    ModelResourceManager& operator=(const ModelResourceManager&) = delete;

    // シリアライズ・デシリアライズ
    bool SaveToCereal(const std::string& cerealPath, const std::shared_ptr<ModelResource>& resource);
    bool LoadFromCereal(const std::string& cerealPath, std::shared_ptr<ModelResource>& resource);

private:
    // Model名を鍵として、リソースのを保管する棚
    std::map<std::string, std::shared_ptr<ModelResource>> m_resources;
};