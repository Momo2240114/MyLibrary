#pragma once

class BaseEvent
{
public:
    virtual ~BaseEvent() = default;

    virtual void OnStart() {}
    virtual void OnUpdate(float dt, float localTime) {}
    virtual void OnEnd() {}
};

class GenericEvent : public BaseEvent
{
public:
    GenericEvent(const std::string& label = "") : label(label) {}

    void OnStart() override {}
    void OnUpdate(float dt, float localTime) override {}
    void OnEnd() override {}

    std::string label;
};