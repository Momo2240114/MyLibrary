#include "TimeManager.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

TimeManager::TimeManager()
{
    for (float& scale : cachedLayerScales)
    {
        scale = 1.0f;
    }
}

void TimeManager::Update(float rawDt_)
{
    lastRawDeltaTime = (std::max)(rawDt_, 0.0f);
    frameStepActive = paused && frameStepRequested;
    frameStepRequested = false;

    const float simulationRawDt_ = (paused && !frameStepActive) ? 0.0f : lastRawDeltaTime;

    defaultState.Update(simulationRawDt_);

    for (auto& [type, state] : states)
    {
        state.Update(simulationRawDt_);
    }
}

void TimeManager::SetPaused(bool paused_)
{
    paused = paused_;
    if (!paused)
    {
        frameStepRequested = false;
        frameStepActive = false;
    }
}

void TimeManager::TogglePaused()
{
    SetPaused(!paused);
}

void TimeManager::RequestFrameStep()
{
    paused = true;
    frameStepRequested = true;
}

void TimeManager::SetTargetScale(uint32_t layerMask_, float scale_, float speed_)
{
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.targetScale = scale_;
        defaultState.lerpSpeed = speed_;
        defaultState.phase = TimeState::PHASE::IDLE;
        if (layerMask_ == 0u) return;
    }

    for (uint32_t i_ = 0; i_ < 32; ++i_)
    {
        uint32_t bit_ = 1u << i_;

        if ((layerMask_ & bit_) != 0u)
        {
            auto& state = states[static_cast<TarGetLayerType>(bit_)];
            state.targetScale = scale_;
            state.lerpSpeed = speed_;
            state.phase = TimeState::PHASE::IDLE;
        }
    }
}

void TimeManager::TriggerHitStop(uint32_t layerMask_, float duration_, float scale_)
{
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.hitStopTimer = duration_;
        defaultState.hitStopScale = (std::clamp)(scale_, 0.0f, 1.0f);
        if (layerMask_ == 0u) return;
    }

    for (uint32_t i_ = 0; i_ < 32; ++i_)
    {
        uint32_t bit_ = 1u << i_;

        if ((layerMask_ & bit_) != 0u)
        {
            TimeState& state_ = states[static_cast<TarGetLayerType>(bit_)];
            state_.hitStopTimer = duration_;
            state_.hitStopScale = (std::clamp)(scale_, 0.0f, 1.0f);
        }
    }
}

void TimeManager::PrepareCache()
{
    const float pauseScale_ = (paused && !frameStepActive) ? 0.0f : 1.0f;
    cachedDefaultScale = defaultState.currentScale * pauseScale_;

    for (uint32_t i_ = 0; i_ < 32; ++i_)
    {
        uint32_t bit_ = 1u << i_;
        TarGetLayerType type_ = static_cast<TarGetLayerType>(bit_);

        auto it = states.find(type_);
        if (it != states.end())
        {
            cachedLayerScales[i_] = it->second.currentScale * pauseScale_;
        }
        else
        {
            cachedLayerScales[i_] = pauseScale_;
        }
    }
}

float TimeManager::GetCachedScale(uint32_t layerMask_) const
{
    // Default = 0 対策
    if (layerMask_ == 0u)
    {
        return cachedDefaultScale;
    }

    float result_ = 1.0f;

    uint32_t mask_ = layerMask_;

    while (mask_ != 0u)
    {
#if defined(_MSC_VER)
        unsigned long index_ = 0;
        if (!_BitScanForward(&index_, mask_))
        {
            break;
        }
#else
        uint32_t index = static_cast<uint32_t>(__builtin_ctz(mask));
#endif

        if (index_ < 32)
        {
            result_ *= cachedLayerScales[index_];
        }

        mask_ &= ~(1u << index_);
    }

    return result_;
}

void TimeManager::PlayTimedScale(
    uint32_t layerMask_,
    float targetScale_,
    float transitionDuration_,
    float holdDuration_,
    float returnDuration_,
    float defaultScale_)
{
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.Play(targetScale_, transitionDuration_, holdDuration_, returnDuration_, defaultScale_);
        if (layerMask_ == 0u) return;
    }

    for (uint32_t index_ = 0; index_ < 32; ++index_)
    {
        const uint32_t bit_ = 1u << index_;
        if ((layerMask_ & bit_) != 0u)
        {
            states[static_cast<TarGetLayerType>(bit_)].Play(
                targetScale_, transitionDuration_, holdDuration_, returnDuration_, defaultScale_);
        }
    }
}

void TimeManager::ResetToDefaults(uint32_t layerMask_, bool resetConfiguredDefaults_)
{
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.Reset(resetConfiguredDefaults_);
        if (layerMask_ == 0u) return;
    }

    for (uint32_t index_ = 0; index_ < 32; ++index_)
    {
        const uint32_t bit_ = 1u << index_;
        if ((layerMask_ & bit_) != 0u)
        {
            states[static_cast<TarGetLayerType>(bit_)].Reset(resetConfiguredDefaults_);
        }
    }
}
