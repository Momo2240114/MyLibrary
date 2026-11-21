#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "BaseEvent.h"


enum class EventType
{
    // Track上で扱うEventの種類。Editor表示や追加時の分類に使う。
    Generic,
    Animation,
    Camera,
    Sound,
    UI,
    Script
};

struct EventItem
{
    // Timeline上のEvent本体。startTimeからendTimeまでの間、Actionを段階的に呼ぶ。
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

    // currentTimeを見て、EventのStart/Update/End Actionを必要なタイミングで実行する。
    void Update(float currentTime, std::shared_ptr<GameObject> owner);
    // LocalRegistryを優先し、見つからなければGlobalRegistryからActionを実行する共通ヘルパー。
    void Execute(const std::string& name, std::shared_ptr<GameObject> owner);

    // イベント操作
    void AddEvent(EventType type, const std::string& label, float start, float end, unsigned int color);
    void DeleteEvent(int index);
    void DuplicateEvent(int index);
    void Copy();
    void Paste();
    // ImSequencerから編集開始/終了を受け取り、Copy/Paste対象を決める。
    void BeginEdit(int index);
    void EndEdit();
    void DoubleClick(int index);

    // EventTrack専用のTimeline Editorを描画する。
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
        // Track全体の長さは、登録済みEventの最も遅いendTimeで決める。
        float maxTime = 0.0f;
        for (auto& ev : events)
        {
            if (ev.endTime > maxTime)
            {
                maxTime = ev.endTime;
            }
        }
        return maxTime;
    }

    // アクセス
    int GetItemCount() const
    {
        return (int)events.size();
    }

    const EventItem& GetEvent(int index) const
    {
        return events[index];
    }

    EventItem& GetEvent(int index)
    {
        return events[index];
    }

private:
    // Track上に並ぶEvent一覧。
    std::vector<EventItem> events;
    // Editor表示用のTrack名。
    std::string trackName;
    // AddEvent時に標準で使うEvent種別。
    EventType defaultEventType;

    // 編集中のEvent Index。-1なら未選択。
    int editingEntry = -1;
    // 将来的に開始端/終了端など編集部位を分けるための値。
    int editingPart = 0;

    // Timeline描画時の拡大率と左Label幅。
    float zoomScale = 100.0f; // 1秒あたりのピクセル数 (pixelsPerSecond)
    float labelWidth = 120.0f; // 左側のラベル幅も可変にすると便利

    // Copy/Paste用に一時保持するEvent。
    EventItem clipboard;
};
