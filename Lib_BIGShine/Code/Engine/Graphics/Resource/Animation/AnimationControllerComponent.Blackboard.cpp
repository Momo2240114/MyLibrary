#include "PrecompiledHeader.h"
#include "AnimationControllerComponent.h"
#include "AnimationGraphBlackboardKeys.h"
#include "AnimationGraphBridgeComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Keys = AnimationGraphBlackboardKeys;

void AnimationControllerComponent::PublishGraphFeedback()
{
    const auto owner_ = GetGameObject();
    const auto board_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!board_) return;
    if (!owner_->GetComponent<AnimationGraphBridgeComponent>() && board_->GetCategory(Keys::REQUEST).empty()) return;
    board_->SetString(Keys::FEEDBACK, Keys::CURRENT_STATE, currentState ? currentState->name : "");
    board_->SetFloat(Keys::FEEDBACK, Keys::PLAYBACK_TIME, GetAnimatorTime());
    board_->SetFloat(Keys::FEEDBACK, Keys::PLAYBACK_DURATION, GetCurrentSampleDuration());
    board_->SetBool(Keys::FEEDBACK, Keys::SKILL_OWNED, skillControlled);
}

void AnimationControllerComponent::ProcessBlackboardGraphRequests(bool transitions_)
{
    BS_PROFILE_SCOPE("AnimationControllerComponent::ProcessBlackboardGraphRequests");
    const auto owner_ = GetGameObject();
    const auto board_ = owner_ ? owner_->GetComponent<BlackboardComponent>() : nullptr;
    if (!board_) return;
    if (!owner_->GetComponent<AnimationGraphBridgeComponent>() && board_->GetCategory(Keys::REQUEST).empty()) return;
    // Event処理中の中断要求も、そのフレームのFlushより先に予約を破棄する。
    // 予兆停止は遷移を評価しないため、再帰防止中でも安全に受け付けられる。
    bool cancelTelegraphs_ = false;
    if (board_->GetBool(Keys::REQUEST, Keys::TELEGRAPH_CANCEL_PENDING, cancelTelegraphs_) && cancelTelegraphs_)
    {
        board_->SetBool(Keys::REQUEST, Keys::TELEGRAPH_CANCEL_PENDING, false);
        animator.CancelAttackTelegraphs();
    }
    if (processingBlackboardGraphRequests) return;
    blackboardComponent = board_.get();
    // StateExitのActionが次の要求を発行しても、遷移評価を再帰呼び出ししない。
    struct ProcessingGuard
    {
        bool& processing;
        explicit ProcessingGuard(bool& processing_) : processing(processing_) { processing = true; }
        ~ProcessingGuard() { processing = false; }
    } guard_(processingBlackboardGraphRequests);
    const auto consume_ = [&](const char* key_)
    {
        bool pending_ = false;
        board_->GetBool(Keys::REQUEST, key_, pending_);
        if (pending_) board_->SetBool(Keys::REQUEST, key_, false);
        return pending_;
    };
    if (consume_(Keys::INPUT_CATEGORY_PENDING))
    {
        std::string category_;
        if (board_->GetString(Keys::REQUEST, Keys::INPUT_CATEGORY, category_) && !category_.empty())
            SetBlackboardCategory(category_);
    }
    if (consume_(Keys::ASSET_PENDING))
    {
        std::string state_, path_;
        board_->GetString(Keys::REQUEST, Keys::ASSET_STATE, state_);
        board_->GetString(Keys::REQUEST, Keys::ASSET_PATH, path_);
        board_->SetBool(Keys::REQUEST, Keys::ASSET_LOADED, LoadMissingStateAsset(state_, path_));
    }
    if (consume_(Keys::ROOT_PENDING))
    {
        std::string state_;
        float start_ = 0.0f, end_ = 0.0f, duration_ = 0.0f;
        DirectX::XMFLOAT3 delta_{};
        board_->GetString(Keys::REQUEST, Keys::ROOT_STATE, state_);
        board_->GetFloat(Keys::REQUEST, Keys::ROOT_START, start_);
        board_->GetFloat(Keys::REQUEST, Keys::ROOT_END, end_);
        const bool success_ = EstimateRootMotion(state_, start_, end_, delta_, duration_);
        board_->SetBool(Keys::REQUEST, Keys::ROOT_SUCCESS, success_);
        board_->SetVector3(Keys::REQUEST, Keys::ROOT_DELTA, { delta_.x, delta_.y, delta_.z });
        board_->SetFloat(Keys::REQUEST, Keys::ROOT_DURATION, duration_);
    }
    if (consume_(Keys::YAW_PENDING))
    {
        bool enabled_ = false;
        float degrees_ = 0.0f;
        board_->GetBool(Keys::REQUEST, Keys::YAW_ENABLED, enabled_);
        board_->GetFloat(Keys::REQUEST, Keys::YAW_LIMIT, degrees_);
        if (enabled_ && std::isfinite(degrees_)) SetRootMotionYawLimit(degrees_);
        else ClearRootMotionYawLimit();
    }
    const bool stateRequest_ = transitions_ && consume_(Keys::STATE_PENDING);
    if (stateRequest_)
    {
        std::string state_, category_ = "Anim";
        float blend_ = 0.2f, startTime_ = 0.0f;
        bool restart_ = false, allowSkill_ = false;
        board_->GetString(Keys::REQUEST, Keys::STATE, state_);
        board_->GetString(Keys::REQUEST, Keys::CATEGORY, category_);
        board_->GetFloat(Keys::REQUEST, Keys::BLEND, blend_);
        board_->GetFloat(Keys::REQUEST, Keys::START_TIME, startTime_);
        board_->GetBool(Keys::REQUEST, Keys::RESTART, restart_);
        board_->GetBool(Keys::REQUEST, Keys::ALLOW_SKILL, allowSkill_);
        board_->SetBool(Keys::REQUEST, Keys::ACCEPTED, false);
        if (!state_.empty() && states.count(state_) && states.at(state_) &&
            std::isfinite(blend_) && std::isfinite(startTime_) && !presentationControlled && (!skillControlled || allowSkill_))
        {
            useTransitionGraph = true;
            EnsureSpecialGraphNodes();
            EnsureGraphNodeForState(state_);
            const std::string trigger_ = std::string(Keys::TRANSITION_PREFIX) + state_;
            auto route_ = std::find_if(graphTransitions.begin(), graphTransitions.end(), [&](const AnimTransition& transition_)
            { return transition_.blackboardRequestRoute && transition_.toStateName == state_; });
            if (route_ == graphTransitions.end())
            {
                // 古いグラフに要求遷移がなければ追加し、Editorから条件を調整可能にする。
                AnimTransition transition_;
                transition_.id = ed::LinkId(GetNextId());
                transition_.fromStateName = "__Any__";
                transition_.toStateName = state_;
                transition_.scopeId = "Root";
                transition_.blackboardRequestRoute = true;
                transition_.conditions.push_back({ ConditionType::Trigger, trigger_, ConditionOp::Equal, 1.0f });
                graphTransitions.push_back(transition_);
                route_ = std::prev(graphTransitions.end());
            }
            const std::string previousCategory_ = animationBlackboardCategory;
            SetBlackboardCategory(category_);
            board_->SetTrigger(category_, trigger_);
            const bool fromMatches_ = route_->fromStateName == "__Any__" ||
                (currentState && route_->fromStateName == currentState->name);
            bool accepted_ = fromMatches_ && route_->conditions.empty();
            if (fromMatches_ && !route_->conditions.empty())
            {
                accepted_ = route_->logic == TransitionLogic::And;
                for (const auto& condition_ : route_->conditions)
                {
                    const bool met_ = EvaluateCondition(condition_);
                    if (route_->logic == TransitionLogic::And && !met_) { accepted_ = false; break; }
                    if (route_->logic == TransitionLogic::Or && met_) { accepted_ = true; break; }
                }
            }
            // 消費した要求を通常評価へ残さず、同じStateの再起動も一度だけ処理する。
            if (accepted_)
                for (const auto& condition_ : route_->conditions)
                    if (condition_.type == ConditionType::Trigger) board_->ResetTrigger(category_, condition_.parameterName);
            board_->ResetTrigger(category_, trigger_);
            if (accepted_) ChangeState(state_, (std::max)(0.0f, blend_), restart_, (std::max)(0.0f, startTime_));
            SetBlackboardCategory(previousCategory_);
            board_->SetBool(Keys::REQUEST, Keys::ACCEPTED, accepted_ && currentState && currentState->name == state_);
        }
    }
    bool evaluationPending_ = false, evaluationImmediate_ = false;
    board_->GetBool(Keys::REQUEST, Keys::EVALUATE_PENDING, evaluationPending_);
    board_->GetBool(Keys::REQUEST, Keys::EVALUATE_IMMEDIATE, evaluationImmediate_);
    if (transitions_ && !stateRequest_ && evaluationPending_ && !presentationControlled && !skillControlled &&
        (evaluationImmediate_ || (!animationPaused && !editorPoseOverride)))
    {
        board_->SetBool(Keys::REQUEST, Keys::EVALUATE_PENDING, false);
        std::string category_ = "Anim";
        board_->GetString(Keys::REQUEST, Keys::CATEGORY, category_);
        const std::string previousCategory_ = animationBlackboardCategory;
        SetBlackboardCategory(category_);
        useTransitionGraph = true;
        EvaluateGraphTransitions();
        SetBlackboardCategory(previousCategory_);
    }
    MarkBlackboardGuiCacheDirty();
    PublishGraphFeedback();
}
