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
    // Model読み込みHandleの現在状態。
    UNLOADED,
    WAITING,
    LOADING_CPU,
    WAITING_GPU_UPLOAD,
    READY,
    FAILED
};

enum class PRELOAD_PRIORITY
{
    // 数値が大きいほどWorker側で先に処理される。
    LOW = 100,
    NORMAL = 200,
    HIGH = 300,
    CRITICAL = 400
};

struct ModelLoadEntry
{
    // 正規化済みModel Path。
    std::string normalizedPath;
    // 複数Threadから読むためatomicで状態を持つ。
    std::atomic<MODEL_LOAD_STATE> state = MODEL_LOAD_STATE::UNLOADED;
    // CPU読み込み完了後、GPU Upload待ちで保持するResource。
    std::shared_ptr<ModelResource> cpuResource;
    // GPU Resource作成まで完了した最終Resource。
    std::shared_ptr<ModelResource> resource;
    std::string errorMessage;
    // Priorityと要求順。Priority同値時の安定順に使う。
    int priority = 0;
    uint64_t requestOrder = 0;
    // Loading画面の進捗推定に使う時間/重み。
    double estimatedLoadMilliseconds = 1000.0;
    double relativeLoadWeight = 1.0;
    double completedLoadMilliseconds = 0.0;
    std::chrono::steady_clock::time_point loadStartTime;
    // Thread間でResource/Error更新を守るMutex。
    mutable std::mutex mutex;
};

class ModelLoadHandle
{
public:
    ModelLoadHandle() = default;
    // Handleが有効な読み込みEntryを指しているか。
    bool IsValid() const;
    // GPU Resourceまで準備済みか。
    bool IsReady() const;
    bool IsFailed() const;
    MODEL_LOAD_STATE GetState() const;
    std::shared_ptr<ModelResource> GetResource() const;
    std::string GetErrorMessage() const;

private:
    friend class ModelResourceManager;
    // ModelResourceManagerだけがEntry付きHandleを作る。
    explicit ModelLoadHandle(const std::shared_ptr<ModelLoadEntry>& entry_);

    // Entryの寿命はManager側が持つため、Handleはweak_ptrで参照する。
    std::weak_ptr<ModelLoadEntry> entry;
};

struct PreloadGroupState
{
    // 複数Modelをまとめて待つPreload Group。
    std::string name;
    std::vector<ModelLoadHandle> handles;
};

class PreloadHandle
{
public:
    PreloadHandle() = default;
    bool IsValid() const;
    // Group内の全ModelがReadyか。
    bool IsReady() const;
    // Group内にFailedが含まれるか。
    bool HasFailed() const;
    // 推定Load時間を重みにした進捗率を返す。
    float GetProgress() const;
    // Group内の未完了ModelのPriorityを引き上げる。
    void PromotePriority(PRELOAD_PRIORITY priority_);

private:
    friend class ModelResourceManager;
    explicit PreloadHandle(
        const std::shared_ptr<PreloadGroupState>& group_,
        ModelResourceManager* manager_);

    // Group状態と、それを操作するManagerへの参照。
    std::weak_ptr<PreloadGroupState> group;
    ModelResourceManager* manager = nullptr;
};
