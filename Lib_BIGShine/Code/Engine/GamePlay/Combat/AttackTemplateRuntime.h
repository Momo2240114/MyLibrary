#pragma once

#include <memory>

class GameObject;
struct AnimEvent;

// Animation Sequencerの攻撃イベントとAttackTemplateをつなぐ実行用ヘルパー。
// Start/Update/EndでHitBoxの有効化、攻撃VFX生成、終了時の後片付けを担当する。
class AttackTemplateRuntime
{
public:
    static void Start(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_);
    static void Update(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_, float elapsedTime_);
    static void End(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_);
};
