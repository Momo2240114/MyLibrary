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
    // 世界にただ一つの神殿へアクセスする門
    static ModelResourceManager& Instance()
    {
        static ModelResourceManager instance;
        return instance;
    }

    // 記憶の神殿からリソースを乞う、あるいは新たに紡ぐ関数
    std::shared_ptr<ModelResource> GetOrCreateResource(ID3D11Device* device, const std::string& filename, float sampleRate = 60.0f);

    // すべての記憶をリセットし、神殿を浄化する
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

    // 聖なるシリアライズ・デシリアライズの秘術（cpp側で実装）
    bool SaveToCereal(const std::string& cerealPath, const std::shared_ptr<ModelResource>& resource);
    bool LoadFromCereal(const std::string& cerealPath, std::shared_ptr<ModelResource>& resource);

private:
    // 「真名（Model名）」を鍵として、リソースの結晶を保管する棚
    std::map<std::string, std::shared_ptr<ModelResource>> m_resources;
};