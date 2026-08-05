#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

enum class STARTUP_RESOURCE_PRIORITY
{
    // TitleScene開始前に読み込み完了を待つResource。
    CRITICAL,
    // 起動時に要求するが、TitleSceneは待たずに進めるResource。
    HIGH
};

struct StartupModelEntry
{
    // ResourceManagerなどから参照する名前。
    std::string resourceName;
    // Data/配下を基準にしたModel Path。
    std::string filePath;
    // 起動時ロードの優先度。
    STARTUP_RESOURCE_PRIORITY priority = STARTUP_RESOURCE_PRIORITY::HIGH;
    // trueならTitle表示前に読み込み完了が必要。
    bool requiredBeforeTitle = false;
};

class StartupResourceConfig
{
public:
    // JSONから起動時Resource設定を読み込む。
    bool Load(const std::string& filePath_);
    // 現在の設定をJSONへ保存する。
    bool Save(const std::string& filePath_) const;
    // Critical/Highの登録を全て空にする。
    void Clear();

    std::vector<StartupModelEntry>& GetCriticalModels() { return criticalModels; }
    std::vector<StartupModelEntry>& GetHighPriorityModels() { return highPriorityModels; }
    const std::vector<StartupModelEntry>& GetCriticalModels() const { return criticalModels; }
    const std::vector<StartupModelEntry>& GetHighPriorityModels() const { return highPriorityModels; }

    // Path表記をData相対/スラッシュ表記へ揃える。
    static std::string NormalizeResourcePath(const std::string& filePath_);
    // File名からResource名の初期値を作る。
    static std::string MakeDefaultResourceName(const std::string& filePath_);
    // 起動時読み込み対象として扱えるModel拡張子か確認する。
    static bool IsSupportedModelPath(const std::string& filePath_);
    // Data/配下のPathか確認する。
    static bool IsDataRelativePath(const std::string& filePath_);
    // 実際にDisk上へ存在するPathか確認する。
    static bool ExistsOnDisk(const std::string& filePath_);
    static const char* PriorityToString(STARTUP_RESOURCE_PRIORITY priority_);
    static STARTUP_RESOURCE_PRIORITY StringToPriority(const std::string& priority_);

private:
    // 読み込み/保存時にPriority情報を揃えたEntryを作る。
    static StartupModelEntry MakeEntry(
        const std::string& resourceName_,
        const std::string& filePath_,
        STARTUP_RESOURCE_PRIORITY priority_);
    // 一時ファイルを経由してJSONを保存し、途中失敗で本体を壊しにくくする。
    static bool SaveJsonAtomically(const std::string& filePath_, const nlohmann::json& json_);

private:
    // Title前に必ず読み込むModel。
    std::vector<StartupModelEntry> criticalModels;
    // 裏で先読みするModel。
    std::vector<StartupModelEntry> highPriorityModels;
};
