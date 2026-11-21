#pragma once

#include "ImSequencer.h"
#include "AnimationState.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "imgui_internal.h" // ImRect 等の使用に必要


// =============================================================
// AnimationControllerComponent 用のシーケンサーアダプター
// =============================================================
struct AnimSequenceAdapter : public ImSequencer::SequenceInterface
{
    AnimState* targetState = nullptr;
    float duration = 0.0f;
    float fps = 60.0f;

    int ToFrame(float time) const
    {
        return static_cast<int>(time * fps);
    }

    float ToTime(int frame) const
    {
        return static_cast<float>(frame) / fps;
    }

    int GetClipCount() const
    {
        return targetState && targetState->type == AnimStateType::SEQUENCE
            ? static_cast<int>(targetState->sequenceClips.size())
            : 0;
    }

    int GetFrameMin() const override
    {
        return 0;
    }

    int GetFrameMax() const override
    {
        return ToFrame(duration);
    }

    int GetItemCount() const override
    {
        return targetState
            ? GetClipCount() + static_cast<int>(targetState->events.size())
            : 0;
    }

    int GetItemTypeCount() const override
    {
        return targetState && targetState->type == AnimStateType::SEQUENCE ? 2 : 1;
    }

    const char* GetItemTypeName(int typeIndex) const override
    {
        return targetState && targetState->type == AnimStateType::SEQUENCE && typeIndex == 0
            ? "Animation State Clip"
            : "Animation Event";
    }

    void SyncFrames()
    {
        if (!targetState)
        {
            return;
        }

        for (auto& clip_ : targetState->sequenceClips)
        {
            clip_.frameStart = ToFrame(clip_.startTime);
            clip_.frameEnd = ToFrame(clip_.endTime);
        }
        for (auto& event_ : targetState->events)
        {
            event_.frameStart = ToFrame(event_.startTime);
            event_.frameEnd = ToFrame(event_.endTime);
        }
    }

    // ドラッグ開始時に呼ばれる（必要に応じて初期状態の保存などが可能）
    void BeginEdit(int index) override {
        // 特になければ空でもOKですが、インターフェースとして存在を確認
    }

    // ドラッグ中や終了時に呼ばれ、フレームから秒へ変換して構造体に書き戻す
    void EndEdit() override
    {
        if (!targetState)
        {
            return;
        }
        for (auto& clip_ : targetState->sequenceClips)
        {
            clip_.startTime = ToTime(clip_.frameStart);
            clip_.endTime = ToTime(clip_.frameEnd);
        }
        for (auto& ev : targetState->events)
        {
            // ここが重要：ドラッグで書き換わった frameStart/End を float に戻す
            ev.startTime = ToTime(ev.frameStart);
            ev.endTime = ToTime(ev.frameEnd);
        }
    }

    // Get メソッドで確実にポインタを渡しているか再確認
    void Get(int index, int** start, int** end, int* type, unsigned int* color) override
    {
        if (!targetState || index < 0 || index >= GetItemCount())
        {
            return;
        }

        const int clipCount_ = GetClipCount();
        if (index < clipCount_)
        {
            auto& clip_ = targetState->sequenceClips[index];
            if (start)
            {
                *start = &clip_.frameStart;
            }
            if (end)
            {
                *end = &clip_.frameEnd;
            }
            if (type)
            {
                *type = 0;
            }
            if (color)
            {
                *color = 0xFF44AAEE;
            }
            return;
        }

        auto& ev = targetState->events[index - clipCount_];

        if (start)
        {
            *start = &ev.frameStart; // int* へのポインタを渡す
        }
        if (end)
        {
            *end = &ev.frameEnd;
        }
        if (type)
        {
            *type = targetState->type == AnimStateType::SEQUENCE ? 1 : 0;
        }
        if (color)
        {
            *color = 0xFFAA9900;
        }
    }

    // 左側の項目名リスト
    const char* GetItemLabel(int index) const override {
        if (!targetState || index < 0 || index >= GetItemCount())
        {
            return "";
        }

        const int clipCount_ = GetClipCount();
        if (index < clipCount_)
        {
            const auto& clip_ = targetState->sequenceClips[index];
            if (!clip_.stateName.empty())
            {
                return clip_.stateName.c_str();
            }
            return "Direct Animation Clip";
        }

        auto& ev = targetState->events[index - clipCount_];

        // 1. ラベルがあれば優先
        if (!ev.eventLabel.empty())
        {
            return ev.eventLabel.c_str();
        }

        return "Untitled Event";
    }

    // タイムライン上のバーの中に描画されるテキスト
    void CustomDraw(int index, ImDrawList* draw_list, const ImRect& rc, const ImRect& legendRect, const ImRect& clippingRect, const ImRect& legendClippingRect) override
    {
        if (!targetState || index < 0 || index >= GetItemCount())
        {
            return;
        }
        const int clipCount_ = GetClipCount();
        if (index < clipCount_)
        {
            const auto& clip_ = targetState->sequenceClips[index];
            std::string info_ = clip_.stateName.empty() ? "Direct Clip" : clip_.stateName;

            draw_list->PushClipRect(rc.Min, rc.Max, true);
            draw_list->AddText(ImVec2(rc.Min.x + 2, rc.Min.y + 2), 0xFFFFFFFF, info_.c_str());
            draw_list->PopClipRect();
            return;
        }

        auto& ev = targetState->events[index - clipCount_];

        // 表示用文字列の構築：ラベル優先
        std::string info = ev.eventLabel;
        if (info.empty()) {
            info = ev.startAction.empty() ? "New Event" : ev.startAction;
        }

        // 必要に応じてアクション情報も小さく追記したい場合は以下を有効に
        // if (!ev.updateAction.empty()) info += " (" + ev.updateAction + ")";

        draw_list->PushClipRect(rc.Min, rc.Max, true);
        draw_list->AddText(ImVec2(rc.Min.x + 2, rc.Min.y + 2), 0xFFFFFFFF, info.c_str());
        draw_list->PopClipRect();
    }

    void Add(int type) override {
        if (!targetState)
        {
            return;
        }
        if (targetState->type == AnimStateType::SEQUENCE && type == 0)
        {
            AnimSequenceClip clip_;
            clip_.startTime = 0.0f;
            clip_.endTime = duration * 0.2f;
            clip_.fitToRange = true;
            targetState->sequenceClips.push_back(clip_);
            return;
        }

        AnimEvent ev;
        ev.startTime = 0.0f;
        ev.endTime = duration * 0.2f;
        ev.eventLabel = "New Event"; // 初期ラベル
        ev.startAction = "";        // アクションは空にする（GUIで選択させる）
        targetState->events.push_back(ev);
    }

    // 指定したインデックスのClipまたはEventを削除する。
    void Del(int index) override {
        if (!targetState || index < 0 || index >= GetItemCount())
        {
            return;
        }

        const int clipCount_ = GetClipCount();
        if (index < clipCount_)
        {
            targetState->sequenceClips.erase(targetState->sequenceClips.begin() + index);
            return;
        }

        targetState->events.erase(targetState->events.begin() + (index - clipCount_));
    }
};
