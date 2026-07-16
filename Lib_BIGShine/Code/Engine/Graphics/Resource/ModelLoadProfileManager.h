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
    DISABLED,
    READ_ONLY,
    READ_WRITE
};

struct ModelLoadHistorySample
{
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
    std::string modelPath;
    uintmax_t fileSize = 0;
    int64_t fileLastWriteTime = 0;
    std::vector<ModelLoadHistorySample> samples;
};

struct ModelLoadProfileEntry
{
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
    bool LoadProfile(const std::filesystem::path& filePath_);
    bool SaveProfile(const std::filesystem::path& filePath_) const;
    bool LoadHistory(const std::filesystem::path& filePath_);
    bool SaveHistory(const std::filesystem::path& filePath_) const;
    bool GenerateProfileFromHistory();

    void RecordLoadResult(
        const std::filesystem::path& modelPath_,
        double loadMilliseconds_,
        unsigned int workerCount_);

    double GetEstimatedLoadMilliseconds(
        const std::filesystem::path& modelPath_) const;
    double GetRelativeLoadWeight(
        const std::filesystem::path& modelPath_) const;

    void RemoveInvalidEntries();
    void Clear();

private:
    bool SaveJsonAtomically(
        const std::filesystem::path& filePath_,
        const nlohmann::json& json_) const;
    std::string NormalizePath(const std::filesystem::path& modelPath_) const;
    double EstimateFromSimilarFiles(const std::filesystem::path& modelPath_) const;
    double GetAverageProfileMilliseconds() const;
    uintmax_t GetFileSize(const std::filesystem::path& modelPath_) const;
    int64_t GetFileLastWriteTime(const std::filesystem::path& modelPath_) const;
    std::string MakeTimestamp() const;
    std::string GetBuildConfigurationName() const;

private:
    std::unordered_map<std::string, ModelLoadHistoryEntry> historyEntries;
    std::unordered_map<std::string, ModelLoadProfileEntry> profileEntries;
    mutable std::mutex profileMutex;
    size_t maximumHistorySampleCount = 32;
    static constexpr double defaultEstimatedLoadMilliseconds = 1000.0;
    static constexpr double minimumEstimatedLoadMilliseconds = 100.0;
    static constexpr double maximumEstimatedLoadMilliseconds = 30000.0;
};
