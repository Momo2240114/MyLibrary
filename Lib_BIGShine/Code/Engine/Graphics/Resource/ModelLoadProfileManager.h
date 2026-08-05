#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

enum class MODEL_LOAD_PROFILE_MODE
{
    // Profileを使わない。
    DISABLED,
    // 既存Profileを読むだけ。
    READ_ONLY,
    // 読み込み結果を履歴へ記録し、Profile生成も行う。
    READ_WRITE
};

struct ModelLoadHistorySample
{
    // 1回のModel読み込み計測結果。
    double totalLoadMilliseconds = 0.0;
    double fileReadMilliseconds = 0.0;
    double parseMilliseconds = 0.0;
    double cpuBuildMilliseconds = 0.0;
    double gpuCreateMilliseconds = 0.0;
    std::string recordedAt;
    std::string buildConfiguration;
    unsigned int workerCount = 0;
};

struct ModelLoadHistoryEntry
{
    // Model PathとFile状態ごとの履歴Sample。
    std::string modelPath;
    uintmax_t fileSize = 0;
    int64_t fileLastWriteTime = 0;
    std::vector<ModelLoadHistorySample> samples;
};

struct ModelLoadProfileEntry
{
    // 履歴から集計した読み込み推定値と統計値。
    double estimatedLoadMilliseconds = 0.0;
    double relativeLoadWeight = 1.0;
    double averageLoadMilliseconds = 0.0;
    double minimumLoadMilliseconds = 0.0;
    double maximumLoadMilliseconds = 0.0;
    double medianLoadMilliseconds = 0.0;
    double standardDeviationMilliseconds = 0.0;
    int sampleCount = 0;
    uintmax_t fileSize = 0;
    int64_t fileLastWriteTime = 0;
};

class ModelLoadProfileManager
{
public:
    // 集計済みProfile JSONを読み書きする。
    bool LoadProfile(const std::filesystem::path& filePath_);
    bool SaveProfile(const std::filesystem::path& filePath_) const;
    // Raw履歴JSONを読み書きする。
    bool LoadHistory(const std::filesystem::path& filePath_);
    bool SaveHistory(const std::filesystem::path& filePath_) const;
    // 履歴SampleからProfile値を再計算する。
    bool GenerateProfileFromHistory();

    // 1回のModel読み込み結果を履歴へ追加する。
    void RecordLoadResult(
        const std::filesystem::path& modelPath_,
        double loadMilliseconds_,
        unsigned int workerCount_);

    // 指定Modelの推定Load時間/相対重みを取得する。
    double GetEstimatedLoadMilliseconds(
        const std::filesystem::path& modelPath_) const;
    double GetRelativeLoadWeight(
        const std::filesystem::path& modelPath_) const;

    // 存在しないFileや古い状態のEntryを削除する。
    void RemoveInvalidEntries();
    void Clear();

private:
    // .tmp経由でJSONを安全に保存する。
    bool SaveJsonAtomically(
        const std::filesystem::path& filePath_,
        const nlohmann::json& json_) const;
    // Profile/History Key用にPath表記を正規化する。
    std::string NormalizePath(const std::filesystem::path& modelPath_) const;
    // Profileが無いModelを、近いFileSizeの履歴から推定する。
    double EstimateFromSimilarFiles(const std::filesystem::path& modelPath_) const;
    double GetAverageProfileMilliseconds() const;
    // FileSize/更新時刻を取得し、古いProfile判定に使う。
    uintmax_t GetFileSize(const std::filesystem::path& modelPath_) const;
    int64_t GetFileLastWriteTime(const std::filesystem::path& modelPath_) const;
    std::string MakeTimestamp() const;
    std::string GetBuildConfigurationName() const;

private:
    // Raw履歴と集計済みProfile。
    std::unordered_map<std::string, ModelLoadHistoryEntry> historyEntries;
    std::unordered_map<std::string, ModelLoadProfileEntry> profileEntries;
    // Profile/Historyへの並行アクセスを守る。
    mutable std::mutex profileMutex;
    // 1Modelあたり保持する履歴Sample数。
    size_t maximumHistorySampleCount = 32;
    static constexpr double defaultEstimatedLoadMilliseconds = 1000.0;
    static constexpr double minimumEstimatedLoadMilliseconds = 100.0;
    static constexpr double maximumEstimatedLoadMilliseconds = 30000.0;
};
