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
    enum class PHASE
    {
        IDLE,
        TO_TARGET,
        HOLD,
        TO_DEFAULT
    };

    float currentScale = 1.0f;
    float defaultScale = 1.0f;
    float targetScale = 1.0f;
    float lerpSpeed = 10.0f;
    float hitStopTimer = 0.0f;
    float hitStopScale = 0.0f;
    float startScale = 1.0f;
    float transitionDuration = 0.0f;
    float holdDuration = 0.0f;
    float returnDuration = 0.0f;
    float phaseElapsed = 0.0f;
    PHASE phase = PHASE::IDLE;

    void Update(float rawDt_)
    {
        if (hitStopTimer > 0.0f)
        {
            hitStopTimer -= rawDt_;
            currentScale = hitStopScale;
            if (hitStopTimer <= 0.0f) hitStopTimer = 0.0f;
        }
        else if (phase != PHASE::IDLE)
        {
            phaseElapsed += rawDt_;
            if (phase == PHASE::TO_TARGET)
            {
                const float ratio_ = transitionDuration > 0.0f
                    ? (std::min)(phaseElapsed / transitionDuration, 1.0f)
                    : 1.0f;
                currentScale = startScale + (targetScale - startScale) * ratio_;
                if (ratio_ >= 1.0f)
                {
                    phase = PHASE::HOLD;
                    phaseElapsed = 0.0f;
                }
            }
            else if (phase == PHASE::HOLD)
            {
                currentScale = targetScale;
                if (phaseElapsed >= holdDuration)
                {
                    phase = PHASE::TO_DEFAULT;
                    phaseElapsed = 0.0f;
                    startScale = currentScale;
                }
            }
            else if (phase == PHASE::TO_DEFAULT)
            {
                const float ratio_ = returnDuration > 0.0f
                    ? (std::min)(phaseElapsed / returnDuration, 1.0f)
                    : 1.0f;
                currentScale = startScale + (defaultScale - startScale) * ratio_;
                if (ratio_ >= 1.0f)
                {
                    currentScale = defaultScale;
                    targetScale = defaultScale;
                    phase = PHASE::IDLE;
                }
            }
        }
        else
        {
            currentScale += (targetScale - currentScale) * (lerpSpeed * rawDt_);
            if (std::abs(targetScale - currentScale) < 0.001f)
            {
                currentScale = targetScale;
            }
        }
    }

    void Play(float targetScale_, float transitionDuration_, float holdDuration_, float returnDuration_, float defaultScale_)
    {
        defaultScale = (std::max)(defaultScale_, 0.0f);
        targetScale = (std::max)(targetScale_, 0.0f);
        startScale = currentScale;
        transitionDuration = (std::max)(transitionDuration_, 0.0f);
        holdDuration = (std::max)(holdDuration_, 0.0f);
        returnDuration = (std::max)(returnDuration_, 0.0f);
        phaseElapsed = 0.0f;
        phase = PHASE::TO_TARGET;
    }

    void Reset(bool resetConfiguredDefault_ = false)
    {
        if (resetConfiguredDefault_) defaultScale = 1.0f;
        currentScale = defaultScale;
        targetScale = defaultScale;
        hitStopTimer = 0.0f;
        phaseElapsed = 0.0f;
        phase = PHASE::IDLE;
    }
};

class TimeManager
{
private:
    TimeState defaultState;
    std::unordered_map<TarGetLayerType, TimeState> states;
    float cachedDefaultScale = 1.0f;
    float cachedLayerScales[32]{};
    float lastRawDeltaTime = 0.0f;
    bool paused = false;
    bool frameStepRequested = false;
    bool frameStepActive = false;
    bool cameraUnscaledWhilePaused = true;

public:
    TimeManager();

    void Update(float rawDt_);
    void SetPaused(bool paused_);
    void TogglePaused();
    void RequestFrameStep();
    bool IsPaused() const { return paused; }
    bool IsFrameStepActive() const { return frameStepActive; }
    void SetCameraUnscaledWhilePaused(bool enabled_) { cameraUnscaledWhilePaused = enabled_; }
    bool IsCameraUnscaledWhilePaused() const { return cameraUnscaledWhilePaused; }
    bool ShouldUseUnscaledCameraDelta() const { return paused && cameraUnscaledWhilePaused && !frameStepActive; }
    void SetTargetScale(uint32_t layerMask, float scale, float speed = 10.0f);
    void PlayTimedScale(uint32_t layerMask_, float targetScale_, float transitionDuration_,
        float holdDuration_, float returnDuration_, float defaultScale_ = 1.0f);
    void ResetToDefaults(
        uint32_t layerMask_ = static_cast<uint32_t>(TarGetLayerType::All),
        bool resetConfiguredDefaults_ = false);
    void TriggerHitStop(uint32_t layerMask_, float duration_, float scale_ = 0.0f);
    void PrepareCache();
    float GetCachedScale(uint32_t layerMask) const;
    float GetLastRawDeltaTime() const { return lastRawDeltaTime; }
};
