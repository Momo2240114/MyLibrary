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
    // カテゴリ/レベルごとの表示フィルタ。falseのものはログ表に出さない。
    std::array<bool, 4> levelFilters{};
    char searchText[128]{};
    // Export時の出力先ベースパス。拡張子や連番は実装側で付ける。
    char outputPath[256] = "Data/DebugLog/DebugLog";
    bool autoScroll = true;
    bool liveTailMode = true;
    bool limitBySeconds = true;
    // ログ量が多い時の表示範囲制限。描画負荷を抑えつつ直近ログを追えるようにする。
    float displaySeconds = 30.0f;
    int maxSourceMessages = 2000;
    int maxDrawRows = 500;
    int browseStartIndex = 0;
    // 前回表示数を保持し、自動スクロールや差分表示の判断に使う。
    int lastVisibleCount = 0;
    int lastSourceCount = 0;
    std::string statusText;
};
