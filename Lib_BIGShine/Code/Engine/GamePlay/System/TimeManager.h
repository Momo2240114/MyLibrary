// TimeManager.h
#pragma once
#include <unordered_map>
#include "Engine/GamePlay/Core/Object/GameObject.h" // LayerType を参照するため

enum class TarGetLayerType
{
    Default = 0,
    Player = 1 << 0,
    Enemy = 1 << 1,
    PlayerBullet = 1 << 2,
    EnemyBullet = 1 << 3,
    UI = 1 << 4,
    Environment = 1 << 5,
    SneakObject = 1 << 6,
    Terrain = 1 << 7,
    MaxLayers = 1 << 8,
    Wall = 1 << 9,
    Item = 1 << 10,
    All = 0xFFFFFFFF
};

struct TimeState {
    float currentScale = 1.0f;
    float targetScale = 1.0f;
    float lerpSpeed = 10.0f;
    float hitStopTimer = 0.0f;

    void Update(float rawDt) {
        if (hitStopTimer > 0.0f) {
            hitStopTimer -= rawDt;
            currentScale = 0.0f;
            if (hitStopTimer <= 0.0f) hitStopTimer = 0.0f;
        }
        else {
            currentScale += (targetScale - currentScale) * (lerpSpeed * rawDt);
            if (std::abs(targetScale - currentScale) < 0.001f) currentScale = targetScale;
        }
    }
};

class TimeManager {
private:
    std::unordered_map<TarGetLayerType, TimeState> m_states;
    // キャッシュ用配列：インデックスをレイヤーのビット位置に対応させる
    float m_cachedLayerScales[32];
public:
    void Update(float rawDt);
    void SetTargetScale(uint32_t layerMask, float scale, float speed = 10.0f);
    void TriggerHitStop(uint32_t layerMask, float duration);
    // Updateの頭で一度だけ呼ぶ計算処理
    void PrepareCache();

    // 非常に高速に取得できる
    float GetCachedScale(uint32_t layer) const;
};