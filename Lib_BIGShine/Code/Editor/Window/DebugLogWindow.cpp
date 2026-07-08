#include "DebugLogWindow.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
    std::string ToLower(std::string text_)
    {
        std::transform(text_.begin(), text_.end(), text_.begin(),
            [](unsigned char char_) { return static_cast<char>(std::tolower(char_)); });
        return text_;
    }
}

DebugLogWindow::DebugLogWindow(GameObjectManager* manager_)
    : EditorWindow("Debug Log", manager_)
{
    defaultWindowSize = ImVec2(760.0f, 520.0f);
    categoryFilters.fill(true);
    levelFilters.fill(true);
}

void DebugLogWindow::Draw()
{
    if (!isVisible) return;

    if (BeginEditorWindow())
    {
        DrawStylePopup();

        DrawToolbar();

        if (ImGui::CollapsingHeader("Log Settings", ImGuiTreeNodeFlags_None))
        {
            DrawLogSettings();
        }

        ImGui::Separator();
        DrawLogTable();
    }

    EndEditorWindow();
}

void DebugLogWindow::DrawToolbar()
{
    DebugLog& log_ = DebugLog::Instance();

    ImGui::Text("Count: %zu", log_.GetMessageCount());
    ImGui::SameLine();

    if (ImGui::Button("Clear"))
    {
        log_.Clear();
        statusText = "Cleared.";
    }

    ImGui::SameLine();
    ImGui::Checkbox("Auto Scroll", &autoScroll);

    if (!statusText.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", statusText.c_str());
    }
}

void DebugLogWindow::DrawFilters()
{
    if (ImGui::TreeNodeEx("Category", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t index_ = 0; index_ < static_cast<size_t>(LOG_CATEGORY::COUNT); ++index_)
        {
            LOG_CATEGORY category_ = static_cast<LOG_CATEGORY>(index_);
            ImGui::Checkbox(DebugLog::GetCategoryName(category_), &categoryFilters[index_]);
            if ((index_ + 1) % 4 != 0) ImGui::SameLine();
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Level", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (size_t index_ = 0; index_ < levelFilters.size(); ++index_)
        {
            LOG_LEVEL level_ = static_cast<LOG_LEVEL>(index_);
            ImGui::Checkbox(DebugLog::GetLevelName(level_), &levelFilters[index_]);
            if (index_ + 1 < levelFilters.size()) ImGui::SameLine();
        }
        ImGui::TreePop();
    }
}

void DebugLogWindow::DrawLogSettings()
{
    DebugLog& log_ = DebugLog::Instance();

    if (ImGui::Button("Test Info"))
    {
        BS_LOG_INFO(LOG_CATEGORY::EDITOR, "Debug log test message.");
    }

    ImGui::InputText("Search", searchText, IM_ARRAYSIZE(searchText));
    ImGui::InputText("Output Path", outputPath, IM_ARRAYSIZE(outputPath));

    if (ImGui::Button("Save Text"))
    {
        const std::string path_ = std::string(outputPath) + ".txt";
        statusText = log_.SaveText(path_) ? "Saved text: " + path_ : "Failed to save text.";
    }

    ImGui::SameLine();

    if (ImGui::Button("Save Json"))
    {
        const std::string path_ = std::string(outputPath) + ".json";
        statusText = log_.SaveJson(path_) ? "Saved json: " + path_ : "Failed to save json.";
    }

    ImGui::Separator();
    DrawFilters();
}
void DebugLogWindow::DrawLogTable()
{
    const std::vector<LogMessage> messages_ = DebugLog::Instance().GetMessages();
    const ImGuiTableFlags flags_ =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY;

    if (!ImGui::BeginTable("DebugLogTable", 6, flags_, ImVec2(0.0f, 0.0f)))
    {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 140.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthFixed, 220.0f);
    ImGui::TableSetupColumn("Line", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableHeadersRow();

    for (const LogMessage& message_ : messages_)
    {
        if (!PassesFilter(message_)) continue;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(message_.time.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(DebugLog::GetCategoryName(message_.category));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextColored(GetLevelColor(message_.level), "%s", DebugLog::GetLevelName(message_.level));
        ImGui::TableSetColumnIndex(3);
        ImGui::TextWrapped("%s", message_.text.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(message_.file.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%d", message_.line);
    }

    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndTable();
}

bool DebugLogWindow::PassesFilter(const LogMessage& message_) const
{
    const size_t categoryIndex_ = static_cast<size_t>(message_.category);
    const size_t levelIndex_ = static_cast<size_t>(message_.level);
    if (categoryIndex_ >= categoryFilters.size() || !categoryFilters[categoryIndex_]) return false;
    if (levelIndex_ >= levelFilters.size() || !levelFilters[levelIndex_]) return false;

    if (searchText[0] == '\0') return true;

    const std::string needle_ = ToLower(searchText);
    const std::string text_ = ToLower(message_.text);
    const std::string file_ = ToLower(message_.file);
    return text_.find(needle_) != std::string::npos ||
        file_.find(needle_) != std::string::npos;
}

ImVec4 DebugLogWindow::GetLevelColor(LOG_LEVEL level_) const
{
    switch (level_)
    {
    case LOG_LEVEL::SUCCESS: return ImVec4(0.35f, 0.9f, 0.45f, 1.0f);
    case LOG_LEVEL::WARNING: return ImVec4(1.0f, 0.78f, 0.25f, 1.0f);
    case LOG_LEVEL::ERROR_LOG: return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    case LOG_LEVEL::INFO:
    default:
        return ImVec4(0.75f, 0.82f, 1.0f, 1.0f);
    }
}
