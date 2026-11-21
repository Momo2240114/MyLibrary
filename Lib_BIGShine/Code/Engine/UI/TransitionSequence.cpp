#include "PrecompiledHeader.h"
#include "TransitionSequence.h"
#include "Engine/Debug/TracyProfiler.h"
#include <algorithm>
#include <cmath>

float TransitionSequence::Evaluate(float ratio_, TransitionEasing easing_)
{
    const float t_ = std::clamp(ratio_, 0.0f, 1.0f);
    switch (easing_)
    {
    case TransitionEasing::EASE_IN_CUBIC: return t_ * t_ * t_;
    case TransitionEasing::EASE_OUT_CUBIC: return 1.0f - std::pow(1.0f - t_, 3.0f);
    case TransitionEasing::EASE_IN_OUT_CUBIC: return t_ < 0.5f ? 4.0f * t_ * t_ * t_ : 1.0f - std::pow(-2.0f * t_ + 2.0f, 3.0f) * 0.5f;
    case TransitionEasing::EASE_OUT_EXPO: return t_ >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t_);
    case TransitionEasing::EASE_OUT_BACK: return 1.0f + 2.70158f * std::pow(t_ - 1.0f, 3.0f) + 1.70158f * std::pow(t_ - 1.0f, 2.0f);
    default: return t_;
    }
}


// 開始時に設定を固定し、実行中のInspector編集による不連続を防ぐ。
void TransitionSequence::Start(const TransitionParameters& parameters_, TransitionPlayback mode_)
{
    parameters = parameters_;
    elapsedTime = effectTime = 0;
    revealStart = 1;
    frozen = false;
    coveredRendered = coveredDispatched = completedPending = false;
    automaticReveal = mode_ == TransitionPlayback::PLAY;
    waitingForWork = mode_ == TransitionPlayback::COVER;
    state = mode_ == TransitionPlayback::REVEAL ? TransitionState::TRANSITION_IN : TransitionState::TRANSITION_OUT;
    progress = mode_ == TransitionPlayback::REVEAL ? 1.0f : 0.0f;
}

void TransitionSequence::Update(float rawDeltaTime_)
{
    BS_PROFILE_SCOPE("TransitionSequence::Update");
    if (!IsActive() || frozen)
    {
        return;
    }
    const float step_ = std::isfinite(rawDeltaTime_) ? (std::max)(0.0f, rawDeltaTime_) * parameters.playbackSpeed : 0;
    elapsedTime += step_;
    effectTime += step_;
    if (state == TransitionState::TRANSITION_OUT)
    {
        progress = parameters.durationOut > 0 ? std::clamp((elapsedTime - parameters.delay) / parameters.durationOut,0.0f,1.0f) : (elapsedTime >= parameters.delay ? 1.0f : 0.0f);
        if (progress >= 1)
        {
            state = TransitionState::COVERED;
            elapsedTime = 0;
        }
    }
    else if ((state == TransitionState::COVERED || state == TransitionState::WAITING_FOR_SCENE) &&
        coveredDispatched && automaticReveal && !waitingForWork && elapsedTime >= parameters.holdDuration)
    {
        Reveal();
    }
    else if (state == TransitionState::TRANSITION_IN)
    {
        progress = parameters.durationIn > 0 ? revealStart * (1 - std::clamp(elapsedTime / (std::max)(parameters.durationIn * revealStart,0.001f),0.0f,1.0f)) : 0;
        if (progress <= 0)
        {
            state = TransitionState::IDLE;
            completedPending = true;
        }
    }
}

void TransitionSequence::NotifyRendered()
{
    if (progress >= 1)
    {
        coveredRendered = true;
    }
}

bool TransitionSequence::TakeCovered()
{
    if (!coveredRendered || coveredDispatched || state == TransitionState::TRANSITION_IN)
    {
        return false;
    }
    coveredDispatched = true;
    elapsedTime = 0;
    return true;
}

bool TransitionSequence::TakeCompleted()
{
    const bool pending_ = completedPending;
    completedPending = false;
    return pending_;
}

void TransitionSequence::Hold()
{
    if (IsCovered())
    {
        waitingForWork = true;
        state = TransitionState::WAITING_FOR_SCENE;
    }
}

void TransitionSequence::Reveal()
{
    if (IsActive() && progress > 0)
    {
        revealStart = progress;
        state = TransitionState::TRANSITION_IN;
        elapsedTime = 0;
        frozen = false;
    }
}

void TransitionSequence::CompleteWork()
{
    waitingForWork = false;
    automaticReveal = true;
}

// CLEARは透明へ戻し、KEEP_CURRENTは進行を凍結、KEEP_COVEREDは被覆を維持する。
void TransitionSequence::Cancel(TransitionCancelMode mode_)
{
    completedPending = false;
    coveredDispatched = true;
    waitingForWork = true;
    automaticReveal = false;
    if (mode_ == TransitionCancelMode::CLEAR)
    {
        state = TransitionState::IDLE;
        progress = 0;
        frozen = false;
    }
    else if (mode_ == TransitionCancelMode::KEEP_COVERED)
    {
        progress = 1;
        coveredRendered = false;
        state = TransitionState::COVERED;
        frozen = true;
    }
    else
    {
        frozen = true;
    }
}
