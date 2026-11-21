#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <filesystem>
#include "PoseLibraryResource.h"

class PoseLibraryResourceManager
{
public:
    // 世界にただ一つの神殿へアクセスする門
    static PoseLibraryResourceManager& Instance()
    {
        static PoseLibraryResourceManager instance;
        return instance;
    }

    // 記憶の神殿からポーズリソースを乞う、あるいは新たに紡ぐ関数
    std::shared_ptr<PoseLibraryResource> GetOrCreateResource(const std::string& modelName);

    // すべての記憶をリセットし、神殿を浄化する
    void Clear()
    {
        for (auto& pair : m_resources) {
            if (pair.second)
            {
                pair.second->Clear();
            }
        }
        m_resources.clear();
    }

    // 聖なるjsonの書記と解読
    bool SaveToJson(const std::string& filename, const std::shared_ptr<PoseLibraryResource>& resource);
    bool LoadFromJson(const std::string& filename, std::shared_ptr<PoseLibraryResource>& resource);

private:
    PoseLibraryResourceManager() = default;

    ~PoseLibraryResourceManager()
    {
        Clear();
    }

    PoseLibraryResourceManager(const PoseLibraryResourceManager&) = delete;
    PoseLibraryResourceManager& operator=(const PoseLibraryResourceManager&) = delete;

    // 真名（Model名）によって紐づけられたリソースの地図
    std::unordered_map<std::string, std::shared_ptr<PoseLibraryResource>> m_resources;
};