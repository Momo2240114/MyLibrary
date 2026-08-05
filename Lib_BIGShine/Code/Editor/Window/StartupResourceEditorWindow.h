#pragma once

#include "EditorWindow.h"
#include "Engine/Graphics/Resource/StartupResourceConfig.h"

#include <string>
#include <vector>

class StartupResourceEditorWindow : public EditorWindow
{
public:
    explicit StartupResourceEditorWindow(GameObjectManager* manager_);
    // StartupResourceConfigをEditor上で編集/検証/保存する。
    void Draw() override;

private:
    // Reload/Save/Validateなどの上部操作。
    void DrawToolbar();
    // Title前に待つCritical Model一覧。
    void DrawCriticalModelSection();
    // 起動時に先読みだけ行うHigh Priority Model一覧。
    void DrawHighPriorityModelSection();
    // 共通のModel Path編集Listを描画する。
    void DrawModelPathList(
        const char* childId_,
        std::vector<StartupModelEntry>& entries_,
        STARTUP_RESOURCE_PRIORITY priority_);
    // 現在設定の検証結果を表示する。
    void DrawValidationResults();

    // FileDialogでModelを選択して追加する。
    void OpenModelFileDialog(STARTUP_RESOURCE_PRIORITY priority_);
    // Pathを検証して指定Priorityへ追加する。
    void AddModelPath(const std::string& filePath_, STARTUP_RESOURCE_PRIORITY priority_);
    void SetModelPath(
        STARTUP_RESOURCE_PRIORITY priority_,
        size_t index_,
        const std::string& filePath_);
    void RemoveModelPath(STARTUP_RESOURCE_PRIORITY priority_, size_t index_);
    void MoveModelPath(STARTUP_RESOURCE_PRIORITY priority_, size_t fromIndex_, size_t toIndex_);
    // Critical/High間でEntryを移動する。
    void ChangePriority(STARTUP_RESOURCE_PRIORITY priority_, size_t index_);

    // JSON設定を読み込む。
    bool LoadSettings();
    // 検証後、JSON設定として保存する。
    bool SaveSettings();
    // Path存在、拡張子、Data配下、重複を検証する。
    void ValidateSettings();
    // 未保存状態にしてStatusを更新する。
    void MarkUnsaved();

    std::vector<StartupModelEntry>& GetEntries(STARTUP_RESOURCE_PRIORITY priority_);
    const std::vector<StartupModelEntry>& GetEntries(STARTUP_RESOURCE_PRIORITY priority_) const;
    // PathがCritical/Highどちらかに既に存在するか探す。
    bool HasPathInAnySection(const std::string& filePath_, STARTUP_RESOURCE_PRIORITY* foundPriority_ = nullptr, size_t* foundIndex_ = nullptr) const;
    // Resource名が他Entryと重複しているか調べる。
    bool HasResourceNameDuplicate(const std::string& resourceName_, STARTUP_RESOURCE_PRIORITY priority_, size_t index_) const;
    // 全EntryのPath/Priority/requiredBeforeTitleを現在Sectionに合わせて整える。
    void NormalizeAllEntries();

private:
    // 編集中のStartupResource設定。
    StartupResourceConfig config;
    // 保存先JSON Path。
    std::string configFilePath = "Data/Startup/StartupResources.json";
    // Toolbar横へ出す短い状態表示。
    std::string statusMessage;
    // Validate結果の一覧。
    std::vector<std::string> validationResults;
    // trueなら保存前の変更がある。
    bool hasUnsavedChanges = false;
};
