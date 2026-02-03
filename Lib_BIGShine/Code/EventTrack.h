#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "BaseEvent.h"


enum class EventType
{
    Generic,
    Animation,
    Camera,
    Sound,
    UI,
    Script
};

struct EventItem
{
    EventType type = EventType::Generic;
    float startTime = 0;
    float endTime = 0;
    unsigned int color = 0xFFFFFFFF;   // UI 表示用
    std::string label;                 // 表示名（任意）
  
    // イベントの状態管理用
    bool started = false;
    bool ended = false;

    std::string payload;               // JSON や Script 名など保存可能
    // Start / Update / End コールバック
    std::function<void(EventItem&)> onStart;
    std::function<void(EventItem&, float currentTime)> onUpdate;
    std::function<void(EventItem&)> onEnd;
};

class EventTrack
{
public:
    EventTrack(const std::string& name = "", EventType defaultType = EventType::Generic);

    //// 現在時刻に応じてイベントを処理
    //void UpdateEvents(float currentTime);
    //// 再生リセット（ループや停止用）
    //void ResetEvents()
    //{
    //    for (auto& ev : events)
    //    {
    //        ev.started = false;
    //        ev.ended = false;
    //    }
    //}

    // イベント操作
    void AddEvent(EventType type, const std::string& label, float start, float end, unsigned int color);
    void DeleteEvent(int index);
    void DuplicateEvent(int index);
    void Copy();
    void Paste();
    void BeginEdit(int index);
    void EndEdit();
    void DoubleClick(int index);

    // ImGui
    bool DrawSequencer(
        float* currentTime,
        bool* expanded,
        int* selectedEntry,
        float* firstVisibleTime,
        float lengthSeconds
    );
    

    float GetLength() const
    {
        float maxTime = 0.0f;
        for (auto& ev : events)
            if (ev.endTime > maxTime) maxTime = ev.endTime;
        return maxTime;
    }

    // アクセス
    int GetItemCount() const { return (int)events.size(); }
    const EventItem& GetEvent(int index) const { return events[index]; }
    EventItem& GetEvent(int index) { return events[index]; }

private:
    std::vector<EventItem> events;
    std::string trackName;
    EventType defaultEventType;

    // 編集
    int editingEntry = -1;
    int editingPart = 0;

    EventItem clipboard;
};
