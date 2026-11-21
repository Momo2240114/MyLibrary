#pragma once
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <cmath>

class BlackboardComponent;

enum class TarGetLayerType : uint32_t
{
    // DefaultはLayer指定なし。TimeManagerでは全体Scaleの参照にも使う。
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
        // 通常状態。targetScaleへlerpSpeedで追従する。
        IDLE,
        // TimedScale開始時、現在ScaleからtargetScaleへ補間中。
        TO_TARGET,
        // targetScaleを一定時間維持中。
        HOLD,
        // defaultScaleへ戻る補間中。
        TO_DEFAULT
    };

    // 現在Frameで使うTimeScale。GameObjectManagerがLayerごとに参照する。
    float currentScale = 1.0f;
    // TimedScale終了後に戻るScale。
    float defaultScale = 1.0f;
    float targetScale = 1.0f;
    float lerpSpeed = 10.0f;
    // HitStopは通常のTimedScaleより優先される短時間停止。
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
        // HitStop中は演出補間より優先して固定Scaleを使う。
        // rawDtでTimerだけ進めることで、Game側が止まっていてもHitStopは解除される。
        if (hitStopTimer > 0.0f)
        {
            hitStopTimer -= rawDt_;
            currentScale = hitStopScale;
            if (hitStopTimer <= 0.0f)
            {
                hitStopTimer = 0.0f;
            }
        }
        else if (phase != PHASE::IDLE)
        {
            // TimedScaleは「目的Scaleへ移動 -> 維持 -> defaultへ戻す」の3段階で進む。
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
            // 通常のSetTargetScaleは、targetScaleへ指数的に近づける簡易補間として扱う。
            currentScale += (targetScale - currentScale) * (lerpSpeed * rawDt_);
            if (std::abs(targetScale - currentScale) < 0.001f)
            {
                currentScale = targetScale;
            }
        }
    }

    void Play(float targetScale_, float transitionDuration_, float holdDuration_, float returnDuration_, float defaultScale_)
    {
        // 演出用の一時的なTimeScaleを開始する。
        // 現在ScaleをstartScaleとして保持し、途中から呼ばれても自然につながるようにする。
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
        // Scale状態をdefaultへ戻す。resetConfiguredDefaultがtrueなら設定済みdefault自体も1へ戻す。
        if (resetConfiguredDefault_)
        {
            defaultScale = 1.0f;
        }
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
    // Layer指定なし、または全体へ適用する標準TimeState。
    TimeState defaultState;
    // LayerごとのTimeState。必要なLayerだけ作られる。
    std::unordered_map<TarGetLayerType, TimeState> states;
    // Update後にPrepareCacheで確定したScale。Frame中の参照を安定させるためCacheする。
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
    // Pause中は基本的にsimulationRawDtを0にし、FrameStep要求時だけ1Frame進める。
    void SetPaused(bool paused_);
    void TogglePaused();
    void RequestFrameStep();

    bool IsPaused() const
    {
        return paused;
    }

    bool IsFrameStepActive() const
    {
        return frameStepActive;
    }

    void SetCameraUnscaledWhilePaused(bool enabled_)
    {
        cameraUnscaledWhilePaused = enabled_;
    }

    bool IsCameraUnscaledWhilePaused() const
    {
        return cameraUnscaledWhilePaused;
    }

    bool ShouldUseUnscaledCameraDelta() const
    {
        return paused && cameraUnscaledWhilePaused && !frameStepActive;
    }

    // 指定LayerのtargetScaleを設定する。Allなら全体と各Layerへ適用する。
    void SetTargetScale(uint32_t layerMask, float scale, float speed = 10.0f);
    // targetへ遷移、維持、defaultへ復帰する一時的なTimeScale演出を再生する。
    void PlayTimedScale(uint32_t layerMask_, float targetScale_, float transitionDuration_,
        float holdDuration_, float returnDuration_, float defaultScale_ = 1.0f);
    void ResetToDefaults(
        uint32_t layerMask_ = static_cast<uint32_t>(TarGetLayerType::All),
        bool resetConfiguredDefaults_ = false);
    void TriggerHitStop(uint32_t layerMask_, float duration_, float scale_ = 0.0f);
    // GameObjectManagerがFrame中に高速参照できるよう、LayerごとのScaleを配列へ確定する。
    void PrepareCache();
    float GetCachedScale(uint32_t layerMask) const;

    float GetLastRawDeltaTime() const
    {
        return lastRawDeltaTime;
    }
};
