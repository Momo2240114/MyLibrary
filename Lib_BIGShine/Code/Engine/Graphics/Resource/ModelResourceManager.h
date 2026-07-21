#pragma once
#include <unordered_map>
#include <string>
#include <memory>
#include <d3d11.h>
#include "ModelResource.h"
#include "GLTFImporter.h"
#include "ModelLoadTypes.h"
#include "ModelLoadProfileManager.h"
#include "StartupResourceConfig.h"
#include <filesystem>
#include <map>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>

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

    ModelLoadHandle RequestResourceAsync(const std::string& filename_, int priority_);
    void UpdateAsyncLoads(ID3D11Device* device_, float gpuUploadBudgetMs_ = 2.0f);
    bool HasPendingLoads() const;
    float GetLoadProgress() const;
    void PromotePriority(const std::string& filename_, int priority_);
    void StartAsyncLoader();
    void ShutdownAsyncLoader();
    void FlushLoadProfile();
    PreloadHandle RequestPreloadGroup(const std::string& groupName_, PRELOAD_PRIORITY priority_);
    void PromotePreloadGroup(const std::string& groupName_, PRELOAD_PRIORITY priority_);
    bool IsPreloadGroupReady(const std::string& groupName_) const;
    float GetPreloadGroupProgress(const std::string& groupName_) const;
    void LoadPreloadSettings(const std::string& filename_ = "Data/Settings/AssetPreload.json");
    bool LoadStartupResourceSettings(const std::string& filename_ = "Data/Startup/StartupResources.json");
    PreloadHandle RequestStartupCriticalModels();
    void RequestStartupHighPriorityModels();

    // すべての記憶をリセットしてリソースをclear
    void Clear()
    {
        ShutdownAsyncLoader();
        for (auto& pair : m_resources) {
            if (pair.second) pair.second->Clear();
        }
        m_resources.clear();
        loadEntries.clear();
        preloadGroups.clear();
        preloadSettings.clear();
        startupResourceConfig.Clear();
        preloadSettingsLoaded = false;
        startupResourceSettingsLoaded = false;
        loadProfileManager.Clear();
        loadProfileLoaded = false;
        loadProfileDirty = false;
        enqueuedTaskCount = 0;
        completedCpuTaskCount = 0;
        waitingWorkerCount = 0;
        executingWorkerCount = 0;
    }

private:
    ModelResourceManager() = default;
    ~ModelResourceManager() { Clear(); }
    ModelResourceManager(const ModelResourceManager&) = delete;
    ModelResourceManager& operator=(const ModelResourceManager&) = delete;

    // シリアライズ・デシリアライズ
    bool SaveToCereal(const std::string& cerealPath, const std::shared_ptr<ModelResource>& resource);
    bool LoadFromCereal(const std::string& cerealPath, std::shared_ptr<ModelResource>& resource);
    std::string NormalizePath(const std::string& filename_) const;
    std::shared_ptr<ModelResource> LoadOrBuildCpuResource(const std::string& filename_, float sampleRate_);
    bool CreateGpuResource(ID3D11Device* device_, const std::string& filename_, const std::shared_ptr<ModelResource>& resource_);
    void QueueGpuUpload(const std::shared_ptr<ModelLoadEntry>& entry_);
    void WorkerLoop(unsigned int workerIndex_);
    std::shared_ptr<ModelLoadEntry> FindOrCreateEntry(const std::string& filename_, int priority_);
    void EnsureLoadProfileLoaded();
    unsigned int GetWorkerCount(size_t loadTaskCount_) const;

private:
    // Model名を鍵として、リソースのを保管する棚
    std::map<std::string, std::shared_ptr<ModelResource>> m_resources;

    struct ModelLoadJob
    {
        int priority = 0;
        double estimatedLoadMilliseconds = 1000.0;
        double relativeLoadWeight = 1.0;
        uint64_t requestOrder = 0;
        std::shared_ptr<ModelLoadEntry> entry;
    };

    struct PreloadAsset
    {
        std::string type;
        std::string path;
        int priority = static_cast<int>(PRELOAD_PRIORITY::NORMAL);
    };

    struct PreloadGroupSetting
    {
        std::string name;
        std::vector<PreloadAsset> assets;
    };

    mutable std::mutex asyncMutex;
    std::condition_variable asyncCondition;
    std::vector<ModelLoadJob> pendingJobs;
    std::deque<std::shared_ptr<ModelLoadEntry>> pendingGpuUploads;
    std::map<std::string, std::shared_ptr<ModelLoadEntry>> loadEntries;
    std::map<std::string, std::shared_ptr<PreloadGroupState>> preloadGroups;
    std::map<std::string, PreloadGroupSetting> preloadSettings;
    std::vector<std::thread> workerThreads;
    ModelLoadProfileManager loadProfileManager;
    StartupResourceConfig startupResourceConfig;
    std::filesystem::path loadHistoryPath = "Data/Cache/ModelLoadHistory.json";
    std::filesystem::path loadProfilePath = "Data/Config/ModelLoadProfile.json";
    bool workerRunning = false;
    bool workerStopRequested = false;
    uint64_t nextRequestOrder = 1;
    bool preloadSettingsLoaded = false;
    bool startupResourceSettingsLoaded = false;
    bool loadProfileLoaded = false;
    bool loadProfileDirty = false;
    unsigned int minimumModelLoadWorkerCount = 1;
    unsigned int maximumModelLoadWorkerCount = 8;
    unsigned int activeModelLoadWorkerCount = 0;
    std::atomic<uint64_t> enqueuedTaskCount = 0;
    std::atomic<uint64_t> completedCpuTaskCount = 0;
    std::atomic<uint32_t> waitingWorkerCount = 0;
    std::atomic<uint32_t> executingWorkerCount = 0;
};
