#pragma once
#include "TransitionParameters.h"
enum class TransitionPlayback { COVER, REVEAL, PLAY };
enum class TransitionCancelMode { CLEAR, KEEP_CURRENT, KEEP_COVERED };

// 描画やSceneを所有せず、Cover・待機・Revealと通知の状態だけを進める。
class TransitionSequence
{
public:
    void Start(const TransitionParameters& parameters_, TransitionPlayback mode_);
    void Update(float rawDeltaTime_);
    // 完全被覆を実際に描画した後でのみ、通知を取り出せる。
    void NotifyRendered();
    bool TakeCovered();
    bool TakeCompleted();
    void Hold();
    void Reveal();
    void CompleteWork();
    void Cancel(TransitionCancelMode mode_ = TransitionCancelMode::CLEAR);

    TransitionState GetState() const
    {
        return state;
    }

    float GetProgress() const
    {
        return progress;
    }

    float GetEffectTime() const
    {
        return effectTime;
    }

    bool IsCovered() const
    {
        return progress >= 1 && coveredRendered && state != TransitionState::TRANSITION_IN;
    }

    bool IsActive() const
    {
        return state != TransitionState::IDLE;
    }

    static float Evaluate(float ratio_, TransitionEasing easing_);

private:
    TransitionParameters parameters;
    TransitionState state = TransitionState::IDLE;
    float progress = 0;
    float elapsedTime = 0;
    float effectTime = 0;
    float revealStart = 1;
    bool coveredRendered = false;
    bool coveredDispatched = false;
    bool completedPending = false;
    bool automaticReveal = false;
    bool waitingForWork = false;
    bool frozen = false;
};
