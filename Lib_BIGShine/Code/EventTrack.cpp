#include "EventTrack.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"



EventTrack::EventTrack(const std::string& name, EventType defaultType):
	trackName(name), defaultEventType(defaultType)
{
}


// ----------------------------------------------------------
// イベント操作
// ----------------------------------------------------------

void EventTrack::AddEvent(EventType type, const std::string& label, float start, float end, unsigned int color)
{
    events.push_back({ type, start, end, color, label });
}

void EventTrack::DeleteEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
        events.erase(events.begin() + index);
}

void EventTrack::DuplicateEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
        events.push_back(events[index]);
}

void EventTrack::BeginEdit(int index)
{
    editingEntry = index;
}

void EventTrack::EndEdit()
{
    editingEntry = -1;
    editingPart = 0;
}

void EventTrack::DoubleClick(int index)
{
    // TODO: イベント詳細設定ウィンドウを開くなど
}

void EventTrack::Copy()
{
    if (editingEntry >= 0 && editingEntry < (int)events.size())
        clipboard = events[editingEntry];
}

void EventTrack::Paste()
{
    if (editingEntry >= 0 && editingEntry < (int)events.size())
        events[editingEntry] = clipboard;
}


// ----------------------------------------------------------
// ImGui Timeline 描画 (汎用イベント)
// ----------------------------------------------------------
bool EventTrack::DrawSequencer(
    float* currentTime,
    bool* expanded,
    int* selectedEntry,
    float* firstVisibleTime,
    float lengthSeconds
)
{
    const float pixelsPerSecond = 100.0f;
    const float rowHeight = 24.0f;
    const float minHeight = 160.0f;
    const float labelWidth = 120.0f;

    // 子ウィンドウ高さはイベント数に応じて動的
    float itemAreaHeight = std::max(minHeight, (float)events.size() * rowHeight);

    ImGui::BeginChild((trackName + "SequencerRegion").c_str(),
        ImVec2(0, itemAreaHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

    float contentWidth = labelWidth + (lengthSeconds * pixelsPerSecond);
    ImGui::Dummy(ImVec2(contentWidth, 0));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // 背景
    draw_list->AddRectFilled(origin,
        origin + ImVec2(contentWidth, itemAreaHeight),
        IM_COL32(30, 30, 30, 255));

    // 左ラベル帯
    draw_list->AddRectFilled(origin,
        { origin.x + labelWidth, origin.y + itemAreaHeight },
        IM_COL32(45, 45, 45, 255));

    // 秒メモリ
    for (float t = 0; t <= lengthSeconds; t += 1.0f)
    {
        float x = origin.x + labelWidth + t * pixelsPerSecond;
        draw_list->AddLine({ x, origin.y }, { x, origin.y + itemAreaHeight },
            (static_cast<int>(t) % 5 == 0) ? IM_COL32(90, 90, 90, 255) : IM_COL32(60, 60, 60, 255));

        if (static_cast<int>(t) % 5 == 0)
        {
            char buf[32]; sprintf_s(buf, "%.0f s", t);
            draw_list->AddText({ x + 2, origin.y + 2 }, IM_COL32_WHITE, buf);
        }
    }

    if (*expanded)
    {
        for (int i = 0; i < (int)events.size(); i++)
        {
            auto& ev = events[i];

            float yTop = origin.y + rowHeight * i;
            float yBottom = yTop + rowHeight;

            // ラベル帯
            ImVec2 labelMin = { origin.x, yTop };
            ImVec2 labelMax = { origin.x + labelWidth, yBottom };
            ImU32 bg = (i == *selectedEntry) ? IM_COL32(70, 70, 70, 255) : IM_COL32(50, 50, 50, 255);
            draw_list->AddRectFilled(labelMin, labelMax, bg);
            draw_list->AddText({ labelMin.x + 4, labelMin.y + 3 }, IM_COL32_WHITE, ev.label.c_str());

            // ラベルクリック判定
            ImGui::SetCursorScreenPos(labelMin);
            ImGui::InvisibleButton(("ev_label_" + std::to_string(i)).c_str(), { labelWidth, rowHeight });
            if (ImGui::IsItemClicked())
                *selectedEntry = i;

            // バー描画
            float x0 = origin.x + labelWidth + ev.startTime * pixelsPerSecond;
            float x1 = origin.x + labelWidth + ev.endTime * pixelsPerSecond;
            ImVec2 pMin = { x0, yTop };
            ImVec2 pMax = { x1, yBottom };
            draw_list->AddRectFilled(pMin, pMax, ev.color);
            draw_list->AddRect(pMin, pMax, IM_COL32(0, 0, 0, 255));

            // ドラッグ用 InvisibleButton
            ImGui::SetCursorScreenPos({ x0 - 5, yTop });
            ImGui::InvisibleButton(("ev_start_" + std::to_string(i)).c_str(), { 10, rowHeight });
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float mx = ImGui::GetIO().MousePos.x - origin.x - labelWidth;
                ev.startTime = std::max(0.0f, std::min(mx / pixelsPerSecond, ev.endTime));
            }

            ImGui::SetCursorScreenPos({ x1 - 5, yTop });
            ImGui::InvisibleButton(("ev_end_" + std::to_string(i)).c_str(), { 10, rowHeight });
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float mx = ImGui::GetIO().MousePos.x - origin.x - labelWidth;
                ev.endTime = std::max(ev.startTime, std::min(mx / pixelsPerSecond, lengthSeconds));
            }

            // バー選択クリック
            ImGui::SetCursorScreenPos({ x0 + 5, yTop });
            ImGui::InvisibleButton(("ev_full_" + std::to_string(i)).c_str(), { (x1 - x0) - 10, rowHeight });
            if (ImGui::IsItemClicked())
                *selectedEntry = i;
        }
    }

    // 現在時間ライン
    float cx = origin.x + labelWidth + (*currentTime) * pixelsPerSecond;
    draw_list->AddLine({ cx, origin.y }, { cx, origin.y + itemAreaHeight }, IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::EndChild();
    return true;
}