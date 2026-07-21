#include "PrecompiledHeader.h"

#include "ModelLoadProfileManager.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace
{
    constexpr int MODEL_LOAD_HISTORY_VERSION = 1;
    constexpr int MODEL_LOAD_PROFILE_VERSION = 1;
    constexpr double MODEL_LOAD_ABSOLUTE_SAMPLE_LIMIT_MS = 15000.0;
    constexpr double MODEL_LOAD_OUTLIER_MEDIAN_MULTIPLIER = 3.0;
    constexpr double MODEL_LOAD_OUTLIER_MINIMUM_MARGIN_MS = 2000.0;

    double CalculateMedian(const std::vector<double>& sortedValues_)
    {
        if (sortedValues_.empty()) return 0.0;
        return sortedValues_.size() % 2 == 0
            ? (sortedValues_[sortedValues_.size() / 2 - 1] + sortedValues_[sortedValues_.size() / 2]) * 0.5
            : sortedValues_[sortedValues_.size() / 2];
    }

    double CalculateOutlierLimit(double median_)
    {
        return (std::max)(
            median_ * MODEL_LOAD_OUTLIER_MEDIAN_MULTIPLIER,
            median_ + MODEL_LOAD_OUTLIER_MINIMUM_MARGIN_MS);
    }

    void RemoveLongLoadOutliers(std::vector<double>& values_)
    {
        values_.erase(
            std::remove_if(
                values_.begin(),
                values_.end(),
                [](double value_)
                {
                    return value_ <= 0.0 || value_ > MODEL_LOAD_ABSOLUTE_SAMPLE_LIMIT_MS;
                }),
            values_.end());

        if (values_.size() < 4) return;

        std::sort(values_.begin(), values_.end());
        const double median_ = CalculateMedian(values_);
        const double outlierLimit_ = CalculateOutlierLimit(median_);
        values_.erase(
            std::remove_if(
                values_.begin(),
                values_.end(),
                [outlierLimit_](double value_)
                {
                    return value_ > outlierLimit_;
                }),
            values_.end());
    }
}

bool ModelLoadProfileManager::LoadProfile(const std::filesystem::path& filePath_)
{
    std::lock_guard<std::mutex> lock_(profileMutex);
    profileEntries.clear();

    std::ifstream file_(filePath_);
    if (!file_.is_open()) return false;

    try
    {
        nlohmann::json json_;
        file_ >> json_;
        if (json_.value("version", 0) != MODEL_LOAD_PROFILE_VERSION) return false;

        const nlohmann::json models_ = json_.value("models", nlohmann::json::object());
        for (auto iterator_ = models_.begin(); iterator_ != models_.end(); ++iterator_)
        {
            const nlohmann::json& item_ = iterator_.value();
            ModelLoadProfileEntry entry_;
            entry_.estimatedLoadMilliseconds = item_.value("estimatedLoadMilliseconds", 0.0);
            entry_.relativeLoadWeight = item_.value("relativeLoadWeight", 1.0);
            entry_.sampleCount = item_.value("sampleCount", 0);
            entry_.minimumLoadMilliseconds = item_.value("minimumLoadMilliseconds", 0.0);
            entry_.maximumLoadMilliseconds = item_.value("maximumLoadMilliseconds", 0.0);
            entry_.medianLoadMilliseconds = item_.value("medianLoadMilliseconds", 0.0);
            entry_.averageLoadMilliseconds = item_.value("averageLoadMilliseconds", 0.0);
            entry_.standardDeviationMilliseconds = item_.value("standardDeviationMilliseconds", 0.0);
            entry_.fileSize = item_.value("fileSize", static_cast<uintmax_t>(0));
            entry_.fileLastWriteTime = item_.value("fileLastWriteTime", static_cast<int64_t>(0));
            if (entry_.estimatedLoadMilliseconds <= 0.0) continue;
            profileEntries[iterator_.key()] = entry_;
        }
    }
    catch (...)
    {
        profileEntries.clear();
        return false;
    }

    return true;
}

bool ModelLoadProfileManager::SaveProfile(const std::filesystem::path& filePath_) const
{
    std::lock_guard<std::mutex> lock_(profileMutex);

    nlohmann::json models_ = nlohmann::json::object();
    for (const auto& pair_ : profileEntries)
    {
        const ModelLoadProfileEntry& entry_ = pair_.second;
        models_[pair_.first] =
        {
            { "estimatedLoadMilliseconds", entry_.estimatedLoadMilliseconds },
            { "relativeLoadWeight", entry_.relativeLoadWeight },
            { "sampleCount", entry_.sampleCount },
            { "minimumLoadMilliseconds", entry_.minimumLoadMilliseconds },
            { "maximumLoadMilliseconds", entry_.maximumLoadMilliseconds },
            { "medianLoadMilliseconds", entry_.medianLoadMilliseconds },
            { "averageLoadMilliseconds", entry_.averageLoadMilliseconds },
            { "standardDeviationMilliseconds", entry_.standardDeviationMilliseconds },
            { "fileSize", entry_.fileSize },
            { "fileLastWriteTime", entry_.fileLastWriteTime }
        };
    }

    nlohmann::json json_;
    json_["version"] = MODEL_LOAD_PROFILE_VERSION;
    json_["generatedAt"] = MakeTimestamp();
    json_["sourceHistoryVersion"] = MODEL_LOAD_HISTORY_VERSION;
    json_["models"] = models_;

    return SaveJsonAtomically(filePath_, json_);
}

bool ModelLoadProfileManager::LoadHistory(const std::filesystem::path& filePath_)
{
    std::lock_guard<std::mutex> lock_(profileMutex);
    historyEntries.clear();

    std::ifstream file_(filePath_);
    if (!file_.is_open()) return false;

    try
    {
        nlohmann::json json_;
        file_ >> json_;
        if (json_.value("version", 0) != MODEL_LOAD_HISTORY_VERSION) return false;

        const nlohmann::json models_ = json_.value("models", nlohmann::json::object());
        for (auto iterator_ = models_.begin(); iterator_ != models_.end(); ++iterator_)
        {
            const nlohmann::json& item_ = iterator_.value();
            ModelLoadHistoryEntry entry_;
            entry_.modelPath = iterator_.key();
            entry_.fileSize = item_.value("fileSize", static_cast<uintmax_t>(0));
            entry_.fileLastWriteTime = item_.value("fileLastWriteTime", static_cast<int64_t>(0));

            for (const nlohmann::json& sampleJson_ : item_.value("samples", nlohmann::json::array()))
            {
                ModelLoadHistorySample sample_;
                sample_.totalLoadMilliseconds = sampleJson_.value("totalLoadMilliseconds", 0.0);
                sample_.fileReadMilliseconds = sampleJson_.value("fileReadMilliseconds", 0.0);
                sample_.parseMilliseconds = sampleJson_.value("parseMilliseconds", 0.0);
                sample_.cpuBuildMilliseconds = sampleJson_.value("cpuBuildMilliseconds", 0.0);
                sample_.gpuCreateMilliseconds = sampleJson_.value("gpuCreateMilliseconds", 0.0);
                sample_.recordedAt = sampleJson_.value("recordedAt", std::string());
                sample_.buildConfiguration = sampleJson_.value("buildConfiguration", std::string());
                sample_.workerCount = sampleJson_.value("workerCount", 0u);
                if (sample_.totalLoadMilliseconds > 0.0) entry_.samples.push_back(sample_);
            }

            if (!entry_.samples.empty()) historyEntries[entry_.modelPath] = entry_;
        }
    }
    catch (...)
    {
        historyEntries.clear();
        return false;
    }

    return true;
}

bool ModelLoadProfileManager::SaveHistory(const std::filesystem::path& filePath_) const
{
    std::lock_guard<std::mutex> lock_(profileMutex);

    nlohmann::json models_ = nlohmann::json::object();
    for (const auto& pair_ : historyEntries)
    {
        const ModelLoadHistoryEntry& entry_ = pair_.second;
        nlohmann::json samples_ = nlohmann::json::array();
        for (const ModelLoadHistorySample& sample_ : entry_.samples)
        {
            samples_.push_back(
                {
                    { "totalLoadMilliseconds", sample_.totalLoadMilliseconds },
                    { "fileReadMilliseconds", sample_.fileReadMilliseconds },
                    { "parseMilliseconds", sample_.parseMilliseconds },
                    { "cpuBuildMilliseconds", sample_.cpuBuildMilliseconds },
                    { "gpuCreateMilliseconds", sample_.gpuCreateMilliseconds },
                    { "recordedAt", sample_.recordedAt },
                    { "buildConfiguration", sample_.buildConfiguration },
                    { "workerCount", sample_.workerCount }
                });
        }

        models_[pair_.first] =
        {
            { "fileSize", entry_.fileSize },
            { "fileLastWriteTime", entry_.fileLastWriteTime },
            { "samples", samples_ }
        };
    }

    nlohmann::json json_;
    json_["version"] = MODEL_LOAD_HISTORY_VERSION;
    json_["models"] = models_;

    return SaveJsonAtomically(filePath_, json_);
}

bool ModelLoadProfileManager::GenerateProfileFromHistory()
{
    std::lock_guard<std::mutex> lock_(profileMutex);
    profileEntries.clear();

    double averageMillisecondsSum_ = 0.0;
    int validModelCount_ = 0;
    std::vector<std::pair<std::string, double>> averages_;

    for (const auto& pair_ : historyEntries)
    {
        const ModelLoadHistoryEntry& historyEntry_ = pair_.second;
        std::vector<double> values_;
        for (const ModelLoadHistorySample& sample_ : historyEntry_.samples)
        {
            if (sample_.totalLoadMilliseconds > 0.0)
            {
                values_.push_back(sample_.totalLoadMilliseconds);
            }
        }
        RemoveLongLoadOutliers(values_);
        if (values_.empty()) continue;

        std::sort(values_.begin(), values_.end());
        const double sum_ = std::accumulate(values_.begin(), values_.end(), 0.0);
        const double average_ = sum_ / static_cast<double>(values_.size());
        const double median_ = CalculateMedian(values_);

        double variance_ = 0.0;
        for (const double value_ : values_)
        {
            const double difference_ = value_ - average_;
            variance_ += difference_ * difference_;
        }
        variance_ /= static_cast<double>(values_.size());

        ModelLoadProfileEntry profileEntry_;
        profileEntry_.sampleCount = static_cast<int>(values_.size());
        profileEntry_.minimumLoadMilliseconds = values_.front();
        profileEntry_.maximumLoadMilliseconds = values_.back();
        profileEntry_.medianLoadMilliseconds = median_;
        profileEntry_.averageLoadMilliseconds = average_;
        profileEntry_.standardDeviationMilliseconds = std::sqrt(variance_);
        profileEntry_.estimatedLoadMilliseconds =
            std::clamp(
                (std::max)(median_, average_),
                minimumEstimatedLoadMilliseconds,
                maximumEstimatedLoadMilliseconds);
        profileEntry_.fileSize = historyEntry_.fileSize;
        profileEntry_.fileLastWriteTime = historyEntry_.fileLastWriteTime;
        profileEntries[pair_.first] = profileEntry_;

        averageMillisecondsSum_ += profileEntry_.estimatedLoadMilliseconds;
        ++validModelCount_;
        averages_.push_back({ pair_.first, profileEntry_.estimatedLoadMilliseconds });
    }

    const double baseMilliseconds_ = validModelCount_ > 0
        ? averageMillisecondsSum_ / static_cast<double>(validModelCount_)
        : defaultEstimatedLoadMilliseconds;

    for (const auto& pair_ : averages_)
    {
        profileEntries[pair_.first].relativeLoadWeight =
            baseMilliseconds_ > 0.0 ? pair_.second / baseMilliseconds_ : 1.0;
    }

    return !profileEntries.empty();
}

void ModelLoadProfileManager::RecordLoadResult(
    const std::filesystem::path& modelPath_,
    double loadMilliseconds_,
    unsigned int workerCount_)
{
    if (loadMilliseconds_ <= 0.0) return;

    const std::string normalizedPath_ = NormalizePath(modelPath_);
    std::lock_guard<std::mutex> lock_(profileMutex);

    auto profileFound_ = profileEntries.find(normalizedPath_);
    if (profileFound_ != profileEntries.end() &&
        profileFound_->second.medianLoadMilliseconds > 0.0)
    {
        const double outlierLimit_ =
            CalculateOutlierLimit(profileFound_->second.medianLoadMilliseconds);
        if (loadMilliseconds_ > outlierLimit_) return;
    }
    else if (loadMilliseconds_ > MODEL_LOAD_ABSOLUTE_SAMPLE_LIMIT_MS)
    {
        return;
    }

    ModelLoadHistoryEntry& entry_ = historyEntries[normalizedPath_];
    entry_.modelPath = normalizedPath_;
    entry_.fileSize = GetFileSize(modelPath_);
    entry_.fileLastWriteTime = GetFileLastWriteTime(modelPath_);

    ModelLoadHistorySample sample_;
    sample_.totalLoadMilliseconds = loadMilliseconds_;
    sample_.cpuBuildMilliseconds = loadMilliseconds_;
    sample_.recordedAt = MakeTimestamp();
    sample_.buildConfiguration = GetBuildConfigurationName();
    sample_.workerCount = workerCount_;
    entry_.samples.push_back(sample_);

    while (entry_.samples.size() > maximumHistorySampleCount)
    {
        entry_.samples.erase(entry_.samples.begin());
    }
}

double ModelLoadProfileManager::GetEstimatedLoadMilliseconds(
    const std::filesystem::path& modelPath_) const
{
    const std::string normalizedPath_ = NormalizePath(modelPath_);
    std::lock_guard<std::mutex> lock_(profileMutex);

    auto found_ = profileEntries.find(normalizedPath_);
    if (found_ != profileEntries.end())
    {
        return std::clamp(
            found_->second.estimatedLoadMilliseconds,
            minimumEstimatedLoadMilliseconds,
            maximumEstimatedLoadMilliseconds);
    }

    const double similarEstimate_ = EstimateFromSimilarFiles(modelPath_);
    return std::clamp(
        similarEstimate_,
        minimumEstimatedLoadMilliseconds,
        maximumEstimatedLoadMilliseconds);
}

double ModelLoadProfileManager::GetRelativeLoadWeight(
    const std::filesystem::path& modelPath_) const
{
    const std::string normalizedPath_ = NormalizePath(modelPath_);
    std::lock_guard<std::mutex> lock_(profileMutex);

    auto found_ = profileEntries.find(normalizedPath_);
    if (found_ != profileEntries.end())
    {
        return (std::max)(0.1, found_->second.relativeLoadWeight);
    }

    const double averageMilliseconds_ = GetAverageProfileMilliseconds();
    if (averageMilliseconds_ <= 0.0) return 1.0;
    return (std::max)(0.1, EstimateFromSimilarFiles(modelPath_) / averageMilliseconds_);
}

void ModelLoadProfileManager::RemoveInvalidEntries()
{
    std::lock_guard<std::mutex> lock_(profileMutex);
    for (auto iterator_ = profileEntries.begin(); iterator_ != profileEntries.end();)
    {
        if (iterator_->second.estimatedLoadMilliseconds <= 0.0)
        {
            iterator_ = profileEntries.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }

    for (auto iterator_ = historyEntries.begin(); iterator_ != historyEntries.end();)
    {
        if (iterator_->second.samples.empty())
        {
            iterator_ = historyEntries.erase(iterator_);
        }
        else
        {
            ++iterator_;
        }
    }
}

void ModelLoadProfileManager::Clear()
{
    std::lock_guard<std::mutex> lock_(profileMutex);
    historyEntries.clear();
    profileEntries.clear();
}

bool ModelLoadProfileManager::SaveJsonAtomically(
    const std::filesystem::path& filePath_,
    const nlohmann::json& json_) const
{
    try
    {
        const std::filesystem::path parentPath_ = filePath_.parent_path();
        if (!parentPath_.empty()) std::filesystem::create_directories(parentPath_);

        std::filesystem::path temporaryPath_ = filePath_;
        temporaryPath_ += ".tmp";

        {
            std::ofstream file_(temporaryPath_);
            if (!file_.is_open()) return false;
            file_ << json_.dump(4);
            if (!file_.good()) return false;
        }

        std::error_code errorCode_;
        std::filesystem::rename(temporaryPath_, filePath_, errorCode_);
        if (errorCode_)
        {
            std::filesystem::remove(filePath_, errorCode_);
            errorCode_.clear();
            std::filesystem::rename(temporaryPath_, filePath_, errorCode_);
        }
        return !errorCode_;
    }
    catch (...)
    {
        return false;
    }
}

std::string ModelLoadProfileManager::NormalizePath(
    const std::filesystem::path& modelPath_) const
{
    std::string normalizedPath_ = modelPath_.lexically_normal().generic_string();
    while (!normalizedPath_.empty() && normalizedPath_.starts_with("./"))
    {
        normalizedPath_.erase(0, 2);
    }
#ifdef _WIN32
    std::transform(normalizedPath_.begin(), normalizedPath_.end(), normalizedPath_.begin(),
        [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
#endif
    return normalizedPath_;
}

double ModelLoadProfileManager::EstimateFromSimilarFiles(
    const std::filesystem::path& modelPath_) const
{
    const std::string extension_ = modelPath_.extension().generic_string();
    const uintmax_t fileSize_ = GetFileSize(modelPath_);

    double weightedMilliseconds_ = 0.0;
    double totalWeight_ = 0.0;
    double allMilliseconds_ = 0.0;
    int allCount_ = 0;

    for (const auto& pair_ : profileEntries)
    {
        const ModelLoadProfileEntry& entry_ = pair_.second;
        if (entry_.estimatedLoadMilliseconds <= 0.0) continue;

        allMilliseconds_ += entry_.estimatedLoadMilliseconds;
        ++allCount_;

        if (!extension_.empty() &&
            std::filesystem::path(pair_.first).extension().generic_string() != extension_)
        {
            continue;
        }

        double weight_ = 1.0;
        if (fileSize_ > 0 && entry_.fileSize > 0)
        {
            const double largerSize_ = static_cast<double>((std::max)(fileSize_, entry_.fileSize));
            const double smallerSize_ = static_cast<double>((std::min)(fileSize_, entry_.fileSize));
            weight_ = smallerSize_ / largerSize_;
        }
        weightedMilliseconds_ += entry_.estimatedLoadMilliseconds * weight_;
        totalWeight_ += weight_;
    }

    if (totalWeight_ > 0.0) return weightedMilliseconds_ / totalWeight_;
    if (allCount_ > 0) return allMilliseconds_ / static_cast<double>(allCount_);

    if (fileSize_ > 0)
    {
        const double megabytes_ = static_cast<double>(fileSize_) / (1024.0 * 1024.0);
        return (std::max)(defaultEstimatedLoadMilliseconds, megabytes_ * 80.0);
    }

    return defaultEstimatedLoadMilliseconds;
}

double ModelLoadProfileManager::GetAverageProfileMilliseconds() const
{
    if (profileEntries.empty()) return defaultEstimatedLoadMilliseconds;

    double milliseconds_ = 0.0;
    int count_ = 0;
    for (const auto& pair_ : profileEntries)
    {
        if (pair_.second.estimatedLoadMilliseconds <= 0.0) continue;
        milliseconds_ += pair_.second.estimatedLoadMilliseconds;
        ++count_;
    }

    return count_ > 0
        ? milliseconds_ / static_cast<double>(count_)
        : defaultEstimatedLoadMilliseconds;
}

uintmax_t ModelLoadProfileManager::GetFileSize(
    const std::filesystem::path& modelPath_) const
{
    std::error_code errorCode_;
    if (!std::filesystem::exists(modelPath_, errorCode_)) return 0;
    const uintmax_t fileSize_ = std::filesystem::file_size(modelPath_, errorCode_);
    return errorCode_ ? 0 : fileSize_;
}

int64_t ModelLoadProfileManager::GetFileLastWriteTime(
    const std::filesystem::path& modelPath_) const
{
    std::error_code errorCode_;
    const auto writeTime_ = std::filesystem::last_write_time(modelPath_, errorCode_);
    if (errorCode_) return 0;
    return std::chrono::duration_cast<std::chrono::seconds>(
        writeTime_.time_since_epoch()).count();
}

std::string ModelLoadProfileManager::MakeTimestamp() const
{
    const auto now_ = std::chrono::system_clock::now();
    const std::time_t time_ = std::chrono::system_clock::to_time_t(now_);
    std::tm localTime_{};
#ifdef _WIN32
    localtime_s(&localTime_, &time_);
#else
    localtime_r(&time_, &localTime_);
#endif
    std::ostringstream stream_;
    stream_ << std::put_time(&localTime_, "%Y-%m-%dT%H:%M:%S");
    return stream_.str();
}

std::string ModelLoadProfileManager::GetBuildConfigurationName() const
{
#if defined(_DEBUG)
    return "DEBUG";
#elif defined(BS_DEVELOPMENT_BUILD)
    return "DEVELOPMENT";
#else
    return "RELEASE";
#endif
}
