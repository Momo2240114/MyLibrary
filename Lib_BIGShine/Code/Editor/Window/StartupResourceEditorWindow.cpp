#include "PrecompiledHeader.h"

#include "StartupResourceEditorWindow.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include "Engine/Platform/WindowsDialogHelper.h"
#include "Engine/Common/DebugLog.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <imgui_stdlib.h>

namespace
{
    std::string MakeComparablePath(const std::string& filePath_)
    {
        // 重複比較用にPath表記を正規化し、Windowsでは大小文字差も吸収する。
        std::string normalizedPath_ = StartupResourceConfig::NormalizeResourcePath(filePath_);
#ifdef _WIN32
        std::transform(normalizedPath_.begin(), normalizedPath_.end(), normalizedPath_.begin(),
            [](unsigned char character_) { return static_cast<char>(std::tolower(character_)); });
#endif
        return normalizedPath_;
    }
}

StartupResourceEditorWindow::StartupResourceEditorWindow(GameObjectManager* manager_)
    : EditorWindow("Startup Model Settings", manager_)
{
    // Startup設定は必要時だけ開くEditor Windowなので初期表示はOFFにする。
    defaultWindowSize = ImVec2(920.0f, 720.0f);
    isVisible = false;
    LoadSettings();
    ValidateSettings();
}

void StartupResourceEditorWindow::Draw()
{
    // 起動時Model設定の編集、保存、検証を1画面にまとめて表示する。
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    ImGui::TextUnformatted("Startup Model Settings");
    ImGui::Text("Config: %s", configFilePath.c_str());
    DrawToolbar();
    ImGui::Separator();

    if (hasUnsavedChanges)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "Unsaved changes");
    }

    DrawCriticalModelSection();
    ImGui::Separator();
    DrawHighPriorityModelSection();
    ImGui::Separator();
    DrawValidationResults();

    DrawStylePopup();
    EndEditorWindow();
}

void StartupResourceEditorWindow::DrawToolbar()
{
    if (ImGui::Button("Reload"))
    {
        // Disk上のJSONを再読み込みして、Editor上の変更状態をリセットする。
        if (LoadSettings())
        {
            statusMessage = "Reloaded.";
        }
        else
        {
            statusMessage = "Reload failed. Empty settings are available.";
        }
        ValidateSettings();
    }

    ImGui::SameLine();
    if (ImGui::Button("Save JSON"))
    {
        // 保存前にValidateし、ErrorがあればSaveSettings側で止める。
        SaveSettings();
        ValidateSettings();
    }

    ImGui::SameLine();
    if (ImGui::Button("Validate"))
    {
        ValidateSettings();
    }

    if (!statusMessage.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", statusMessage.c_str());
    }
}

void StartupResourceEditorWindow::DrawCriticalModelSection()
{
    if (!ImGui::CollapsingHeader("Critical Models", ImGuiTreeNodeFlags_DefaultOpen)) return;

    // CriticalはTitleSceneが待つため、最初の表示に必要なModelだけ入れる。
    ImGui::TextWrapped("TitleScene waits until these models finish loading.");
    if (ImGui::Button("Add Model...##Critical"))
    {
        OpenModelFileDialog(STARTUP_RESOURCE_PRIORITY::CRITICAL);
    }

    DrawModelPathList(
        "CriticalModelList",
        config.GetCriticalModels(),
        STARTUP_RESOURCE_PRIORITY::CRITICAL);
}

void StartupResourceEditorWindow::DrawHighPriorityModelSection()
{
    if (!ImGui::CollapsingHeader("High Priority Models", ImGuiTreeNodeFlags_DefaultOpen)) return;

    // High Priorityは先読みするが、Title表示の待機条件には含めない。
    ImGui::TextWrapped("These models are requested at startup, but TitleScene does not wait for them.");
    if (ImGui::Button("Add Model...##High"))
    {
        OpenModelFileDialog(STARTUP_RESOURCE_PRIORITY::HIGH);
    }

    DrawModelPathList(
        "HighPriorityModelList",
        config.GetHighPriorityModels(),
        STARTUP_RESOURCE_PRIORITY::HIGH);
}

void StartupResourceEditorWindow::DrawModelPathList(
    const char* childId_,
    std::vector<StartupModelEntry>& entries_,
    STARTUP_RESOURCE_PRIORITY priority_)
{
    const float childHeight_ = (std::min)(360.0f, (std::max)(160.0f, ImGui::GetContentRegionAvail().y * 0.45f));
    // 一覧が長くなってもWindow全体を押し潰さないよう、Section内だけScrollさせる。
    ImGui::BeginChild(childId_, ImVec2(0.0f, childHeight_), true);

    int removeIndex_ = -1;
    int moveUpIndex_ = -1;
    int moveDownIndex_ = -1;
    int changePriorityIndex_ = -1;

    for (int index_ = 0; index_ < static_cast<int>(entries_.size()); ++index_)
    {
        StartupModelEntry& entry_ = entries_[index_];
        ImGui::PushID(index_);

        const bool exists_ = StartupResourceConfig::ExistsOnDisk(entry_.filePath);
        const bool supported_ = StartupResourceConfig::IsSupportedModelPath(entry_.filePath);
        const bool dataPath_ = StartupResourceConfig::IsDataRelativePath(entry_.filePath);
        const bool nameDuplicate_ = HasResourceNameDuplicate(entry_.resourceName, priority_, static_cast<size_t>(index_));

        // 各Entryの妥当性をその場で見せ、保存前に問題を見つけやすくする。
        if (exists_ && supported_ && dataPath_ && !nameDuplicate_)
        {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "[OK]");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "[WARN]");
        }
        ImGui::SameLine();
        ImGui::Text("%d.", index_ + 1);

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::InputText("Resource", &entry_.resourceName))
        {
            MarkUnsaved();
        }

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("Path", &entry_.filePath))
        {
            // 手入力Pathも即正規化して、比較や保存形式を安定させる。
            entry_.filePath = StartupResourceConfig::NormalizeResourcePath(entry_.filePath);
            MarkUnsaved();
        }

        if (!exists_) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "File not found.");
        if (!supported_) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Unsupported model extension.");
        if (!dataPath_) ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "Path should be under Data/.");
        if (nameDuplicate_) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "Duplicated Resource name.");

        if (ImGui::SmallButton("Browse..."))
        {
            const std::string selectedPath_ = WindowsDialogHelper::OpenGltfDialog(
                L"Select Startup Model",
                entry_.filePath.empty() ? "Data/Model" : entry_.filePath,
                GraphicsDeviceManager::Instance().GetWindowHandle());
            if (!selectedPath_.empty())
            {
                SetModelPath(priority_, static_cast<size_t>(index_), selectedPath_);
            }
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Up") && index_ > 0) moveUpIndex_ = index_;
        ImGui::SameLine();
        if (ImGui::SmallButton("Down") && index_ + 1 < static_cast<int>(entries_.size())) moveDownIndex_ = index_;
        ImGui::SameLine();
        if (priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL)
        {
            if (ImGui::SmallButton("Move To High")) changePriorityIndex_ = index_;
        }
        else
        {
            if (ImGui::SmallButton("Move To Critical")) changePriorityIndex_ = index_;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete")) removeIndex_ = index_;

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();

    if (moveUpIndex_ >= 0)
    {
        // List操作はLoop中にVectorを変更しないよう、Indexだけ記録して後で処理する。
        MoveModelPath(priority_, static_cast<size_t>(moveUpIndex_), static_cast<size_t>(moveUpIndex_ - 1));
    }
    if (moveDownIndex_ >= 0)
    {
        MoveModelPath(priority_, static_cast<size_t>(moveDownIndex_), static_cast<size_t>(moveDownIndex_ + 1));
    }
    if (changePriorityIndex_ >= 0)
    {
        ChangePriority(priority_, static_cast<size_t>(changePriorityIndex_));
    }
    if (removeIndex_ >= 0)
    {
        RemoveModelPath(priority_, static_cast<size_t>(removeIndex_));
    }
}

void StartupResourceEditorWindow::DrawValidationResults()
{
    // ValidateSettingsで作った結果を、Error/Warning/OKで色分けして表示する。
    ImGui::TextUnformatted("Validation Results");
    ImGui::BeginChild("StartupValidationResults", ImVec2(0.0f, 150.0f), true);
    for (const std::string& result_ : validationResults)
    {
        if (result_.starts_with("[ERROR]"))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "%s", result_.c_str());
        }
        else if (result_.starts_with("[WARNING]"))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "%s", result_.c_str());
        }
        else
        {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "%s", result_.c_str());
        }
    }
    ImGui::EndChild();
}

void StartupResourceEditorWindow::OpenModelFileDialog(STARTUP_RESOURCE_PRIORITY priority_)
{
    const std::string selectedPath_ = WindowsDialogHelper::OpenGltfDialog(
        L"Select Startup Model",
        "Data/Model",
        GraphicsDeviceManager::Instance().GetWindowHandle());
    if (selectedPath_.empty()) return;

    AddModelPath(selectedPath_, priority_);
}

void StartupResourceEditorWindow::AddModelPath(
    const std::string& filePath_,
    STARTUP_RESOURCE_PRIORITY priority_)
{
    const std::string normalizedPath_ = StartupResourceConfig::NormalizeResourcePath(filePath_);
    // 登録前にPath形式、拡張子、Data配下かどうかを確認する。
    if (normalizedPath_.empty())
    {
        statusMessage = "Invalid path.";
        BS_LOG_WARNING(LOG_CATEGORY::EDITOR, "[StartupResourceEditor] Invalid model path.");
        return;
    }
    if (!StartupResourceConfig::IsSupportedModelPath(normalizedPath_))
    {
        statusMessage = "Unsupported model extension.";
        BS_LOG_WARNING(LOG_CATEGORY::EDITOR, "[StartupResourceEditor] Unsupported model path: " + normalizedPath_);
        return;
    }
    if (!StartupResourceConfig::IsDataRelativePath(normalizedPath_))
    {
        statusMessage = "Model path must be under Data/.";
        BS_LOG_WARNING(LOG_CATEGORY::EDITOR, "[StartupResourceEditor] Non Data path: " + normalizedPath_);
        return;
    }

    STARTUP_RESOURCE_PRIORITY foundPriority_ = STARTUP_RESOURCE_PRIORITY::HIGH;
    size_t foundIndex_ = 0;
    if (HasPathInAnySection(normalizedPath_, &foundPriority_, &foundIndex_))
    {
        // 既に登録済みの場合、Criticalへの昇格は許可し、CriticalからHighへの降格は事故防止で止める。
        if (foundPriority_ == priority_)
        {
            statusMessage = "Model is already registered.";
            return;
        }

        if (priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL)
        {
            ChangePriority(foundPriority_, foundIndex_);
            statusMessage = "Moved model to Critical.";
        }
        else
        {
            statusMessage = "Model is already Critical. It was not downgraded.";
            BS_LOG_WARNING(LOG_CATEGORY::EDITOR, "[StartupResourceEditor] Critical model downgrade blocked: " + normalizedPath_);
        }
        return;
    }

    // Resource名はPathから自動生成し、必要ならEditor上で後から変更できる。
    StartupModelEntry entry_;
    entry_.filePath = normalizedPath_;
    entry_.resourceName = StartupResourceConfig::MakeDefaultResourceName(normalizedPath_);
    entry_.priority = priority_;
    entry_.requiredBeforeTitle = priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL;

    GetEntries(priority_).push_back(entry_);
    MarkUnsaved();
    ValidateSettings();
}

void StartupResourceEditorWindow::SetModelPath(
    STARTUP_RESOURCE_PRIORITY priority_,
    size_t index_,
    const std::string& filePath_)
{
    std::vector<StartupModelEntry>& entries_ = GetEntries(priority_);
    if (index_ >= entries_.size()) return;

    const std::string normalizedPath_ = StartupResourceConfig::NormalizeResourcePath(filePath_);
    STARTUP_RESOURCE_PRIORITY foundPriority_ = STARTUP_RESOURCE_PRIORITY::HIGH;
    size_t foundIndex_ = 0;
    if (HasPathInAnySection(normalizedPath_, &foundPriority_, &foundIndex_) &&
        (foundPriority_ != priority_ || foundIndex_ != index_))
    {
        // 同じPathを複数Entryに入れると起動時Load要求が重複するため禁止する。
        statusMessage = "Duplicated model path.";
        return;
    }

    entries_[index_].filePath = normalizedPath_;
    if (entries_[index_].resourceName.empty())
    {
        entries_[index_].resourceName = StartupResourceConfig::MakeDefaultResourceName(normalizedPath_);
    }
    MarkUnsaved();
    ValidateSettings();
}

void StartupResourceEditorWindow::RemoveModelPath(STARTUP_RESOURCE_PRIORITY priority_, size_t index_)
{
    std::vector<StartupModelEntry>& entries_ = GetEntries(priority_);
    if (index_ >= entries_.size()) return;
    entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(index_));
    MarkUnsaved();
    ValidateSettings();
}

void StartupResourceEditorWindow::MoveModelPath(
    STARTUP_RESOURCE_PRIORITY priority_,
    size_t fromIndex_,
    size_t toIndex_)
{
    std::vector<StartupModelEntry>& entries_ = GetEntries(priority_);
    if (fromIndex_ >= entries_.size() || toIndex_ >= entries_.size()) return;
    std::swap(entries_[fromIndex_], entries_[toIndex_]);
    MarkUnsaved();
}

void StartupResourceEditorWindow::ChangePriority(STARTUP_RESOURCE_PRIORITY priority_, size_t index_)
{
    std::vector<StartupModelEntry>& sourceEntries_ = GetEntries(priority_);
    if (index_ >= sourceEntries_.size()) return;

    const STARTUP_RESOURCE_PRIORITY targetPriority_ =
        priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL
        ? STARTUP_RESOURCE_PRIORITY::HIGH
        : STARTUP_RESOURCE_PRIORITY::CRITICAL;

    // Sectionを移すと同時に、Entry内のPriorityとTitle待機Flagも移動先に合わせる。
    StartupModelEntry entry_ = sourceEntries_[index_];
    entry_.priority = targetPriority_;
    entry_.requiredBeforeTitle = targetPriority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL;

    sourceEntries_.erase(sourceEntries_.begin() + static_cast<std::ptrdiff_t>(index_));
    GetEntries(targetPriority_).push_back(entry_);
    MarkUnsaved();
    ValidateSettings();
}

bool StartupResourceEditorWindow::LoadSettings()
{
    const bool loaded_ = config.Load(configFilePath);
    hasUnsavedChanges = false;
    statusMessage = loaded_ ? "Loaded." : "Config not found or failed.";
    return loaded_;
}

bool StartupResourceEditorWindow::SaveSettings()
{
    NormalizeAllEntries();
    ValidateSettings();

    // Errorが残っている状態では、壊れた起動設定を保存しない。
    for (const std::string& result_ : validationResults)
    {
        if (result_.starts_with("[ERROR]"))
        {
            statusMessage = "Save blocked by validation errors.";
            return false;
        }
    }

    if (config.Save(configFilePath))
    {
        hasUnsavedChanges = false;
        statusMessage = "Saved.";
        return true;
    }

    statusMessage = "Save failed.";
    return false;
}

void StartupResourceEditorWindow::ValidateSettings()
{
    // 現在の編集内容を保存可能か検査し、画面下のValidation Resultsへ出す。
    validationResults.clear();
    NormalizeAllEntries();

    bool hasError_ = false;
    std::vector<std::string> allPaths_;
    std::vector<std::string> allNames_;

    auto validateEntries_ =
        [&](const std::vector<StartupModelEntry>& entries_, const char* sectionName_)
        {
            // SectionごとにResource名、Path、拡張子、存在確認を行う。
            for (const StartupModelEntry& entry_ : entries_)
            {
                if (entry_.resourceName.empty())
                {
                    validationResults.push_back("[ERROR] Empty Resource name in " + std::string(sectionName_) + ".");
                    hasError_ = true;
                }
                if (entry_.filePath.empty())
                {
                    validationResults.push_back("[ERROR] Empty model path in " + std::string(sectionName_) + ".");
                    hasError_ = true;
                    continue;
                }
                if (!StartupResourceConfig::IsSupportedModelPath(entry_.filePath))
                {
                    validationResults.push_back("[ERROR] Unsupported model extension: " + entry_.filePath);
                    hasError_ = true;
                }
                if (!StartupResourceConfig::IsDataRelativePath(entry_.filePath))
                {
                    validationResults.push_back("[ERROR] Path is not under Data/: " + entry_.filePath);
                    hasError_ = true;
                }
                if (!StartupResourceConfig::ExistsOnDisk(entry_.filePath))
                {
                    validationResults.push_back("[ERROR] Model file not found: " + entry_.filePath);
                    hasError_ = true;
                }
                allPaths_.push_back(MakeComparablePath(entry_.filePath));
                allNames_.push_back(entry_.resourceName);
            }
        };

    validateEntries_(config.GetCriticalModels(), "Critical");
    validateEntries_(config.GetHighPriorityModels(), "High");

    auto hasDuplicate_ = [](std::vector<std::string> values_)
        {
            // Sort後に隣同士を見れば重複の有無だけを軽く判定できる。
            std::sort(values_.begin(), values_.end());
            return std::adjacent_find(values_.begin(), values_.end()) != values_.end();
        };

    if (hasDuplicate_(allPaths_))
    {
        validationResults.push_back("[ERROR] Duplicated model path.");
        hasError_ = true;
    }
    if (hasDuplicate_(allNames_))
    {
        validationResults.push_back("[ERROR] Duplicated Resource name.");
        hasError_ = true;
    }
    if (config.GetCriticalModels().empty())
    {
        validationResults.push_back("[WARNING] Critical model list is empty.");
    }
    if (!hasError_)
    {
        validationResults.push_back("[OK] No blocking validation errors.");
    }
}

void StartupResourceEditorWindow::MarkUnsaved()
{
    hasUnsavedChanges = true;
    statusMessage = "Modified.";
}

std::vector<StartupModelEntry>& StartupResourceEditorWindow::GetEntries(STARTUP_RESOURCE_PRIORITY priority_)
{
    return priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL
        ? config.GetCriticalModels()
        : config.GetHighPriorityModels();
}

const std::vector<StartupModelEntry>& StartupResourceEditorWindow::GetEntries(STARTUP_RESOURCE_PRIORITY priority_) const
{
    return priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL
        ? config.GetCriticalModels()
        : config.GetHighPriorityModels();
}

bool StartupResourceEditorWindow::HasPathInAnySection(
    const std::string& filePath_,
    STARTUP_RESOURCE_PRIORITY* foundPriority_,
    size_t* foundIndex_) const
{
    const std::string normalizedPath_ = MakeComparablePath(filePath_);
    auto findPath_ =
        [&](const std::vector<StartupModelEntry>& entries_, STARTUP_RESOURCE_PRIORITY priority_) -> bool
        {
            // 比較用PathでCritical/High両方を検索し、見つかった位置も必要なら返す。
            for (size_t index_ = 0; index_ < entries_.size(); ++index_)
            {
                if (MakeComparablePath(entries_[index_].filePath) == normalizedPath_)
                {
                    if (foundPriority_) *foundPriority_ = priority_;
                    if (foundIndex_) *foundIndex_ = index_;
                    return true;
                }
            }
            return false;
        };

    return findPath_(config.GetCriticalModels(), STARTUP_RESOURCE_PRIORITY::CRITICAL) ||
        findPath_(config.GetHighPriorityModels(), STARTUP_RESOURCE_PRIORITY::HIGH);
}

bool StartupResourceEditorWindow::HasResourceNameDuplicate(
    const std::string& resourceName_,
    STARTUP_RESOURCE_PRIORITY priority_,
    size_t index_) const
{
    if (resourceName_.empty()) return false;

    auto findName_ =
        [&](const std::vector<StartupModelEntry>& entries_, STARTUP_RESOURCE_PRIORITY currentPriority_) -> bool
        {
            for (size_t currentIndex_ = 0; currentIndex_ < entries_.size(); ++currentIndex_)
            {
                if (currentPriority_ == priority_ && currentIndex_ == index_) continue;
                if (entries_[currentIndex_].resourceName == resourceName_) return true;
            }
            return false;
        };

    return findName_(config.GetCriticalModels(), STARTUP_RESOURCE_PRIORITY::CRITICAL) ||
        findName_(config.GetHighPriorityModels(), STARTUP_RESOURCE_PRIORITY::HIGH);
}

void StartupResourceEditorWindow::NormalizeAllEntries()
{
    auto normalizeEntries_ =
        [](std::vector<StartupModelEntry>& entries_, STARTUP_RESOURCE_PRIORITY priority_)
        {
            // Entryが所属Sectionと矛盾しないよう、保存/検証前に必ず揃える。
            for (StartupModelEntry& entry_ : entries_)
            {
                entry_.filePath = StartupResourceConfig::NormalizeResourcePath(entry_.filePath);
                entry_.priority = priority_;
                entry_.requiredBeforeTitle = priority_ == STARTUP_RESOURCE_PRIORITY::CRITICAL;
            }
        };

    normalizeEntries_(config.GetCriticalModels(), STARTUP_RESOURCE_PRIORITY::CRITICAL);
    normalizeEntries_(config.GetHighPriorityModels(), STARTUP_RESOURCE_PRIORITY::HIGH);
}
