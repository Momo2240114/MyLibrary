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
    void DrawDisplayControls(size_t messageCount_);
    bool PassesFilter(const LogMessage& message_) const;
    ImVec4 GetLevelColor(LOG_LEVEL level_) const;

    std::array<bool, static_cast<size_t>(LOG_CATEGORY::COUNT)> categoryFilters{};
    std::array<bool, 4> levelFilters{};
    char searchText[128]{};
    char outputPath[256] = "Data/DebugLog/DebugLog";
    bool autoScroll = true;
    bool liveTailMode = true;
    bool limitBySeconds = true;
    float displaySeconds = 30.0f;
    int maxSourceMessages = 2000;
    int maxDrawRows = 500;
    int browseStartIndex = 0;
    int lastVisibleCount = 0;
    int lastSourceCount = 0;
    std::string statusText;
};
