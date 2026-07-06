#pragma once

#include "imgui.h"
#include "ImSequencer.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

class EventTrackSequencer : public ImSequencer::SequenceInterface
{
public:
    EventTrackSequencer(EventTrack* track, float fps = 30.0f)
        : eventTrack(track), framesPerSecond(fps) {
    }

    // Frame range
    int GetFrameMin() const override { return 0; }
    int GetFrameMax() const override { return static_cast<int>(eventTrack->GetLength() * framesPerSecond); }

    // Item count
    int GetItemCount() const override { return static_cast<int>(eventTrack->GetItemCount()); }

    const char* GetItemLabel(int index) const override
    {
        return eventTrack->GetEvent(index).label.c_str();
    }

    void Get(int index, int** start, int** end, int* type, unsigned int* color) override
    {
        const auto& ev = eventTrack->GetEvent(index);
        if (start) *start = new int(static_cast<int>(ev.startTime * framesPerSecond));
        if (end)   *end = new int(static_cast<int>(ev.endTime * framesPerSecond));
        if (type)  *type = static_cast<int>(ev.type);
        if (color) *color = ev.color;
    }

    void Add(int typeIndex) override
    {
        eventTrack->AddEvent(static_cast<EventType>(typeIndex), "NewEvent", 0.0f, 1.0f, 0xFF00FFFF);
    }
    void Del(int index) override { eventTrack->DeleteEvent(index); }
    void Duplicate(int index) override { eventTrack->DuplicateEvent(index); }

    void Copy() override { eventTrack->Copy(); }
    void Paste() override { eventTrack->Paste(); }
    void BeginEdit(int index) override { eventTrack->BeginEdit(index); }
    void EndEdit() override { eventTrack->EndEdit(); }
    void DoubleClick(int index) override { eventTrack->DoubleClick(index); }

    int GetItemTypeCount() const override { return 6; }
    const char* GetItemTypeName(int typeIndex) const override
    {
        static const char* names[] = { "Generic", "Animation", "Camera", "Sound", "UI", "Script" };
        return names[typeIndex];
    }

    // -----------------------------
    // CustomDraw で現在時間に応じてイベント実行
    // -----------------------------
    void CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc,
        const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect)
    {
        if (!eventTrack) return;

        auto& ev = eventTrack->GetEvent(index);
        float pixelsPerSecond = 100.0f;

        ImVec2 origin = rc.Min;
        float yTop = origin.y;
        float yBottom = rc.Max.y;

        // バー描画
        float x0 = origin.x + ev.startTime * pixelsPerSecond;
        float x1 = origin.x + ev.endTime * pixelsPerSecond;
        ImVec2 pMin = { x0, yTop };
        ImVec2 pMax = { x1, yBottom };
        draw_list->AddRectFilled(pMin, pMax, ev.color);
        draw_list->AddRect(pMin, pMax, IM_COL32(0, 0, 0, 255));

        // ドラッグ用 InvisibleButton（開始端）
        ImGui::SetCursorScreenPos({ x0 - 5, yTop });
        ImGui::InvisibleButton(("ev_start_" + std::to_string(index)).c_str(), { 10, yBottom - yTop });
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float mx = ImGui::GetIO().MousePos.x - origin.x;
            ev.startTime = std::max(0.0f, std::min(mx / pixelsPerSecond, ev.endTime));
        }

        // ドラッグ用 InvisibleButton（終了端）
        ImGui::SetCursorScreenPos({ x1 - 5, yTop });
        ImGui::InvisibleButton(("ev_end_" + std::to_string(index)).c_str(), { 10, yBottom - yTop });
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            float mx = ImGui::GetIO().MousePos.x - origin.x;
            ev.endTime = std::max(ev.startTime, mx / pixelsPerSecond);
        }

        // バー全体クリックで選択
        ImGui::SetCursorScreenPos({ x0 + 5, yTop });
        ImGui::InvisibleButton(("ev_full_" + std::to_string(index)).c_str(), { (x1 - x0) - 10, yBottom - yTop });
        if (ImGui::IsItemClicked())
        {
            // 選択処理
        }
    }

    void SetCurrentTimePointer(float* timePtr) { currentTimePtr = timePtr; }

private:
    EventTrack* eventTrack;
    float framesPerSecond = 30.0f;
    float* currentTimePtr = nullptr; // 再生中の現在時間を Sequencer から受け取る
};