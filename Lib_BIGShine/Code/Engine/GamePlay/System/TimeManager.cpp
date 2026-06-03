#include "TimeManager.h"
#include <bit>

void TimeManager::Update(float rawDt)
{
    for (auto& [type, state] : m_states) 
    {
        state.Update(rawDt);
    }
}


void TimeManager::SetTargetScale(uint32_t layerMask, float scale, float speed)
{
    for (int i = 0; i < 31; ++i) { // 全レイヤービットを走査
        if (layerMask & (1 << i)) {
            auto& state = m_states[static_cast<TarGetLayerType>(1 << i)];
            state.targetScale = scale;
            state.lerpSpeed = speed;
        }
    }
}

void TimeManager::TriggerHitStop(uint32_t layerMask, float duration)
{
    for (int i = 0; i < 31; ++i) {
        if (layerMask & (1 << i)) {
            m_states[static_cast<TarGetLayerType>(1 << i)].hitStopTimer = duration;
        }
    }
}

void TimeManager::PrepareCache()
{
    for (int i = 0; i < 32; ++i) {
        TarGetLayerType type = static_cast<TarGetLayerType>(1 << i);
        if (m_states.count(type)) {
            m_cachedLayerScales[i] = m_states[type].currentScale;
        }
        else {
            m_cachedLayerScales[i] = 1.0f; // 未設定なら等倍
        }
    }
}

// インデックス変換とキャッシュ取得
float TimeManager::GetCachedScale(uint32_t layer) const {
    // ビット位置を計算

    uint32_t mask = layer;

#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, mask);
    return m_cachedLayerScales[index];
#else
    int index = __builtin_ctz(mask);
    return m_cachedLayerScales[index];
#endif
}