#include "TimeManager.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

TimeManager::TimeManager()
{
    for (float& scale : m_cachedLayerScales)
    {
        scale = 1.0f;
    }
}

void TimeManager::Update(float rawDt)
{
    for (auto& [type, state] : m_states)
    {
        state.Update(rawDt);
    }
}

void TimeManager::SetTargetScale(uint32_t layerMask, float scale, float speed)
{
    if (layerMask == 0u) return;

    for (uint32_t i = 0; i < 32; ++i)
    {
        uint32_t bit = 1u << i;

        if ((layerMask & bit) != 0u)
        {
            auto& state = m_states[static_cast<TarGetLayerType>(bit)];
            state.targetScale = scale;
            state.lerpSpeed = speed;
        }
    }
}

void TimeManager::TriggerHitStop(uint32_t layerMask, float duration)
{
    if (layerMask == 0u) return;

    for (uint32_t i = 0; i < 32; ++i)
    {
        uint32_t bit = 1u << i;

        if ((layerMask & bit) != 0u)
        {
            m_states[static_cast<TarGetLayerType>(bit)].hitStopTimer = duration;
        }
    }
}

void TimeManager::PrepareCache()
{
    for (uint32_t i = 0; i < 32; ++i)
    {
        uint32_t bit = 1u << i;
        TarGetLayerType type = static_cast<TarGetLayerType>(bit);

        auto it = m_states.find(type);
        if (it != m_states.end())
        {
            m_cachedLayerScales[i] = it->second.currentScale;
        }
        else
        {
            m_cachedLayerScales[i] = 1.0f;
        }
    }
}

float TimeManager::GetCachedScale(uint32_t layerMask) const
{
    // Default = 0 ëŒçÙ
    if (layerMask == 0u)
    {
        return 1.0f;
    }

    float result = 1.0f;

    uint32_t mask = layerMask;

    while (mask != 0u)
    {
#if defined(_MSC_VER)
        unsigned long index = 0;
        if (!_BitScanForward(&index, mask))
        {
            break;
        }
#else
        uint32_t index = static_cast<uint32_t>(__builtin_ctz(mask));
#endif

        if (index < 32)
        {
            result *= m_cachedLayerScales[index];
        }

        mask &= ~(1u << index);
    }

    return result;
}