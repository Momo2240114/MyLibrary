#include "PrecompiledHeader.h"
#include "AnimationGraphBridgeComponent.h"
#include "AnimationGraphBlackboardKeys.h"
#include "AnimationControllerComponent.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

namespace Keys = AnimationGraphBlackboardKeys;

const std::string& AnimationGraphBridgeComponent::GetName() const
{
    static const std::string NAME = "AnimationGraphBridgeComponent";
    return NAME;
}

std::shared_ptr<AnimationGraphBridgeComponent> AnimationGraphBridgeComponent::Acquire(const std::shared_ptr<GameObject>& owner_)
{
    if (!owner_) return nullptr;
    if (const auto bridge_ = owner_->GetComponent<AnimationGraphBridgeComponent>())
        return bridge_->ResolveController() ? bridge_ : nullptr;
    if (!owner_->GetComponent<AnimationControllerComponent>()) return nullptr;
    const auto bridge_ = owner_->GetOrAddComponent<AnimationGraphBridgeComponent>(false);
    return bridge_->ResolveController() ? bridge_ : nullptr;
}

std::shared_ptr<BlackboardComponent> AnimationGraphBridgeComponent::ResolveBoard()
{
    if (const auto board_ = board.lock()) return board_;
    const auto owner_ = GetGameObject();
    if (!owner_) return nullptr;
    const auto board_ = owner_->GetOrAddComponent<BlackboardComponent>();
    board = board_;
    return board_;
}

std::shared_ptr<AnimationControllerComponent> AnimationGraphBridgeComponent::ResolveController()
{
    if (const auto controller_ = controller.lock()) return controller_;
    const auto owner_ = GetGameObject();
    const auto controller_ = owner_ ? owner_->GetComponent<AnimationControllerComponent>() : nullptr;
    if (!controller_) return nullptr;
    controller = controller_;
    if (const auto board_ = ResolveBoard())
    {
        // 保存された途中の要求や別のControllerの結果を再生し直さない。
        board_->ClearCategory(Keys::REQUEST);
        board_->ClearCategory(Keys::FEEDBACK);
    }
    return controller_;
}

void AnimationGraphBridgeComponent::ProcessRequests(bool transitions_)
{
    if (const auto controller_ = ResolveController()) controller_->ProcessBlackboardGraphRequests(transitions_);
}

void AnimationGraphBridgeComponent::RefreshFeedback()
{
    if (const auto controller_ = ResolveController()) controller_->PublishGraphFeedback();
}

bool AnimationGraphBridgeComponent::RequestStateTransition(const std::string& state_, float blend_, bool restart_,
    float startTime_, bool allowSkill_, const std::string& category_)
{
    if (!ResolveController()) return false;
    BS_PROFILE_SCOPE("AnimationGraphBridgeComponent::RequestStateTransition");
    const auto board_ = ResolveBoard();
    if (!board_ || state_.empty()) return false;
    board_->SetString(Keys::REQUEST, Keys::CATEGORY, category_);
    board_->SetString(Keys::REQUEST, Keys::STATE, state_);
    board_->SetFloat(Keys::REQUEST, Keys::BLEND, blend_);
    board_->SetFloat(Keys::REQUEST, Keys::START_TIME, startTime_);
    board_->SetBool(Keys::REQUEST, Keys::RESTART, restart_);
    board_->SetBool(Keys::REQUEST, Keys::ALLOW_SKILL, allowSkill_);
    board_->SetBool(Keys::REQUEST, Keys::ACCEPTED, false);
    board_->SetBool(Keys::REQUEST, Keys::STATE_PENDING, true);
    ProcessRequests(true);
    bool accepted_ = false;
    board_->GetBool(Keys::REQUEST, Keys::ACCEPTED, accepted_);
    return accepted_;
}

void AnimationGraphBridgeComponent::RequestTrigger(const std::string& trigger_, const std::string& category_)
{
    if (!ResolveController()) return;
    if (const auto board_ = ResolveBoard())
    {
        // 通常Triggerは次のAnimation更新で評価し、AI更新の途中で再生順を変えない。
        board_->SetTrigger(category_, trigger_);
        board_->SetString(Keys::REQUEST, Keys::CATEGORY, category_);
        board_->SetBool(Keys::REQUEST, Keys::EVALUATE_IMMEDIATE, false);
        board_->SetBool(Keys::REQUEST, Keys::EVALUATE_PENDING, true);
    }
}

void AnimationGraphBridgeComponent::ClearTrigger(const std::string& trigger_, const std::string& category_)
{
    if (!ResolveController()) return;
    if (const auto board_ = ResolveBoard()) board_->ResetTrigger(category_, trigger_);
}

void AnimationGraphBridgeComponent::RequestEvaluation(const std::string& category_)
{
    if (!ResolveController()) return;
    if (const auto board_ = ResolveBoard())
    {
        board_->SetString(Keys::REQUEST, Keys::CATEGORY, category_);
        board_->SetBool(Keys::REQUEST, Keys::EVALUATE_IMMEDIATE, true);
        board_->SetBool(Keys::REQUEST, Keys::EVALUATE_PENDING, true);
        ProcessRequests(true);
    }
}

void AnimationGraphBridgeComponent::RequestInputCategory(const std::string& category_)
{
    if (!ResolveController() || category_.empty()) return;
    if (const auto board_ = ResolveBoard())
    {
        board_->SetString(Keys::REQUEST, Keys::INPUT_CATEGORY, category_);
        board_->SetBool(Keys::REQUEST, Keys::INPUT_CATEGORY_PENDING, true);
        ProcessRequests();
    }
}

void AnimationGraphBridgeComponent::RequestYawLimit(float degrees_)
{
    if (!ResolveController()) return;
    if (const auto board_ = ResolveBoard())
    {
        board_->SetFloat(Keys::REQUEST, Keys::YAW_LIMIT, degrees_);
        board_->SetBool(Keys::REQUEST, Keys::YAW_ENABLED, true);
        board_->SetBool(Keys::REQUEST, Keys::YAW_PENDING, true);
        ProcessRequests();
    }
}

void AnimationGraphBridgeComponent::ClearYawLimit()
{
    if (!ResolveController()) return;
    if (const auto board_ = ResolveBoard())
    {
        board_->SetBool(Keys::REQUEST, Keys::YAW_ENABLED, false);
        board_->SetBool(Keys::REQUEST, Keys::YAW_PENDING, true);
        ProcessRequests();
    }
}

bool AnimationGraphBridgeComponent::LoadMissingStateAsset(const std::string& state_, const std::string& path_)
{
    if (!ResolveController()) return false;
    const auto board_ = ResolveBoard();
    if (!board_) return false;
    board_->SetString(Keys::REQUEST, Keys::ASSET_STATE, state_);
    board_->SetString(Keys::REQUEST, Keys::ASSET_PATH, path_);
    board_->SetBool(Keys::REQUEST, Keys::ASSET_LOADED, false);
    board_->SetBool(Keys::REQUEST, Keys::ASSET_PENDING, true);
    ProcessRequests();
    bool loaded_ = false;
    board_->GetBool(Keys::REQUEST, Keys::ASSET_LOADED, loaded_);
    return loaded_;
}

bool AnimationGraphBridgeComponent::EstimateRootMotion(const std::string& state_, float start_, float end_,
    DirectX::XMFLOAT3& delta_, float& duration_)
{
    if (!ResolveController()) return false;
    BS_PROFILE_SCOPE("AnimationGraphBridgeComponent::EstimateRootMotion");
    const auto board_ = ResolveBoard();
    if (!board_) return false;
    board_->SetString(Keys::REQUEST, Keys::ROOT_STATE, state_);
    board_->SetFloat(Keys::REQUEST, Keys::ROOT_START, start_);
    board_->SetFloat(Keys::REQUEST, Keys::ROOT_END, end_);
    board_->SetBool(Keys::REQUEST, Keys::ROOT_SUCCESS, false);
    board_->SetBool(Keys::REQUEST, Keys::ROOT_PENDING, true);
    ProcessRequests();
    bool success_ = false;
    board_->GetBool(Keys::REQUEST, Keys::ROOT_SUCCESS, success_);
    if (!success_) return false;
    DirectX::SimpleMath::Vector3 result_{};
    board_->GetVector3(Keys::REQUEST, Keys::ROOT_DELTA, result_);
    board_->GetFloat(Keys::REQUEST, Keys::ROOT_DURATION, duration_);
    delta_ = { result_.x, result_.y, result_.z };
    return true;
}

const std::unordered_map<std::string, std::unique_ptr<AnimState>>& AnimationGraphBridgeComponent::GetStateCatalog()
{
    static const std::unordered_map<std::string, std::unique_ptr<AnimState>> EMPTY;
    const auto controller_ = ResolveController();
    return controller_ ? controller_->GetStates() : EMPTY;
}

const AnimState* AnimationGraphBridgeComponent::GetActiveState()
{
    RefreshFeedback();
    const auto board_ = ResolveBoard();
    std::string state_;
    if (!board_ || !board_->GetString(Keys::FEEDBACK, Keys::CURRENT_STATE, state_)) return nullptr;
    const auto& catalog_ = GetStateCatalog();
    const auto iterator_ = catalog_.find(state_);
    return iterator_ != catalog_.end() ? iterator_->second.get() : nullptr;
}

float AnimationGraphBridgeComponent::GetPlaybackTime()
{
    RefreshFeedback();
    float time_ = 0.0f;
    if (const auto board_ = ResolveBoard()) board_->GetFloat(Keys::FEEDBACK, Keys::PLAYBACK_TIME, time_);
    return time_;
}

float AnimationGraphBridgeComponent::GetPlaybackDuration()
{
    RefreshFeedback();
    float duration_ = 0.0f;
    if (const auto board_ = ResolveBoard()) board_->GetFloat(Keys::FEEDBACK, Keys::PLAYBACK_DURATION, duration_);
    return duration_;
}

bool AnimationGraphBridgeComponent::IsSkillOwned()
{
    RefreshFeedback();
    bool controlled_ = false;
    if (const auto board_ = ResolveBoard()) board_->GetBool(Keys::FEEDBACK, Keys::SKILL_OWNED, controlled_);
    return controlled_;
}
