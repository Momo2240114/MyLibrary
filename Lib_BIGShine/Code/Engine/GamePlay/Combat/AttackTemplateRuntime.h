#pragma once

#include <memory>

class GameObject;
struct AnimEvent;

class AttackTemplateRuntime
{
public:
    static void Start(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_);
    static void Update(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_, float elapsedTime_);
    static void End(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_);
};
