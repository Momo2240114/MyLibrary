#pragma once

#include "EditorWindow.h"
#include "Engine/GamePlay/Input/KeyConfig.h"

#include <deque>
#include <string>
#include <unordered_set>
#include <vector>

class InputMonitorWindow : public EditorWindow
{
public:
    explicit InputMonitorWindow(GameObjectManager* manager_);
    void Draw() override;

private:
    struct ActionHistoryEntry
    {
        // Actionの押下/離しなど、入力変化を履歴へ残すための1行。
        std::string actionName;
        std::string state;
        int frame = 0;
    };

    struct AxisHistoryEntry
    {
        // Axis値がしきい値以上変化した時に記録する履歴行。
        std::string axisName;
        float value = 0.0f;
        int frame = 0;
    };

    void UpdateInputState();
    bool IsActionMonitored(const std::string& actionName_) const;
    bool IsAxisMonitored(const std::string& axisName_) const;
    void PushActionHistory(
        const std::string& actionName_,
        const std::string& state_);
    void PushAxisHistory(
        const std::string& axisName_,
        float value_);
    void DrawActionMonitor();
    void DrawAxisMonitor();
    void DrawStickPad(
        const char* label_,
        const DirectX::XMFLOAT2& value_,
        float size_);
    DirectX::XMFLOAT2 BuildSelectedStickValue(
        AxisCode xCode_,
        AxisCode yCode_) const;
    bool HasSelectedStickAxis(
        AxisCode xCode_,
        AxisCode yCode_) const;
    void DrawHistory();
    void DrawMonitorSelector();
    std::vector<std::string> BuildActionNames() const;
    std::vector<std::string> BuildAxisNames() const;

private:
    std::deque<ActionHistoryEntry> actionHistory;
    std::deque<AxisHistoryEntry> axisHistory;
    // 表示フレーム番号と履歴上限。古い入力ログを溜め込みすぎないようにする。
    int frameCounter = 0;
    int maxHistoryCount = 120;
    float axisRecordThreshold = 0.15f;
    float axisRecordDelta = 0.08f;
    // 監視対象を絞るための表示/記録オプション。
    bool showOnlyActiveActions = false;
    bool recordAxisNeutral = false;
    std::unordered_set<std::string> monitoredActions;
    // 空なら全入力を監視し、要素がある場合は選択されたAction/Axisだけを表示する。
    std::unordered_set<std::string> monitoredAxes;
};
