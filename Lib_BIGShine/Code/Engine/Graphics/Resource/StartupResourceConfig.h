#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

enum class STARTUP_RESOURCE_PRIORITY
{
    CRITICAL,
    HIGH
};

struct StartupModelEntry
{
    std::string resourceName;
    std::string filePath;
    STARTUP_RESOURCE_PRIORITY priority = STARTUP_RESOURCE_PRIORITY::HIGH;
    bool requiredBeforeTitle = false;
};

class StartupResourceConfig
{
public:
    bool Load(const std::string& filePath_);
    bool Save(const std::string& filePath_) const;
    void Clear();

    std::vector<StartupModelEntry>& GetCriticalModels() { return criticalModels; }
    std::vector<StartupModelEntry>& GetHighPriorityModels() { return highPriorityModels; }
    const std::vector<StartupModelEntry>& GetCriticalModels() const { return criticalModels; }
    const std::vector<StartupModelEntry>& GetHighPriorityModels() const { return highPriorityModels; }

    static std::string NormalizeResourcePath(const std::string& filePath_);
    static std::string MakeDefaultResourceName(const std::string& filePath_);
    static bool IsSupportedModelPath(const std::string& filePath_);
    static bool IsDataRelativePath(const std::string& filePath_);
    static bool ExistsOnDisk(const std::string& filePath_);
    static const char* PriorityToString(STARTUP_RESOURCE_PRIORITY priority_);
    static STARTUP_RESOURCE_PRIORITY StringToPriority(const std::string& priority_);

private:
    static StartupModelEntry MakeEntry(
        const std::string& resourceName_,
        const std::string& filePath_,
        STARTUP_RESOURCE_PRIORITY priority_);
    static bool SaveJsonAtomically(const std::string& filePath_, const nlohmann::json& json_);

private:
    std::vector<StartupModelEntry> criticalModels;
    std::vector<StartupModelEntry> highPriorityModels;
};
