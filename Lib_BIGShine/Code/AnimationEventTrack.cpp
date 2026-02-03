#include "AnimationEventTrack.h"

#include <algorithm>
#include <cmath>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

AnimationEventTrack::AnimationEventTrack()
    : editingEntry(-1), editingPart(0)
{
}

void AnimationEventTrack::AddEvent(const std::string& label,float start, float end, unsigned int color)
{
    events.push_back({ start, end, color, label });
}


void AnimationEventTrack::DeleteEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
        events.erase(events.begin() + index);
}

void AnimationEventTrack::DuplicateEvent(int index)
{
    if (index >= 0 && index < (int)events.size())
        events.push_back(events[index]);
}

void AnimationEventTrack::BeginEdit(int index)
{
    editingEntry = index;
}

void AnimationEventTrack::EndEdit()
{
    editingEntry = -1;
    editingPart = 0;
}

void AnimationEventTrack::DoubleClick(int index)
{
    // TODO: ダブルクリックで何かする場合
}

void AnimationEventTrack::Copy()
{
    if (editingEntry >= 0)
        clipboard = events[editingEntry];
}

void AnimationEventTrack::Paste()
{
    if (editingEntry >= 0)
        events[editingEntry] = clipboard;
}

bool AnimationEventTrack::DrawSequencer(
    float* currentTime,             // 現在の秒数
    bool* expanded,
    int* selectedEntry,
    float* firstVisibleTime,        // スクロール位置（秒）
    float animationLengthSeconds
)
{
    const float pixelsPerSecond = 100.0f;      // 1 秒 = 100px
    const float rowHeight = 24.0f;

    ImGui::BeginChild("SequencerRegion", ImVec2(0, 180), true, ImGuiWindowFlags_HorizontalScrollbar);
    float contentWidth = animationLengthSeconds * pixelsPerSecond;
    ImGui::Dummy(ImVec2(contentWidth, 0));   // スクロール可能領域確保

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    // 背景
    draw_list->AddRectFilled(origin, origin + ImVec2(contentWidth, 180), IM_COL32(30, 30, 30, 255));

    // 時間メモリ線（5秒ごと）
    for (float t = 0; t <= animationLengthSeconds; t += 5.0f)
    {
        float x = origin.x + t * pixelsPerSecond;
        draw_list->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + 180), IM_COL32(100, 100, 100, 255));
        char buf[32]; sprintf_s(buf, "%.0f s", t);
        draw_list->AddText(ImVec2(x + 2, origin.y + 2), IM_COL32_WHITE, buf);
    }

    if (*expanded)
    {
        for (int i = 0; i < events.size(); ++i)
        {
            auto& ev = events[i];

            float x0 = origin.x + ev.startTime * pixelsPerSecond;
            float x1 = origin.x + ev.endTime * pixelsPerSecond;

            // バー描画
            draw_list->AddRectFilled(ImVec2(x0, origin.y + rowHeight * i),
                ImVec2(x1, origin.y + rowHeight * (i + 1)), ev.color);
            draw_list->AddRect(ImVec2(x0, origin.y + rowHeight * i),
                ImVec2(x1, origin.y + rowHeight * (i + 1)), IM_COL32(0, 0, 0, 255));
            draw_list->AddText(ImVec2(x0 + 4, origin.y + rowHeight * i + 4), IM_COL32(0, 0, 0, 255), ev.label.c_str());

            // -----------------------------
            // start ハンドル
            // -----------------------------
            ImGui::SetCursorScreenPos(ImVec2(x0 - 5, origin.y + rowHeight * i));
            ImGui::InvisibleButton(("ev_start_" + std::to_string(i)).c_str(), ImVec2(10, rowHeight));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float mx = ImGui::GetIO().MousePos.x - origin.x;
                ev.startTime = std::max(0.0f, std::min(mx / pixelsPerSecond, ev.endTime));
            }

            // -----------------------------
            // end ハンドル
            // -----------------------------
            ImGui::SetCursorScreenPos(ImVec2(x1 - 5, origin.y + rowHeight * i));
            ImGui::InvisibleButton(("ev_end_" + std::to_string(i)).c_str(), ImVec2(10, rowHeight));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float mx = ImGui::GetIO().MousePos.x - origin.x;
                ev.endTime = std::max(ev.startTime, std::min(mx / pixelsPerSecond, animationLengthSeconds));
            }

            // -----------------------------
            // バー全体クリック用 InvisibleButton（start/end の後に置く）
            // -----------------------------
            ImGui::SetCursorScreenPos(ImVec2(x0 + 5, origin.y + rowHeight * i));  // 中央部分だけ
            ImGui::InvisibleButton(("ev_full_" + std::to_string(i)).c_str(), ImVec2((x1 - x0) - 10, rowHeight));
            if (ImGui::IsItemClicked())
                *selectedEntry = i;
        }
    }

    // 現在時間カーソル
    float cx = origin.x + (*currentTime) * pixelsPerSecond;
    draw_list->AddLine(ImVec2(cx, origin.y), ImVec2(cx, origin.y + 180), IM_COL32(255, 0, 0, 255), 2.0f);

    // カーソルドラッグ用
    ImGui::SetCursorScreenPos(ImVec2(cx - 5, origin.y));
    ImGui::InvisibleButton("CurrentTimeDragZone", ImVec2(10, 180));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float mouseX = ImGui::GetIO().MousePos.x;
        *currentTime = std::max(0.0f, std::min((mouseX - origin.x) / pixelsPerSecond, animationLengthSeconds));
    }

    ImGui::EndChild();
    return true;
}
