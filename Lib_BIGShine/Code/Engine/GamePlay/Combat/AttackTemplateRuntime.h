#pragma once

#include <memory>

class GameObject;
struct AnimEvent;
struct AnimState;
class GltfModelCommponennt;

// Animation Sequencerの攻撃イベントとAttackTemplateをつなぐ実行用ヘルパー。
// Start/Update/EndでHitBoxの有効化、攻撃VFX生成、終了時の後片付けを担当する。
class AttackTemplateRuntime
{
public:
    // 描画姿勢を復元しながら、通過した判定窓の武器軌道だけを取得する。
    static void SampleWeaponInterval(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_,
        GltfModelCommponennt* model_, const AnimState& state_, float previousTime_, float currentTime_);
    static void Start(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_);
    static void Update(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_, float elapsedTime_);
    static void End(const std::shared_ptr<GameObject>& owner_, AnimEvent& event_);
};
