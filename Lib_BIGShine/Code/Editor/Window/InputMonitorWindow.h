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
        std::string actionName;
        std::string state;
        int frame = 0;
    };

    struct AxisHistoryEntry
    {
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
    int frameCounter = 0;
    int maxHistoryCount = 120;
    float axisRecordThreshold = 0.15f;
    float axisRecordDelta = 0.08f;
    bool showOnlyActiveActions = false;
    bool recordAxisNeutral = false;
    std::unordered_set<std::string> monitoredActions;
    std::unordered_set<std::string> monitoredAxes;
};
