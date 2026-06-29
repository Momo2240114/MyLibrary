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
    for (auto& [type, state] : states)
    {
        state.Update(rawDt_);
    }
}

void TimeManager::SetTargetScale(uint32_t layerMask_, float scale_, float speed_)
{
    if (layerMask_ == 0u) return;

    for (uint32_t i_ = 0; i_ < 32; ++i_)
    {
        uint32_t bit_ = 1u << i_;

        if ((layerMask_ & bit_) != 0u)
        {
            auto& state = states[static_cast<TarGetLayerType>(bit_)];
            state.targetScale = scale_;
            state.lerpSpeed = speed_;
        }
    }
}

void TimeManager::TriggerHitStop(uint32_t layerMask_, float duration_, float scale_)
{
    if (layerMask_ == 0u) return;

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
    for (uint32_t i_ = 0; i_ < 32; ++i_)
    {
        uint32_t bit_ = 1u << i_;
        TarGetLayerType type_ = static_cast<TarGetLayerType>(bit_);

        auto it = states.find(type_);
        if (it != states.end())
        {
            cachedLayerScales[i_] = it->second.currentScale;
        }
        else
        {
            cachedLayerScales[i_] = 1.0f;
        }
    }
}

float TimeManager::GetCachedScale(uint32_t layerMask_) const
{
    // Default = 0 ëŒçÙ
    if (layerMask_ == 0u)
    {
        return 1.0f;
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