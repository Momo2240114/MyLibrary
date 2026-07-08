#pragma once

#include "EditorWindow.h"

#include "Engine/Common/DebugLog.h"

#include <array>

class DebugLogWindow : public EditorWindow
{
public:
    explicit DebugLogWindow(GameObjectManager* manager_);

    void Draw() override;

private:
    void DrawToolbar();
    void DrawFilters();
    void DrawLogSettings();
    void DrawLogTable();
    bool PassesFilter(const LogMessage& message_) const;
    ImVec4 GetLevelColor(LOG_LEVEL level_) const;

    std::array<bool, static_cast<size_t>(LOG_CATEGORY::COUNT)> categoryFilters{};
    std::array<bool, 4> levelFilters{};
    char searchText[128]{};
    char outputPath[256] = "Data/DebugLog/DebugLog";
    bool autoScroll = true;
    std::string statusText;
};
