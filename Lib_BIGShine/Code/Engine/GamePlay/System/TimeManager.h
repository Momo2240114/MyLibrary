#pragma once
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cmath>

class BlackboardComponent;

enum class TarGetLayerType : uint32_t
{
    Default = 0,
    Player = 1u << 0,
    Enemy = 1u << 1,
    PlayerBullet = 1u << 2,
    EnemyBullet = 1u << 3,
    UI = 1u << 4,
    Environment = 1u << 5,
    SneakObject = 1u << 6,
    Terrain = 1u << 7,
    Wall = 1u << 9,
    Item = 1u << 10,
    All = 0xFFFFFFFFu
};

struct TimeState
{
    float currentScale = 1.0f;
    float targetScale = 1.0f;
    float lerpSpeed = 10.0f;
    float hitStopTimer = 0.0f;
    float hitStopScale = 0.0f;

    void Update(float rawDt)
    {
        if (hitStopTimer > 0.0f)
        {
            hitStopTimer -= rawDt;
            currentScale = hitStopScale;
            if (hitStopTimer <= 0.0f) hitStopTimer = 0.0f;
        }
        else
        {
            currentScale += (targetScale - currentScale) * (lerpSpeed * rawDt);
            if (std::abs(targetScale - currentScale) < 0.001f)
            {
                currentScale = targetScale;
            }
        }
    }
};

class TimeManager
{
private:
    TimeState defaultState;
    std::unordered_map<TarGetLayerType, TimeState> states;
    float cachedDefaultScale = 1.0f;
    float cachedLayerScales[32]{};

public:
    TimeManager();

    void Update(float rawDt_);
    void SetTargetScale(uint32_t layerMask, float scale, float speed = 10.0f);
    void TriggerHitStop(uint32_t layerMask_, float duration_, float scale_ = 0.0f);
    void PrepareCache();
    float GetCachedScale(uint32_t layerMask) const;
};
