#pragma once

class BaseEvent
{
public:
    virtual ~BaseEvent() = default;

    // Event開始時に一度だけ呼ばれる入口。
    virtual void OnStart()
    {
    }

    // Event再生中に毎Frame呼ばれる。dtは差分時間、localTimeはEvent内での経過時間。
    virtual void OnUpdate(float dt, float localTime)
    {
    }

    // Event終了時の後片付け用。派生Eventで状態を戻したい場合に上書きする。
    virtual void OnEnd()
    {
    }
};

class GenericEvent : public BaseEvent
{
public:
    // Labelだけを持つ汎用Event。EditorやDebug上で名前付きの空Eventとして扱う。
    GenericEvent(const std::string& label = "") : label(label)
    {
    }

    // 必要な処理は派生先や外部のEvent管理側に任せるため、ここでは何もしない。
    void OnStart() override
    {
    }

    void OnUpdate(float dt, float localTime) override
    {
    }

    void OnEnd() override
    {
    }

    // TimelineやEditor表示で識別するためのEvent名。
    std::string label;
};
