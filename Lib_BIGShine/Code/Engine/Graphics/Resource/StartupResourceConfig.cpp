#include "PrecompiledHeader.h"

#include "StartupResourceConfig.h"
#include "Engine/Common/DebugLog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace
{
    constexpr int STARTUP_RESOURCE_CONFIG_VERSION = 1;

    std::string ToLowerString(std::string text_)
    {
        std::transform(text_.begin(), text_.end(), text_.begin(),
            [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
        return text_;
    }
}

bool StartupResourceConfig::Load(const std::string& filePath_)
{
    Clear();

    std::ifstream file_(filePath_);
    if (!file_.is_open())
    {
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[StartupResourceConfig] Missing startup resource config: " + filePath_);
        return false;
    }

    try
    {
        nlohmann::json json_;
        file_ >> json_;

        auto readArray_ =
            [this](const nlohmann::json& array_, STARTUP_RESOURCE_PRIORITY priority_)
            {
                if (!array_.is_array()) return;
                for (const nlohmann::json& item_ : array_)
                {
                    std::string filePath_ = item_.value("filePath", std::string());
                    if (filePath_.empty()) filePath_ = item_.value("Path", std::string());

                    const std::string normalizedPath_ = NormalizeResourcePath(filePath_);
                    if (normalizedPath_.empty())
                    {
                        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[StartupResourceConfig] Skipped empty startup model path.");
                        continue;
                    }

                    if (!IsSupportedModelPath(normalizedPath_))
                    {
                        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[StartupResourceConfig] Unsupported startup model path: " + normalizedPath_);
                        continue;
                    }

                    std::string resourceName_ = item_.value("resourceName", std::string());
                    if (resourceName_.empty()) resourceName_ = item_.value("ResourceName", std::string());
                    if (resourceName_.empty()) resourceName_ = MakeDefaultResourceName(normalizedPath_);

                    StartupModelEntry entry_ = MakeEntry(resourceName_, normalizedPath_, priority_);
                    if (priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL)
                    {
                        criticalModels.push_back(entry_);
                    }
                    else
                    {
                        highPriorityModels.push_back(entry_);
                    }
                }
            };

        readArray_(json_.value("criticalModels", nlohmann::json::array()), STARTUP_RESOURCE_PRIORITY::CRITICAL);
        readArray_(json_.value("highPriorityModels", nlohmann::json::array()), STARTUP_RESOURCE_PRIORITY::HIGH);

        const nlohmann::json resources_ = json_.value("resources", nlohmann::json::array());
        if (resources_.is_array())
        {
            for (const nlohmann::json& item_ : resources_)
            {
                const STARTUP_RESOURCE_PRIORITY priority_ =
                    StringToPriority(item_.value("priority", std::string("HIGH")));
                const bool requiredBeforeTitle_ =
                    item_.value("requiredBeforeTitle", priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL);
                readArray_(nlohmann::json::array({ item_ }),
                    requiredBeforeTitle_ ? STARTUP_RESOURCE_PRIORITY::CRITICAL : priority_);
            }
        }
    }
    catch (const std::exception& exception_)
    {
        Clear();
        BS_LOG_ERROR(LOG_CATEGORY::GAME, "[StartupResourceConfig] Parse failed: " + std::string(exception_.what()));
        return false;
    }
    catch (...)
    {
        Clear();
        BS_LOG_ERROR(LOG_CATEGORY::GAME, "[StartupResourceConfig] Unknown parse error.");
        return false;
    }

    return true;
}

bool StartupResourceConfig::Save(const std::string& filePath_) const
{
    nlohmann::json json_;
    json_["version"] = STARTUP_RESOURCE_CONFIG_VERSION;
    json_["criticalModels"] = nlohmann::json::array();
    json_["highPriorityModels"] = nlohmann::json::array();

    auto writeArray_ =
        [](nlohmann::json& array_, const std::vector<StartupModelEntry>& entries_)
        {
            for (const StartupModelEntry& entry_ : entries_)
            {
                array_.push_back(
                    {
                        { "resourceName", entry_.resourceName },
                        { "filePath", NormalizeResourcePath(entry_.filePath) }
                    });
            }
        };

    writeArray_(json_["criticalModels"], criticalModels);
    writeArray_(json_["highPriorityModels"], highPriorityModels);

    if (!SaveJsonAtomically(filePath_, json_))
    {
        BS_LOG_ERROR(LOG_CATEGORY::GAME, "[StartupResourceConfig] Save failed: " + filePath_);
        return false;
    }

    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[StartupResourceConfig] Saved: " + filePath_);
    return true;
}

void StartupResourceConfig::Clear()
{
    criticalModels.clear();
    highPriorityModels.clear();
}

std::string StartupResourceConfig::NormalizeResourcePath(const std::string& filePath_)
{
    if (filePath_.empty()) return {};

    std::filesystem::path path_(filePath_);
    std::error_code errorCode_;
    if (path_.is_absolute())
    {
        const std::filesystem::path basePath_ = std::filesystem::current_path(errorCode_);
        if (errorCode_) return {};
        const std::filesystem::path relativePath_ = std::filesystem::relative(path_, basePath_, errorCode_);
        if (errorCode_ || relativePath_.empty()) return {};
        path_ = relativePath_;
    }

    std::string normalizedPath_ = path_.lexically_normal().generic_string();
    while (!normalizedPath_.empty() && normalizedPath_.starts_with("./"))
    {
        normalizedPath_.erase(0, 2);
    }
    return normalizedPath_;
}

std::string StartupResourceConfig::MakeDefaultResourceName(const std::string& filePath_)
{
    return std::filesystem::path(filePath_).stem().generic_string();
}

bool StartupResourceConfig::IsSupportedModelPath(const std::string& filePath_)
{
    const std::string extension_ = ToLowerString(std::filesystem::path(filePath_).extension().generic_string());
    return extension_ == ".gltf" || extension_ == ".glb";
}

bool StartupResourceConfig::IsDataRelativePath(const std::string& filePath_)
{
    const std::string normalizedPath_ = NormalizeResourcePath(filePath_);
    const std::string lowerPath_ = ToLowerString(normalizedPath_);
    return lowerPath_ == "data" || lowerPath_.starts_with("data/");
}

bool StartupResourceConfig::ExistsOnDisk(const std::string& filePath_)
{
    std::error_code errorCode_;
    return std::filesystem::exists(std::filesystem::path(NormalizeResourcePath(filePath_)), errorCode_) && !errorCode_;
}

const char* StartupResourceConfig::PriorityToString(STARTUP_RESOURCE_PRIORITY priority_)
{
    switch (priority_)
    {
    case STARTUP_RESOURCE_PRIORITY::CRITICAL:
        return "CRITICAL";
    case STARTUP_RESOURCE_PRIORITY::HIGH:
    default:
        return "HIGH";
    }
}

STARTUP_RESOURCE_PRIORITY StartupResourceConfig::StringToPriority(const std::string& priority_)
{
    const std::string upperPriority_ = ToLowerString(priority_);
    return upperPriority_ == "critical"
        ? STARTUP_RESOURCE_PRIORITY::CRITICAL
        : STARTUP_RESOURCE_PRIORITY::HIGH;
}

StartupModelEntry StartupResourceConfig::MakeEntry(
    const std::string& resourceName_,
    const std::string& filePath_,
    STARTUP_RESOURCE_PRIORITY priority_)
{
    StartupModelEntry entry_;
    entry_.resourceName = resourceName_;
    entry_.filePath = NormalizeResourcePath(filePath_);
    entry_.priority = priority_;
    entry_.requiredBeforeTitle = priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL;
    return entry_;
}

bool StartupResourceConfig::SaveJsonAtomically(const std::string& filePath_, const nlohmann::json& json_)
{
    try
    {
        const std::filesystem::path outputPath_(filePath_);
        const std::filesystem::path parentPath_ = outputPath_.parent_path();
        if (!parentPath_.empty()) std::filesystem::create_directories(parentPath_);

        std::filesystem::path temporaryPath_ = outputPath_;
        temporaryPath_ += ".tmp";

        {
            std::ofstream file_(temporaryPath_);
            if (!file_.is_open()) return false;
            file_ << json_.dump(4);
            if (!file_.good()) return false;
        }

        std::error_code errorCode_;
        std::filesystem::rename(temporaryPath_, outputPath_, errorCode_);
        if (errorCode_)
        {
            std::filesystem::remove(outputPath_, errorCode_);
            errorCode_.clear();
            std::filesystem::rename(temporaryPath_, outputPath_, errorCode_);
        }
        return !errorCode_;
    }
    catch (...)
    {
        return false;
    }
}
