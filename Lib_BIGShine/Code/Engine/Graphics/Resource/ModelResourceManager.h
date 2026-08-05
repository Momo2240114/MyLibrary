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

    // CPU 読み込みをワーカースレッドへ投げ、GPU アップロードは UpdateAsyncLoads 側で進める。
    ModelLoadHandle RequestResourceAsync(const std::string& filename_, int priority_);

    // メインスレッドで GPU リソース化を進める。budget を超えない範囲でキューを消化する。
    void UpdateAsyncLoads(ID3D11Device* device_, float gpuUploadBudgetMs_ = 2.0f);
    bool HasPendingLoads() const;
    float GetLoadProgress() const;

    // 既にキューにあるモデルの優先度を上げ、次のワーカー選択で先に処理されやすくする。
    void PromotePriority(const std::string& filename_, int priority_);

    // 非同期読み込み用ワーカーを必要数だけ起動する。
    void StartAsyncLoader();

    // ワーカー停止を要求し、残っているスレッドを安全に join する。
    void ShutdownAsyncLoader();

    // 計測した実ロード時間を履歴ファイルへ反映する。
    void FlushLoadProfile();

    // 設定ファイルに定義されたグループ単位で、複数モデルの先読みを依頼する。
    PreloadHandle RequestPreloadGroup(const std::string& groupName_, PRELOAD_PRIORITY priority_);
    void PromotePreloadGroup(const std::string& groupName_, PRELOAD_PRIORITY priority_);
    bool IsPreloadGroupReady(const std::string& groupName_) const;
    float GetPreloadGroupProgress(const std::string& groupName_) const;

    // 任意タイミングで使う先読みグループ設定を JSON から読み込む。
    void LoadPreloadSettings(const std::string& filename_ = "Data/Settings/AssetPreload.json");

    // 起動直後に必要なモデル一覧を読み込み、Critical/High の起動プリロードに使う。
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

    // パス表記ゆれを吸収し、m_resources や loadEntries のキーを安定させる。
    std::string NormalizePath(const std::string& filename_) const;

    // Cereal キャッシュがあれば復元し、無ければ GLTF から CPU 側 ModelResource を構築する。
    std::shared_ptr<ModelResource> LoadOrBuildCpuResource(const std::string& filename_, float sampleRate_);

    // CPU 側に読み込まれたメッシュ・テクスチャ・アニメーションを D3D11 リソースへ変換する。
    bool CreateGpuResource(ID3D11Device* device_, const std::string& filename_, const std::shared_ptr<ModelResource>& resource_);

    // ワーカーが完了したエントリをメインスレッド用アップロードキューへ渡す。
    void QueueGpuUpload(const std::shared_ptr<ModelLoadEntry>& entry_);

    // 優先度付きキューから仕事を取り出し、CPU 読み込みだけをバックグラウンドで行う。
    void WorkerLoop(unsigned int workerIndex_);

    // 同じモデルへの重複依頼をまとめ、ハンドルが共有する ModelLoadEntry を返す。
    std::shared_ptr<ModelLoadEntry> FindOrCreateEntry(const std::string& filename_, int priority_);

    // 初回アクセス時に履歴とプロファイルを読み込み、以降の優先度見積もりに使う。
    void EnsureLoadProfileLoaded();

    // ロード対象数に応じて起動するワーカー数を決め、過剰なスレッド生成を避ける。
    unsigned int GetWorkerCount(size_t loadTaskCount_) const;

private:
    // Model名を鍵として、リソースのを保管する棚
    std::map<std::string, std::shared_ptr<ModelResource>> m_resources;

    struct ModelLoadJob
    {
        // 数値が大きいほど先に処理する。estimatedLoadMilliseconds は同優先度内の並べ替え補助。
        int priority = 0;
        double estimatedLoadMilliseconds = 1000.0;
        double relativeLoadWeight = 1.0;

        // 同じ優先度の依頼を FIFO に近い順序で扱うための通し番号。
        uint64_t requestOrder = 0;
        std::shared_ptr<ModelLoadEntry> entry;
    };

    struct PreloadAsset
    {
        // 将来的に texture/audio などを含めるため type を持つ。現状は model が中心。
        std::string type;
        std::string path;
        int priority = static_cast<int>(PRELOAD_PRIORITY::NORMAL);
    };

    struct PreloadGroupSetting
    {
        // 例: StageStart、BossRoom など、ゲーム側が指定する先読み単位。
        std::string name;
        std::vector<PreloadAsset> assets;
    };

    // 非同期読み込み全体の共有状態。Worker と Main Thread の両方から触る。
    mutable std::mutex asyncMutex;
    std::condition_variable asyncCondition;

    // CPU ロード待ち、GPU アップロード待ち、外部ハンドルが参照するロード状態。
    std::vector<ModelLoadJob> pendingJobs;
    std::deque<std::shared_ptr<ModelLoadEntry>> pendingGpuUploads;
    std::map<std::string, std::shared_ptr<ModelLoadEntry>> loadEntries;
    std::map<std::string, std::shared_ptr<PreloadGroupState>> preloadGroups;
    std::map<std::string, PreloadGroupSetting> preloadSettings;
    std::vector<std::thread> workerThreads;

    // 起動時先読みと、過去のロード時間に基づく見積もりを保持する補助クラス。
    ModelLoadProfileManager loadProfileManager;
    StartupResourceConfig startupResourceConfig;
    std::filesystem::path loadHistoryPath = "Data/Cache/ModelLoadHistory.json";
    std::filesystem::path loadProfilePath = "Data/Config/ModelLoadProfile.json";

    // Worker の起動状態と停止要求。Shutdown 時は condition_variable で起こして抜けさせる。
    bool workerRunning = false;
    bool workerStopRequested = false;
    uint64_t nextRequestOrder = 1;

    // 設定やプロファイルの読み込み済み判定。毎回ディスクを読まないために保持する。
    bool preloadSettingsLoaded = false;
    bool startupResourceSettingsLoaded = false;
    bool loadProfileLoaded = false;
    bool loadProfileDirty = false;

    // CPU ロードの並列数。環境とタスク数に合わせて GetWorkerCount で制御する。
    unsigned int minimumModelLoadWorkerCount = 1;
    unsigned int maximumModelLoadWorkerCount = 8;
    unsigned int activeModelLoadWorkerCount = 0;

    // 進捗表示やデバッグ用のカウンタ。Worker から更新されるため atomic にする。
    std::atomic<uint64_t> enqueuedTaskCount = 0;
    std::atomic<uint64_t> completedCpuTaskCount = 0;
    std::atomic<uint32_t> waitingWorkerCount = 0;
    std::atomic<uint32_t> executingWorkerCount = 0;
};
