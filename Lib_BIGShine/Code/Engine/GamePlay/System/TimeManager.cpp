#include "PrecompiledHeader.h"

#include "TimeManager.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

TimeManager::TimeManager()
{
    // Cache配列は全Layer分を1.0で初期化し、未設定Layerが通常速度で動くようにする。
    for (float& scale : cachedLayerScales)
    {
        scale = 1.0f;
    }
}

void TimeManager::Update(float rawDt_)
{
    // rawDtは実時間として保存する。
    // Pause中はsimulationRawDtを0にし、FrameStepが要求されたFrameだけ通常のdtを通す。
    lastRawDeltaTime = (std::max)(rawDt_, 0.0f);
    frameStepActive = paused && frameStepRequested;
    frameStepRequested = false;

    const float simulationRawDt_ = (paused && !frameStepActive) ? 0.0f : lastRawDeltaTime;

    defaultState.Update(simulationRawDt_);

    // Layer別Scaleも同じsimulationRawDtで進めるため、Pause中はHitStopやTimedScaleも止まる。
    // ただしFrameStep時だけ1Frame分進み、Debug停止中のコマ送りができる。
    for (auto& [type, state] : states)
    {
        state.Update(simulationRawDt_);
    }
}

void TimeManager::SetPaused(bool paused_)
{
    // Pause解除時は保留中のFrameStep状態も消し、通常更新へ戻す。
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
    // コマ送りはPause状態を維持したまま、次のUpdateだけsimulationRawDtを通す。
    paused = true;
    frameStepRequested = true;
}

void TimeManager::SetTargetScale(uint32_t layerMask_, float scale_, float speed_)
{
    // layerMask 0はDefaultだけ、AllはDefaultと全指定Layerの両方へ設定する。
    // 個別LayerはbitごとにTimeStateを作り、GameObjectのLayerBitから参照される。
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.targetScale = scale_;
        defaultState.lerpSpeed = speed_;
        defaultState.phase = TimeState::PHASE::IDLE;
        if (layerMask_ == 0u)
        {
            return;
        }
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
    // HitStopは指定LayerのTimeStateへTimerと固定Scaleを直接設定する。
    // AttackSystemから呼ばれ、Hitした瞬間だけ対象Layerの動きを止める。
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.hitStopTimer = duration_;
        defaultState.hitStopScale = (std::clamp)(scale_, 0.0f, 1.0f);
        if (layerMask_ == 0u)
        {
            return;
        }
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
    // GameObjectManagerはObjectごとに何度もScaleを読むため、Update後に配列へ確定しておく。
    // Pause中は全ScaleにpauseScale=0を掛け、FrameStep時だけ1に戻す。
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
    // Default = 0は全体Scaleとして扱う。
    if (layerMask_ == 0u)
    {
        return cachedDefaultScale;
    }

    float result_ = 1.0f;

    uint32_t mask_ = layerMask_;

    while (mask_ != 0u)
    {
        // 複数Layer bitが立っている場合は、該当Scaleを掛け合わせる。
        // 例えば全体Slow + 個別HitStopのような重ね掛けができる。
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
    // 演出用TimeScaleを指定Layerへ流す。
    // SetTargetScaleと違い、到達、保持、復帰までをTimeState側で自動進行する。
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.Play(targetScale_, transitionDuration_, holdDuration_, returnDuration_, defaultScale_);
        if (layerMask_ == 0u)
        {
            return;
        }
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
    // 指定LayerのScale状態をdefaultへ戻す。
    // 演出終了やScene切り替え時に、HitStop/TimedScaleの残り状態を消すために使う。
    if (layerMask_ == 0u || layerMask_ == static_cast<uint32_t>(TarGetLayerType::All))
    {
        defaultState.Reset(resetConfiguredDefaults_);
        if (layerMask_ == 0u)
        {
            return;
        }
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
