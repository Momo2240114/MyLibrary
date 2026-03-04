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
    unsigned int color = 0xFFFFFFFF;
    std::string label;                 // 表示名（任意）
  
    // タイミングごとに違う名前を保存できるようにする
    std::string actionStart;
    std::string actionUpdate;
    std::string actionEnd;

    // イベントの状態管理用
    bool started = false;
    bool ended = false;

    std::string payload;               // JSON や Script 名など保存可能
    // Start / Update / End コールバック
    std::function<void(EventItem&)> onStart;
    std::function<void(EventItem&, float currentTime)> onUpdate;
    std::function<void(EventItem&)> onEnd;
};

class GameObject;

class EventTrack
{
public:
    EventTrack(const std::string& name = "", EventType defaultType = EventType::Generic);

    void Update(float currentTime, std::shared_ptr<GameObject> owner);
    // 共通の実行ヘルパー
    void Execute(const std::string& name, std::shared_ptr<GameObject> owner);

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
        float lengthSeconds,
        std::shared_ptr<GameObject> owner
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

    float zoomScale = 100.0f; // 1秒あたりのピクセル数 (pixelsPerSecond)
    float labelWidth = 120.0f; // 左側のラベル幅も可変にすると便利

    EventItem clipboard;
};
