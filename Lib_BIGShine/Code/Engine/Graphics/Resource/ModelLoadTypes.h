#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class ModelResource;
class ModelResourceManager;

enum class MODEL_LOAD_STATE
{
    UNLOADED,
    WAITING,
    LOADING_CPU,
    WAITING_GPU_UPLOAD,
    READY,
    FAILED
};

enum class PRELOAD_PRIORITY
{
    LOW = 100,
    NORMAL = 200,
    HIGH = 300,
    CRITICAL = 400
};

struct ModelLoadEntry
{
    std::string normalizedPath;
    std::atomic<MODEL_LOAD_STATE> state = MODEL_LOAD_STATE::UNLOADED;
    std::shared_ptr<ModelResource> cpuResource;
    std::shared_ptr<ModelResource> resource;
    std::string errorMessage;
    int priority = 0;
    uint64_t requestOrder = 0;
    double estimatedLoadMilliseconds = 1000.0;
    double relativeLoadWeight = 1.0;
    double completedLoadMilliseconds = 0.0;
    std::chrono::steady_clock::time_point loadStartTime;
    mutable std::mutex mutex;
};

class ModelLoadHandle
{
public:
    ModelLoadHandle() = default;
    bool IsValid() const;
    bool IsReady() const;
    bool IsFailed() const;
    MODEL_LOAD_STATE GetState() const;
    std::shared_ptr<ModelResource> GetResource() const;
    std::string GetErrorMessage() const;

private:
    friend class ModelResourceManager;
    explicit ModelLoadHandle(const std::shared_ptr<ModelLoadEntry>& entry_);

    std::weak_ptr<ModelLoadEntry> entry;
};

struct PreloadGroupState
{
    std::string name;
    std::vector<ModelLoadHandle> handles;
};

class PreloadHandle
{
public:
    PreloadHandle() = default;
    bool IsValid() const;
    bool IsReady() const;
    bool HasFailed() const;
    float GetProgress() const;
    void PromotePriority(PRELOAD_PRIORITY priority_);

private:
    friend class ModelResourceManager;
    explicit PreloadHandle(
        const std::shared_ptr<PreloadGroupState>& group_,
        ModelResourceManager* manager_);

    std::weak_ptr<PreloadGroupState> group;
    ModelResourceManager* manager = nullptr;
};
