#pragma once

#include "EditorWindow.h"
#include "Engine/Graphics/Resource/StartupResourceConfig.h"

#include <string>
#include <vector>

class StartupResourceEditorWindow : public EditorWindow
{
public:
    explicit StartupResourceEditorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    void DrawToolbar();
    void DrawCriticalModelSection();
    void DrawHighPriorityModelSection();
    void DrawModelPathList(
        const char* childId_,
        std::vector<StartupModelEntry>& entries_,
        STARTUP_RESOURCE_PRIORITY priority_);
    void DrawValidationResults();

    void OpenModelFileDialog(STARTUP_RESOURCE_PRIORITY priority_);
    void AddModelPath(const std::string& filePath_, STARTUP_RESOURCE_PRIORITY priority_);
    void SetModelPath(
        STARTUP_RESOURCE_PRIORITY priority_,
        size_t index_,
        const std::string& filePath_);
    void RemoveModelPath(STARTUP_RESOURCE_PRIORITY priority_, size_t index_);
    void MoveModelPath(STARTUP_RESOURCE_PRIORITY priority_, size_t fromIndex_, size_t toIndex_);
    void ChangePriority(STARTUP_RESOURCE_PRIORITY priority_, size_t index_);

    bool LoadSettings();
    bool SaveSettings();
    void ValidateSettings();
    void MarkUnsaved();

    std::vector<StartupModelEntry>& GetEntries(STARTUP_RESOURCE_PRIORITY priority_);
    const std::vector<StartupModelEntry>& GetEntries(STARTUP_RESOURCE_PRIORITY priority_) const;
    bool HasPathInAnySection(const std::string& filePath_, STARTUP_RESOURCE_PRIORITY* foundPriority_ = nullptr, size_t* foundIndex_ = nullptr) const;
    bool HasResourceNameDuplicate(const std::string& resourceName_, STARTUP_RESOURCE_PRIORITY priority_, size_t index_) const;
    void NormalizeAllEntries();

private:
    StartupResourceConfig config;
    std::string configFilePath = "Data/Startup/StartupResources.json";
    std::string statusMessage;
    std::vector<std::string> validationResults;
    bool hasUnsavedChanges = false;
};
