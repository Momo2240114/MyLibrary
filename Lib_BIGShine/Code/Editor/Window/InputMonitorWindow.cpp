#include "PrecompiledHeader.h"

#include "InputMonitorWindow.h"

#include <algorithm>
#include <cmath>

InputMonitorWindow::InputMonitorWindow(GameObjectManager* manager_)
    : EditorWindow("Input Monitor", manager_)
{
    defaultWindowSize = ImVec2(760.0f, 680.0f);
    isVisible = false;
}

void InputMonitorWindow::Draw()
{
    if (!BeginEditorWindow(nullptr, ImGuiWindowFlags_NoCollapse))
    {
        EndEditorWindow();
        return;
    }

    UpdateInputState();

    ImGui::Checkbox("Show Only Active Actions", &showOnlyActiveActions);
    ImGui::SameLine();
    ImGui::Checkbox("Record Axis Neutral", &recordAxisNeutral);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::DragInt("History Count", &maxHistoryCount, 1.0f, 20, 500);
    maxHistoryCount = std::clamp(maxHistoryCount, 20, 500);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::DragFloat("Axis Threshold", &axisRecordThreshold, 0.01f, 0.0f, 1.0f);
    axisRecordThreshold = std::clamp(axisRecordThreshold, 0.0f, 1.0f);
    ImGui::SetNextItemWidth(140.0f);
    ImGui::DragFloat("Axis Delta", &axisRecordDelta, 0.01f, 0.0f, 1.0f);
    axisRecordDelta = std::clamp(axisRecordDelta, 0.0f, 1.0f);

    ImGui::Separator();

    const float leftWidth_ = 300.0f;
    ImGui::BeginChild("InputMonitorLeft", ImVec2(leftWidth_, 0.0f), true);
    DrawMonitorSelector();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("InputMonitorRight", ImVec2(0.0f, 0.0f), true);
    DrawActionMonitor();
    ImGui::Separator();
    DrawAxisMonitor();
    ImGui::Separator();
    DrawHistory();
    ImGui::EndChild();

    DrawStylePopup();
    EndEditorWindow();
}

void InputMonitorWindow::UpdateInputState()
{
    ++frameCounter;

    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    for (const std::string& actionName_ : BuildActionNames())
    {
        if (!IsActionMonitored(actionName_)) continue;
        if (keyConfig_.ActionPressedIn(actionName_))
        {
            PushActionHistory(actionName_, "Pressed");
        }
        else if (keyConfig_.ActionPressedOut(actionName_))
        {
            PushActionHistory(actionName_, "Released");
        }
    }

    for (const std::string& axisName_ : BuildAxisNames())
    {
        if (!IsAxisMonitored(axisName_)) continue;
        const float value_ = keyConfig_.GetAxis(axisName_);
        const bool shouldRecord_ =
            std::abs(value_) >= axisRecordThreshold ||
            (recordAxisNeutral && std::abs(value_) <= 0.001f);
        if (!shouldRecord_) continue;

        bool shouldPush_ = true;
        for (auto iterator_ = axisHistory.rbegin();
            iterator_ != axisHistory.rend();
            ++iterator_)
        {
            if (iterator_->axisName != axisName_) continue;
            shouldPush_ =
                std::abs(iterator_->value - value_) >= axisRecordDelta;
            break;
        }
        if (shouldPush_) PushAxisHistory(axisName_, value_);
    }
}

bool InputMonitorWindow::IsActionMonitored(const std::string& actionName_) const
{
    return monitoredActions.find(actionName_) != monitoredActions.end();
}

bool InputMonitorWindow::IsAxisMonitored(const std::string& axisName_) const
{
    return monitoredAxes.find(axisName_) != monitoredAxes.end();
}

void InputMonitorWindow::PushActionHistory(
    const std::string& actionName_,
    const std::string& state_)
{
    actionHistory.push_front({ actionName_, state_, frameCounter });
    while (static_cast<int>(actionHistory.size()) > maxHistoryCount)
    {
        actionHistory.pop_back();
    }
}

void InputMonitorWindow::PushAxisHistory(
    const std::string& axisName_,
    float value_)
{
    axisHistory.push_front({ axisName_, value_, frameCounter });
    while (static_cast<int>(axisHistory.size()) > maxHistoryCount)
    {
        axisHistory.pop_back();
    }
}

void InputMonitorWindow::DrawActionMonitor()
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    ImGui::TextUnformatted("Actions");
    ImGui::Separator();

    for (const std::string& actionName_ : BuildActionNames())
    {
        if (!IsActionMonitored(actionName_)) continue;
        const bool held_ = keyConfig_.ActionPressed(actionName_);
        const bool pressed_ = keyConfig_.ActionPressedIn(actionName_);
        const bool released_ = keyConfig_.ActionPressedOut(actionName_);
        if (showOnlyActiveActions && !held_ && !pressed_ && !released_) continue;

        const ImVec4 color_ = pressed_
            ? ImVec4(0.2f, 0.85f, 1.0f, 1.0f)
            : held_
                ? ImVec4(0.2f, 1.0f, 0.35f, 1.0f)
                : released_
                    ? ImVec4(1.0f, 0.65f, 0.2f, 1.0f)
                    : ImVec4(0.65f, 0.65f, 0.65f, 1.0f);

        ImGui::TextColored(
            color_,
            "%s  %s%s%s",
            actionName_.c_str(),
            held_ ? "Hold " : "",
            pressed_ ? "In " : "",
            released_ ? "Out" : "");
    }
}

void InputMonitorWindow::DrawAxisMonitor()
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    ImGui::TextUnformatted("Axes");
    ImGui::Separator();

    const float padSize_ = 150.0f;
    bool drewStick_ = false;
    if (HasSelectedStickAxis(AxisCode::LeftStickX, AxisCode::LeftStickY))
    {
        DrawStickPad(
            "Selected Left Stick",
            BuildSelectedStickValue(AxisCode::LeftStickX, AxisCode::LeftStickY),
            padSize_);
        drewStick_ = true;
    }
    if (HasSelectedStickAxis(AxisCode::RightStickX, AxisCode::RightStickY))
    {
        if (drewStick_) ImGui::SameLine();
        DrawStickPad(
            "Selected Right Stick",
            BuildSelectedStickValue(AxisCode::RightStickX, AxisCode::RightStickY),
            padSize_);
        drewStick_ = true;
    }
    if (!drewStick_)
    {
        ImGui::TextDisabled("Select a stick axis to show the stick pad.");
    }

    ImGui::Separator();
    for (const std::string& axisName_ : BuildAxisNames())
    {
        if (!IsAxisMonitored(axisName_)) continue;
        const float value_ = keyConfig_.GetAxis(axisName_);
        ImGui::Text("%s", axisName_.c_str());
        ImGui::SameLine(180.0f);
        ImGui::ProgressBar(
            (value_ + 1.0f) * 0.5f,
            ImVec2(180.0f, 0.0f),
            std::to_string(value_).c_str());
    }
}

void InputMonitorWindow::DrawStickPad(
    const char* label_,
    const DirectX::XMFLOAT2& value_,
    float size_)
{
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label_);

    const ImVec2 cursor_ = ImGui::GetCursorScreenPos();
    const ImVec2 sizeVector_(size_, size_);
    ImGui::InvisibleButton(label_, sizeVector_);

    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    const ImVec2 center_(
        cursor_.x + size_ * 0.5f,
        cursor_.y + size_ * 0.5f);
    const float radius_ = size_ * 0.5f - 8.0f;
    const ImVec2 stickPosition_(
        center_.x + std::clamp(value_.x, -1.0f, 1.0f) * radius_,
        center_.y - std::clamp(value_.y, -1.0f, 1.0f) * radius_);

    drawList_->AddRectFilled(
        cursor_,
        ImVec2(cursor_.x + size_, cursor_.y + size_),
        IM_COL32(42, 48, 45, 230));
    drawList_->AddRect(
        cursor_,
        ImVec2(cursor_.x + size_, cursor_.y + size_),
        IM_COL32(120, 135, 128, 255));
    drawList_->AddLine(
        ImVec2(center_.x, cursor_.y + 6.0f),
        ImVec2(center_.x, cursor_.y + size_ - 6.0f),
        IM_COL32(145, 155, 150, 160));
    drawList_->AddLine(
        ImVec2(cursor_.x + 6.0f, center_.y),
        ImVec2(cursor_.x + size_ - 6.0f, center_.y),
        IM_COL32(145, 155, 150, 160));
    drawList_->AddCircle(
        center_,
        radius_,
        IM_COL32(110, 125, 118, 200),
        48,
        1.5f);
    drawList_->AddCircleFilled(
        stickPosition_,
        6.0f,
        IM_COL32(30, 160, 255, 255),
        24);

    ImGui::Text("X %.3f  Y %.3f", value_.x, value_.y);
    ImGui::EndGroup();
}

DirectX::XMFLOAT2 InputMonitorWindow::BuildSelectedStickValue(
    AxisCode xCode_,
    AxisCode yCode_) const
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    DirectX::XMFLOAT2 value_{ 0.0f, 0.0f };

    for (const auto& pair_ : keyConfig_.GetAxisMap())
    {
        if (!IsAxisMonitored(pair_.first)) continue;

        bool usesX_ = false;
        bool usesY_ = false;
        for (const AxisUnit& unit_ : pair_.second)
        {
            if (unit_.sourceType != AxisSourceType::GamepadStick) continue;
            if (unit_.code == static_cast<int>(xCode_)) usesX_ = true;
            if (unit_.code == static_cast<int>(yCode_)) usesY_ = true;
        }

        if (usesX_) value_.x += keyConfig_.GetAxis(pair_.first);
        if (usesY_) value_.y += keyConfig_.GetAxis(pair_.first);
    }

    value_.x = std::clamp(value_.x, -1.0f, 1.0f);
    value_.y = std::clamp(value_.y, -1.0f, 1.0f);
    return value_;
}

bool InputMonitorWindow::HasSelectedStickAxis(
    AxisCode xCode_,
    AxisCode yCode_) const
{
    const KeyConfig& keyConfig_ = KeyConfig::Instance();
    for (const auto& pair_ : keyConfig_.GetAxisMap())
    {
        if (!IsAxisMonitored(pair_.first)) continue;

        for (const AxisUnit& unit_ : pair_.second)
        {
            if (unit_.sourceType != AxisSourceType::GamepadStick) continue;
            if (unit_.code == static_cast<int>(xCode_) ||
                unit_.code == static_cast<int>(yCode_))
            {
                return true;
            }
        }
    }

    return false;
}

void InputMonitorWindow::DrawHistory()
{
    ImGui::TextUnformatted("History");
    if (ImGui::Button("Clear History"))
    {
        actionHistory.clear();
        axisHistory.clear();
    }

    ImGui::BeginChild("InputMonitorHistory", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Action");
    for (const ActionHistoryEntry& entry_ : actionHistory)
    {
        ImGui::Text(
            "#%d  %s  %s",
            entry_.frame,
            entry_.actionName.c_str(),
            entry_.state.c_str());
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Axis");
    for (const AxisHistoryEntry& entry_ : axisHistory)
    {
        ImGui::Text(
            "#%d  %s  %.3f",
            entry_.frame,
            entry_.axisName.c_str(),
            entry_.value);
    }
    ImGui::EndChild();
}

void InputMonitorWindow::DrawMonitorSelector()
{
    ImGui::TextUnformatted("Monitor Targets");
    ImGui::Separator();

    if (ImGui::Button("Clear Selection", ImVec2(-1.0f, 0.0f)))
    {
        monitoredActions.clear();
        monitoredAxes.clear();
        actionHistory.clear();
        axisHistory.clear();
    }

    if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const std::string& actionName_ : BuildActionNames())
        {
            bool selected_ = IsActionMonitored(actionName_);
            if (ImGui::Checkbox(actionName_.c_str(), &selected_))
            {
                if (selected_)
                {
                    monitoredActions.insert(actionName_);
                }
                else
                {
                    monitoredActions.erase(actionName_);
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Axes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const std::string& axisName_ : BuildAxisNames())
        {
            bool selected_ = IsAxisMonitored(axisName_);
            if (ImGui::Checkbox(axisName_.c_str(), &selected_))
            {
                if (selected_)
                {
                    monitoredAxes.insert(axisName_);
                }
                else
                {
                    monitoredAxes.erase(axisName_);
                }
            }
        }
    }
}

std::vector<std::string> InputMonitorWindow::BuildActionNames() const
{
    std::vector<std::string> names_;
    for (const auto& pair_ : KeyConfig::Instance().GetActionMap())
    {
        names_.push_back(pair_.first);
    }
    std::sort(names_.begin(), names_.end());
    return names_;
}

std::vector<std::string> InputMonitorWindow::BuildAxisNames() const
{
    std::vector<std::string> names_;
    for (const auto& pair_ : KeyConfig::Instance().GetAxisMap())
    {
        names_.push_back(pair_.first);
    }
    std::sort(names_.begin(), names_.end());
    return names_;
}
