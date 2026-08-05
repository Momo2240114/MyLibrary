#include "PrecompiledHeader.h"

#include "AnimationEventTrack.h"

#include <algorithm>
#include <cmath>

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

AnimationEventTrack::AnimationEventTrack()
    : editingEntry(-1), editingPart(0)
{
}

void AnimationEventTrack::AddEvent(const std::string& label_, float start_, float end_, unsigned int color_)
{
    // 追加時点では範囲補正せず、呼び出し側が指定した秒数をそのまま保持する。
    events.push_back({ start_, end_, color_, label_ });
}


void AnimationEventTrack::DeleteEvent(int index_)
{
    // UIから渡された選択値が無効な場合は、既存イベントを変更しない。
    if (index_ >= 0 && index_ < static_cast<int>(events.size()))
        events.erase(events.begin() + index_);
}

void AnimationEventTrack::DuplicateEvent(int index_)
{
    // 複製後の位置調整は Editor 側のドラッグで行う想定。
    if (index_ >= 0 && index_ < static_cast<int>(events.size()))
        events.push_back(events[index_]);
}

void AnimationEventTrack::BeginEdit(int index_)
{
    // Copy/Paste の対象を現在選択中のイベントへ切り替える。
    editingEntry = index_;
}

void AnimationEventTrack::EndEdit()
{
    editingEntry = -1;
    editingPart = 0;
}

void AnimationEventTrack::DoubleClick(int index_)
{
    // 詳細編集ダイアログなどを追加する際の拡張ポイントとして保持する。
    (void)index_;
}

void AnimationEventTrack::Copy()
{
    // 編集対象が無い時は clipboard を変更しない。
    if (editingEntry >= 0)
        clipboard = events[editingEntry];
}

void AnimationEventTrack::Paste()
{
    // 既存イベントへ上書きする形の Paste。新規追加ではない点に注意。
    if (editingEntry >= 0)
        events[editingEntry] = clipboard;
}

bool AnimationEventTrack::DrawSequencer(
    float* currentTime_,
    bool* expanded_,
    int* selectedEntry_,
    float* firstVisibleTime_,
    float animationLengthSeconds_
)
{
    const float pixelsPerSecond_ = 100.0f;
    const float rowHeight_ = 24.0f;
    (void)firstVisibleTime_; // ImGuiのスクロール機能へ移行済み。互換性維持のため引数は残す。

    ImGui::BeginChild("SequencerRegion", ImVec2(0, 180), true, ImGuiWindowFlags_HorizontalScrollbar);
    float contentWidth_ = animationLengthSeconds_ * pixelsPerSecond_;
    ImGui::Dummy(ImVec2(contentWidth_, 0)); // アニメーション尺に応じた横スクロール領域を確保する。

    ImDrawList* drawList_ = ImGui::GetWindowDrawList();
    ImVec2 origin_ = ImGui::GetCursorScreenPos();

    // Sequencer の背景を描き、イベントバーとのコントラストを確保する。
    drawList_->AddRectFilled(origin_, origin_ + ImVec2(contentWidth_, 180), IM_COL32(30, 30, 30, 255));

    // 長いアニメーションでも時間位置を把握できるよう、5秒間隔で目盛りを描画する。
    for (float time_ = 0; time_ <= animationLengthSeconds_; time_ += 5.0f)
    {
        float x_ = origin_.x + time_ * pixelsPerSecond_;
        drawList_->AddLine(ImVec2(x_, origin_.y), ImVec2(x_, origin_.y + 180), IM_COL32(100, 100, 100, 255));
        char timeText_[32];
        sprintf_s(timeText_, "%.0f s", time_);
        drawList_->AddText(ImVec2(x_ + 2, origin_.y + 2), IM_COL32_WHITE, timeText_);
    }

    if (*expanded_)
    {
        for (int index_ = 0; index_ < static_cast<int>(events.size()); ++index_)
        {
            auto& event_ = events[index_];

            float startX_ = origin_.x + event_.startTime * pixelsPerSecond_;
            float endX_ = origin_.x + event_.endTime * pixelsPerSecond_;

            // イベントの有効期間をバーとして可視化する。
            drawList_->AddRectFilled(ImVec2(startX_, origin_.y + rowHeight_ * index_),
                ImVec2(endX_, origin_.y + rowHeight_ * (index_ + 1)), event_.color);
            drawList_->AddRect(ImVec2(startX_, origin_.y + rowHeight_ * index_),
                ImVec2(endX_, origin_.y + rowHeight_ * (index_ + 1)), IM_COL32(0, 0, 0, 255));
            drawList_->AddText(ImVec2(startX_ + 4, origin_.y + rowHeight_ * index_ + 4),
                IM_COL32(0, 0, 0, 255), event_.label.c_str());

            // 左右のハンドルを別々に判定し、イベント期間が逆転しない範囲で編集する。
            ImGui::SetCursorScreenPos(ImVec2(startX_ - 5, origin_.y + rowHeight_ * index_));
            ImGui::InvisibleButton(("ev_start_" + std::to_string(index_)).c_str(), ImVec2(10, rowHeight_));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float mouseX_ = ImGui::GetIO().MousePos.x - origin_.x;
                event_.startTime = std::max(0.0f, std::min(mouseX_ / pixelsPerSecond_, event_.endTime));
            }

            ImGui::SetCursorScreenPos(ImVec2(endX_ - 5, origin_.y + rowHeight_ * index_));
            ImGui::InvisibleButton(("ev_end_" + std::to_string(index_)).c_str(), ImVec2(10, rowHeight_));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                float mouseX_ = ImGui::GetIO().MousePos.x - origin_.x;
                event_.endTime = std::max(event_.startTime,
                    std::min(mouseX_ / pixelsPerSecond_, animationLengthSeconds_));
            }

            // ハンドルを優先して判定するため、バー本体のクリック領域は最後に配置する。
            ImGui::SetCursorScreenPos(ImVec2(startX_ + 5, origin_.y + rowHeight_ * index_));
            ImGui::InvisibleButton(("ev_full_" + std::to_string(index_)).c_str(),
                ImVec2((endX_ - startX_) - 10, rowHeight_));
            if (ImGui::IsItemClicked())
                *selectedEntry_ = index_;
        }
    }

    // 再生位置を示すカーソルは、ドラッグ操作によるシークにも使用する。
    float cursorX_ = origin_.x + (*currentTime_) * pixelsPerSecond_;
    drawList_->AddLine(ImVec2(cursorX_, origin_.y), ImVec2(cursorX_, origin_.y + 180),
        IM_COL32(255, 0, 0, 255), 2.0f);

    ImGui::SetCursorScreenPos(ImVec2(cursorX_ - 5, origin_.y));
    ImGui::InvisibleButton("CurrentTimeDragZone", ImVec2(10, 180));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        float mouseX_ = ImGui::GetIO().MousePos.x;
        *currentTime_ = std::max(0.0f,
            std::min((mouseX_ - origin_.x) / pixelsPerSecond_, animationLengthSeconds_));
    }

    ImGui::EndChild();
    return true;
}
