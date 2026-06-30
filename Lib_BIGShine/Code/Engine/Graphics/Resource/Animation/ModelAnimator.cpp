#include "ModelAnimator.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Combat/AttackTemplateRuntime.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include <algorithm>
#include <cmath>



static bool CallLocalAnimationAction(const std::shared_ptr<GameObject>& owner_, const std::string& actionName_)
{
    if (!owner_ || actionName_.empty()) return false;

    if (owner_->localRegistry.HasAction(actionName_)) {
        owner_->localRegistry.Call(actionName_);
        return true;
    }

    return false;
}

static std::shared_ptr<GameObject> GetParentObject(const std::shared_ptr<GameObject>& owner_)
{
    if (!owner_ || !owner_->transform) return nullptr;

    auto parentTransform_ = owner_->transform->parent.lock();
    return parentTransform_ ? parentTransform_->gameObject.lock() : nullptr;
}

static bool CallAncestorAnimationAction(const std::shared_ptr<GameObject>& owner_, const std::string& actionName_)
{
    auto parent_ = GetParentObject(owner_);
    while (parent_)
    {
        if (CallLocalAnimationAction(parent_, actionName_))
        {
            return true;
        }

        parent_ = GetParentObject(parent_);
    }

    return false;
}

static bool CallChildAnimationAction(const std::shared_ptr<GameObject>& owner_, const std::string& actionName_)
{
    if (!owner_ || !owner_->transform) return false;

    for (const auto& childTransform_ : owner_->transform->children)
    {
        if (!childTransform_) continue;

        auto child_ = childTransform_->gameObject.lock();
        if (CallLocalAnimationAction(child_, actionName_) ||
            CallChildAnimationAction(child_, actionName_))
        {
            return true;
        }
    }

    return false;
}

static void SpawnAnimationMeshVFXEvent(
    const std::shared_ptr<GameObject>& owner_,
    const AnimEvent& event_)
{
    if (!owner_ || event_.meshVFXAssetPath.empty()) return;

    AttackEffectSpawnRequest request_{};
    request_.ownerObject = owner_.get();
    request_.setting.vfxPath = event_.meshVFXAssetPath;
    request_.setting.lifeTime = event_.meshVFXLifeTime;
    request_.setting.localOffset = event_.localPosition;
    request_.setting.localRotation = event_.localRotation;
    request_.setting.localScale = event_.localScale;
    request_.setting.playbackSpeed = event_.meshVFXPlaybackSpeed;
    request_.setting.followOwner = event_.attachToOwner || !event_.attachNodeName.empty();
    request_.setting.attachNodeName = event_.attachNodeName;
    request_.setting.mirror = event_.mirrorMeshVFX;
    owner_->GetManager().GetEffectManager().SpawnSlashEffect(request_);
}
static void CallAnimationAction(const std::shared_ptr<GameObject>& owner_, const std::string& actionName_)
{
    if (actionName_.empty()) return;

    if (CallLocalAnimationAction(owner_, actionName_) ||
        CallAncestorAnimationAction(owner_, actionName_) ||
        CallChildAnimationAction(owner_, actionName_))
    {
        return;
    }

    ActionRegistry::Instance().Call(actionName_);
}

static void ApplyAnimationBlackboardWrites(
    const std::shared_ptr<GameObject>& owner_,
    const std::vector<AnimBlackboardWrite>& writes_)
{
    if (!owner_ || writes_.empty()) return;

    const std::shared_ptr<BlackboardComponent> blackboard_ =
        owner_->GetComponent<BlackboardComponent>();
    if (!blackboard_) return;

    for (const AnimBlackboardWrite& write_ : writes_)
    {
        if (write_.category.empty() || write_.key.empty()) continue;

        switch (write_.type)
        {
        case ANIM_BLACKBOARD_VALUE_TYPE::FLOAT:
            blackboard_->SetFloat(write_.category, write_.key, write_.floatValue);
            break;
        case ANIM_BLACKBOARD_VALUE_TYPE::INT:
            blackboard_->SetInt(write_.category, write_.key, write_.intValue);
            break;
        case ANIM_BLACKBOARD_VALUE_TYPE::BOOL:
            blackboard_->SetBool(write_.category, write_.key, write_.boolValue);
            break;
        case ANIM_BLACKBOARD_VALUE_TYPE::STRING:
            blackboard_->SetString(write_.category, write_.key, write_.stringValue);
            break;
        }
    }
}

static void CallStateExitEndActions(ModelAnimator::ActiveSlot& slot_, const std::shared_ptr<GameObject>& owner_)
{
    if (!slot_.state) return;

    for (auto& ev_ : const_cast<AnimState*>(slot_.state)->events)
    {
        if (!ev_.callEndOnStateExit || !ev_.isFired || ev_.isEndFired)
        {
            continue;
        }

        if (ev_.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
        {
            AttackTemplateRuntime::End(owner_, ev_);
        }
        if (!ev_.endAction.empty())
        {
            CallAnimationAction(owner_, ev_.endAction);
        }
        ev_.isEndFired = true;
    }
}

static float ApplyPoseInterpolation(int interpolation, float t)
{
    t = (std::max)(0.0f, (std::min)(1.0f, t));

    switch (interpolation)
    {
    case 0: return t;
    case 2: return t * t;
    case 3: return 1.0f - (1.0f - t) * (1.0f - t);
    case 4: return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
    case 5: return 0.0f;
    case 1:
    default:
        return t * t * (3.0f - 2.0f * t);
    }
}

static bool MatchesNaturalMotionNode(const std::string& filter, const std::string& nodeName)
{
    return filter.empty() || nodeName.find(filter) != std::string::npos;
}


static float ResolveBlendWeight(
    const AnimState* state_,
    const BlackboardCategory* blackboard_)
{
    if (!state_) return 0.0f;

    if (!blackboard_ || state_->blendParamName.empty())
    {
        return (std::clamp)(state_->blendDefaultWeight, 0.0f, 1.0f);
    }

    const auto valueIt_ = blackboard_->find(state_->blendParamName);
    if (valueIt_ == blackboard_->end())
    {
        return (std::clamp)(state_->blendDefaultWeight, 0.0f, 1.0f);
    }

    const float range_ = state_->blendParameterMax - state_->blendParameterMin;
    if (std::abs(range_) <= 0.000001f)
    {
        return (std::clamp)(state_->blendDefaultWeight, 0.0f, 1.0f);
    }

    const float rawValue_ = BlackboardComponent::ToFloat(valueIt_->second);
    return (std::clamp)(
        (rawValue_ - state_->blendParameterMin) / range_,
        0.0f,
        1.0f);
}
static bool IsValidAnimationIndex(
    const GltfModelCommponennt* model_,
    int animationIndex_)
{
    return model_ &&
        animationIndex_ >= 0 &&
        animationIndex_ < static_cast<int>(model_->GetAnimations().size());
}
static float ResolveBlendDuration(
    const AnimState* state_,
    GltfModelCommponennt* model_,
    const BlackboardCategory* blackboard_)
{
    if (!state_ || !model_) return 1.0f;

    const bool validA_ = IsValidAnimationIndex(model_, state_->animIndexA);
    const bool validB_ = IsValidAnimationIndex(model_, state_->animIndexB);
    if (!validA_ && !validB_) return 1.0f;

    const float durationA_ = validA_
        ? (std::max)(model_->GetAnimationDuration(state_->animIndexA), 0.0001f)
        : (std::max)(model_->GetAnimationDuration(state_->animIndexB), 0.0001f);
    if (!state_->blendSyncNormalizedTime || state_->blendUseMask || !validA_ || !validB_)
    {
        return durationA_;
    }

    const float durationB_ = (std::max)(model_->GetAnimationDuration(state_->animIndexB), 0.0001f);
    const float weight_ = ResolveBlendWeight(state_, blackboard_);
    return durationA_ + (durationB_ - durationA_) * weight_;
}

struct Blend2DSelection
{
    int animationIndexA = -1;
    int animationIndexB = -1;
    float phaseOffsetA = 0.0f;
    float phaseOffsetB = 0.0f;
    float weightB = 0.0f;
};

static Blend2DSelection ResolveBlend2DSelection(
    const AnimState* state_,
    const BlackboardCategory* blackboard_)
{
    Blend2DSelection selection_{};
    if (!state_) return selection_;

    DirectX::SimpleMath::Vector2 input_
    {
        state_->blend2DDefaultInput.x,
        state_->blend2DDefaultInput.y
    };
    if (blackboard_ && !state_->blend2DParamName.empty())
    {
        const auto valueIterator_ = blackboard_->find(state_->blend2DParamName);
        if (valueIterator_ != blackboard_->end())
        {
            if (const auto* vectorValue_ =
                std::get_if<DirectX::SimpleMath::Vector2>(&valueIterator_->second))
            {
                input_ = *vectorValue_;
            }
        }
    }

    input_.x = (std::clamp)(input_.x, -1.0f, 1.0f);
    input_.y = (std::clamp)(input_.y, -1.0f, 1.0f);
    float absoluteX_ = std::abs(input_.x);
    float absoluteY_ = std::abs(input_.y);
    if (absoluteX_ + absoluteY_ <= 0.000001f)
    {
        input_ = { state_->blend2DDefaultInput.x, state_->blend2DDefaultInput.y };
        absoluteX_ = std::abs(input_.x);
        absoluteY_ = std::abs(input_.y);
    }

    if (absoluteX_ > 0.000001f)
    {
        const bool useLeft_ = input_.x < 0.0f;
        selection_.animationIndexA = useLeft_
            ? state_->blend2DLeftAnimationIndex
            : state_->blend2DRightAnimationIndex;
        selection_.phaseOffsetA = useLeft_
            ? state_->blend2DLeftPhaseOffset
            : state_->blend2DRightPhaseOffset;
    }
    if (absoluteY_ > 0.000001f)
    {
        const bool useBackward_ = input_.y < 0.0f;
        selection_.animationIndexB = useBackward_
            ? state_->blend2DBackwardAnimationIndex
            : state_->blend2DForwardAnimationIndex;
        selection_.phaseOffsetB = useBackward_
            ? state_->blend2DBackwardPhaseOffset
            : state_->blend2DForwardPhaseOffset;
    }

    const float totalWeight_ = absoluteX_ + absoluteY_;
    selection_.weightB = totalWeight_ > 0.000001f
        ? absoluteY_ / totalWeight_
        : 1.0f;
    return selection_;
}

static float ResolveBlend2DDuration(
    const AnimState* state_,
    GltfModelCommponennt* model_,
    const BlackboardCategory* blackboard_)
{
    const Blend2DSelection selection_ = ResolveBlend2DSelection(state_, blackboard_);
    const bool validA_ = IsValidAnimationIndex(model_, selection_.animationIndexA);
    const bool validB_ = IsValidAnimationIndex(model_, selection_.animationIndexB);
    if (!validA_ && !validB_) return 1.0f;
    if (!validA_) return (std::max)(model_->GetAnimationDuration(selection_.animationIndexB), 0.0001f);
    if (!validB_) return (std::max)(model_->GetAnimationDuration(selection_.animationIndexA), 0.0001f);

    const float durationA_ = (std::max)(
        model_->GetAnimationDuration(selection_.animationIndexA), 0.0001f);
    if (!state_->blend2DSyncNormalizedTime) return durationA_;

    const float durationB_ = (std::max)(
        model_->GetAnimationDuration(selection_.animationIndexB), 0.0001f);
    return durationA_ + (durationB_ - durationA_) * selection_.weightB;
}

static float ResolveBlendSampleTime(
    float stateTime_,
    float stateDuration_,
    float animationDuration_,
    float phaseOffset_,
    bool syncNormalizedTime_,
    bool loop_)
{
    if (animationDuration_ <= 0.0f) return 0.0f;

    float sampleTime_ = stateTime_ + phaseOffset_ * animationDuration_;
    if (syncNormalizedTime_ && stateDuration_ > 0.0f)
    {
        sampleTime_ = (stateTime_ / stateDuration_ + phaseOffset_) * animationDuration_;
    }

    if (loop_)
    {
        sampleTime_ = std::fmod(sampleTime_, animationDuration_);
        if (sampleTime_ < 0.0f) sampleTime_ += animationDuration_;
        return sampleTime_;
    }

    return (std::clamp)(sampleTime_, 0.0f, animationDuration_);
}

static bool IsNodeInBlendMask(
    const AnimState* state_,
    const std::vector<GltfModelCommponennt::Node>& nodes_,
    size_t nodeIndex_)
{
    if (!state_ || !state_->blendUseMask) return true;
    if (state_->blendMaskRootNodes.empty()) return false;

    int currentIndex_ = static_cast<int>(nodeIndex_);
    while (currentIndex_ >= 0 && currentIndex_ < static_cast<int>(nodes_.size()))
    {
        const std::string& nodeName_ = nodes_[currentIndex_].name;
        if (std::find(
            state_->blendMaskRootNodes.begin(),
            state_->blendMaskRootNodes.end(),
            nodeName_) != state_->blendMaskRootNodes.end())
        {
            return true;
        }
        currentIndex_ = nodes_[currentIndex_].parentIndex;
    }

    return false;
}
void ModelAnimator::SetCurrentTime(float time_)
{
    current.time = (std::max)(0.0f, time_);
    previous = current;
    isTransitioning = false;
    isMixing = false;
    fadeTimer = 0.0f;

    // シークは連続再生ではないため、直前のRoot Motionとの差分を適用しない。
    currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    _isFirstFrame = true;
    _prevTime = current.time;
    _prevRootMotionState = nullptr;
    previousRootMotionNodeIndex = -1;
    hasPrevPose = false;
}

void ModelAnimator::Update(float et, GltfModelCommponennt* model, CurveComponent* curvecomp, const std::shared_ptr<PoseLibraryResource>& poseLib, std::shared_ptr<GameObject> owner, const BlackboardCategory* blackboard_)
{
    if (!current.state || !model) return;

    const size_t nodeCount = model->GetNodes().size();
    if (tempPoseA.size() != nodeCount) tempPoseA.resize(nodeCount);
    if (tempPoseB.size() != nodeCount) tempPoseB.resize(nodeCount);
    if (blendPoseScratch.size() != nodeCount) blendPoseScratch.resize(nodeCount);
    if (finalPose.size() != nodeCount) finalPose.resize(nodeCount);
    if (rootMotionLoopStartPose.size() != nodeCount) rootMotionLoopStartPose.resize(nodeCount);
    if (rootMotionLoopEndPose.size() != nodeCount) rootMotionLoopEndPose.resize(nodeCount);

    // ------------------------------------------------------------
    // 補助: 状態から pose をサンプルする
    // ------------------------------------------------------------
    auto SamplePose = [&](const AnimState* state, float time, float sampleDuration_, std::vector<GltfModelCommponennt::NodePose>& outPose)
        {
            if (!state) return;

            switch (state->type)
            {
            case AnimStateType::Custom:
                if (poseLib &&
                    state->customAnimIndex >= 0 &&
                    state->customAnimIndex < (int)poseLib->customAnims.size())
                {
                    ComputeCustomAnimationPose(
                        poseLib->customAnims[state->customAnimIndex],
                        time,
                        poseLib->poseLibrary,
                        outPose,
                        model
                    );
                }
                break;

            case AnimStateType::Single:
                model->ComputeAnimation(state->animationIndex, time, outPose);
                break;

            case AnimStateType::Blend1D:
            {
                const bool validA_ = IsValidAnimationIndex(model, state->animIndexA);
                const bool validB_ = IsValidAnimationIndex(model, state->animIndexB);
                if (!validA_ && !validB_) break;
                if (!validA_)
                {
                    model->ComputeAnimation(state->animIndexB, time, outPose);
                    break;
                }
                if (!validB_)
                {
                    model->ComputeAnimation(state->animIndexA, time, outPose);
                    break;
                }

                const float blendWeight_ = ResolveBlendWeight(state, blackboard_);
                const float durationA_ = model->GetAnimationDuration(state->animIndexA);
                const float durationB_ = model->GetAnimationDuration(state->animIndexB);
                const float stateDuration_ = sampleDuration_ > 0.0001f
                    ? sampleDuration_
                    : ResolveBlendDuration(state, model, blackboard_);
                const float timeA_ = ResolveBlendSampleTime(
                    time,
                    stateDuration_,
                    durationA_,
                    state->blendPhaseOffsetA,
                    state->blendSyncNormalizedTime,
                    state->isLoop);
                const float timeB_ = ResolveBlendSampleTime(
                    time,
                    stateDuration_,
                    durationB_,
                    state->blendPhaseOffsetB,
                    state->blendSyncNormalizedTime,
                    state->isLoop);

                model->ComputeAnimation(state->animIndexA, timeA_, outPose);
                model->ComputeAnimation(state->animIndexB, timeB_, blendPoseScratch);

                const auto& nodes_ = model->GetNodes();
                for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount; ++nodeIndex_)
                {
                    if (!IsNodeInBlendMask(state, nodes_, nodeIndex_)) continue;

                    outPose[nodeIndex_].position = DirectX::SimpleMath::Vector3::Lerp(
                        outPose[nodeIndex_].position,
                        blendPoseScratch[nodeIndex_].position,
                        blendWeight_);
                    outPose[nodeIndex_].rotation = DirectX::SimpleMath::Quaternion::Slerp(
                        outPose[nodeIndex_].rotation,
                        blendPoseScratch[nodeIndex_].rotation,
                        blendWeight_);
                    outPose[nodeIndex_].rotation.Normalize();
                    outPose[nodeIndex_].scale = DirectX::SimpleMath::Vector3::Lerp(
                        outPose[nodeIndex_].scale,
                        blendPoseScratch[nodeIndex_].scale,
                        blendWeight_);
                }
            }
            break;

            case AnimStateType::BLEND_2D:
            {
                const Blend2DSelection selection_ = ResolveBlend2DSelection(state, blackboard_);
                const bool validA_ = IsValidAnimationIndex(model, selection_.animationIndexA);
                const bool validB_ = IsValidAnimationIndex(model, selection_.animationIndexB);
                if (!validA_ && !validB_) break;
                if (!validA_)
                {
                    model->ComputeAnimation(selection_.animationIndexB, time, outPose);
                    break;
                }
                if (!validB_)
                {
                    model->ComputeAnimation(selection_.animationIndexA, time, outPose);
                    break;
                }

                const float durationA_ = model->GetAnimationDuration(selection_.animationIndexA);
                const float durationB_ = model->GetAnimationDuration(selection_.animationIndexB);
                const float stateDuration_ = sampleDuration_ > 0.0001f
                    ? sampleDuration_
                    : ResolveBlend2DDuration(state, model, blackboard_);
                const float timeA_ = ResolveBlendSampleTime(
                    time,
                    stateDuration_,
                    durationA_,
                    selection_.phaseOffsetA,
                    state->blend2DSyncNormalizedTime,
                    state->isLoop);
                const float timeB_ = ResolveBlendSampleTime(
                    time,
                    stateDuration_,
                    durationB_,
                    selection_.phaseOffsetB,
                    state->blend2DSyncNormalizedTime,
                    state->isLoop);

                model->ComputeAnimation(selection_.animationIndexA, timeA_, outPose);
                model->ComputeAnimation(selection_.animationIndexB, timeB_, blendPoseScratch);
                for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount; ++nodeIndex_)
                {
                    outPose[nodeIndex_].position = DirectX::SimpleMath::Vector3::Lerp(
                        outPose[nodeIndex_].position,
                        blendPoseScratch[nodeIndex_].position,
                        selection_.weightB);
                    outPose[nodeIndex_].rotation = DirectX::SimpleMath::Quaternion::Slerp(
                        outPose[nodeIndex_].rotation,
                        blendPoseScratch[nodeIndex_].rotation,
                        selection_.weightB);
                    outPose[nodeIndex_].rotation.Normalize();
                    outPose[nodeIndex_].scale = DirectX::SimpleMath::Vector3::Lerp(
                        outPose[nodeIndex_].scale,
                        blendPoseScratch[nodeIndex_].scale,
                        selection_.weightB);
                }
            }
            break;

            default:
                break;
            }
        };

    // ------------------------------------------------------------
    // 1. Duration 取得
    // ------------------------------------------------------------
    float baseDuration = 1.0f;

    if (current.state->type == AnimStateType::Custom &&
        poseLib &&
        current.state->customAnimIndex >= 0 &&
        current.state->customAnimIndex < (int)poseLib->customAnims.size())
    {
        baseDuration = poseLib->customAnims[current.state->customAnimIndex].duration;
    }
    else if (current.state->type == AnimStateType::Single)
    {
        baseDuration = model->GetAnimationDuration(current.state->animationIndex);
    }
    else if (current.state->type == AnimStateType::Blend1D)
    {
        baseDuration = ResolveBlendDuration(current.state, model, blackboard_);
    }
    else if (current.state->type == AnimStateType::BLEND_2D)
    {
        baseDuration = ResolveBlend2DDuration(current.state, model, blackboard_);
    }

    // ------------------------------------------------------------
    // 2. 時間更新
    // ------------------------------------------------------------
    if (current.state->type == AnimStateType::Blend1D ||
        current.state->type == AnimStateType::BLEND_2D)
    {
        if (current.sampleDuration > 0.0001f &&
            std::abs(current.sampleDuration - baseDuration) > 0.0001f)
        {
            float normalizedPhase_ = current.time / current.sampleDuration;
            if (current.state->isLoop)
            {
                normalizedPhase_ -= std::floor(normalizedPhase_);
            }
            else
            {
                normalizedPhase_ = (std::clamp)(normalizedPhase_, 0.0f, 1.0f);
            }
            current.time = normalizedPhase_ * baseDuration;
        }
    }
    current.sampleDuration = baseDuration;
    current.Update(et, baseDuration, curvecomp, owner);

    // ------------------------------------------------------------
    // 3. 現在ポーズ計算
    // ------------------------------------------------------------
    SamplePose(current.state, current.time, current.sampleDuration, tempPoseA);

    // ------------------------------------------------------------
    // 4. クロスフェード
    // ------------------------------------------------------------
    if (isTransitioning)
    {
        fadeTimer += et;
        float alpha = fadeDuration > 0.0f ? (fadeTimer / fadeDuration) : 1.0f;
        if (alpha >= 1.0f)
        {
            alpha = 1.0f;
            isTransitioning = false;
        }

        if (previous.state)
        {
            SamplePose(previous.state, previous.time, previous.sampleDuration, tempPoseB);
        }

        for (size_t i = 0; i < nodeCount; ++i)
        {
            finalPose[i].position = DirectX::SimpleMath::Vector3::Lerp(tempPoseB[i].position, tempPoseA[i].position, alpha);
            finalPose[i].rotation = DirectX::SimpleMath::Quaternion::Slerp(tempPoseB[i].rotation, tempPoseA[i].rotation, alpha);
            finalPose[i].rotation.Normalize();
            finalPose[i].scale = DirectX::SimpleMath::Vector3::Lerp(tempPoseB[i].scale, tempPoseA[i].scale, alpha);
        }
    }
    else
    {
        finalPose = tempPoseA;
    }



    // ------------------------------------------------------------
    // 5. RootMotion extraction
    // ------------------------------------------------------------
    currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;

    auto resolveRootNodeIndex_ = [&](const AnimState* state_)
        {
            if (!state_ || !state_->rootMotion.enable) return -1;

            const std::string rootName_ = state_->rootMotion.sourceNodeName.empty()
                ? model->GetRootMotionName()
                : state_->rootMotion.sourceNodeName;

            return rootName_.empty() ? -1 : model->GetNodeIndex(rootName_.c_str());
        };

    const int currentRootNodeIndex_ = resolveRootNodeIndex_(current.state);
    const int previousRootNodeIndex_ = isTransitioning ? resolveRootNodeIndex_(previous.state) : -1;
    const int defaultRootNodeIndex_ = model->GetRootMotionName().empty() ? -1 : model->GetNodeIndex(model->GetRootMotionName().c_str());

    if (currentRootNodeIndex_ != -1 && tempPoseA.size() > currentRootNodeIndex_)
    {
        using namespace DirectX::SimpleMath;

        const Vector3 rawPos_ = tempPoseA[currentRootNodeIndex_].position;
        const auto& bindRootPose_ = model->GetBindNodePose(currentRootNodeIndex_);
        const bool looped_ = (current.time < _prevTime);
        const bool stateChanged_ = (_prevRootMotionState != current.state);
        const bool sourceNodeChanged_ = (previousRootMotionNodeIndex != currentRootNodeIndex_);

        if (_isFirstFrame || stateChanged_ || sourceNodeChanged_)
        {
            _prevRootPos = rawPos_;
            rootMotionBasePosition = bindRootPose_.position;
            _prevTime = current.time;
            _prevRootMotionState = current.state;
            previousRootMotionNodeIndex = currentRootNodeIndex_;
            _isFirstFrame = false;
        }
        else
        {
            Vector3 deltaPos_;
            if (looped_)
            {
                const float loopEndTime_ = (std::max)(0.0f, baseDuration - 0.0001f);
                SamplePose(current.state, 0.0f, baseDuration, rootMotionLoopStartPose);
                SamplePose(current.state, loopEndTime_, baseDuration, rootMotionLoopEndPose);

                const Vector3 loopStartPos_ = rootMotionLoopStartPose[currentRootNodeIndex_].position;
                const Vector3 loopEndPos_ = rootMotionLoopEndPose[currentRootNodeIndex_].position;
                deltaPos_ = (loopEndPos_ - _prevRootPos) + (rawPos_ - loopStartPos_);
            }
            else
            {
                deltaPos_ = rawPos_ - _prevRootPos;
            }

            if (!current.state->rootMotion.applyPositionX) deltaPos_.x = 0.0f;
            if (!current.state->rootMotion.applyPositionY) deltaPos_.y = 0.0f;
            if (!current.state->rootMotion.applyPositionZ) deltaPos_.z = 0.0f;

            float rootMotionScale_ = current.state->rootMotionScale;
            if (curvecomp && !current.state->rootMotionScaleCurveName.empty())
            {
                float normalizedTime_ = (baseDuration > 0.0f) ? (current.time / baseDuration) : 0.0f;
                normalizedTime_ = (std::max)(0.0f, (std::min)(1.0f, normalizedTime_));
                rootMotionScale_ *= curvecomp->GetValue(current.state->rootMotionScaleCurveName, normalizedTime_);
            }

            currentRootMotionDelta.deltaPosition = deltaPos_ * rootMotionScale_;

            _prevRootPos = rawPos_;
            _prevTime = current.time;
            _prevRootMotionState = current.state;
            previousRootMotionNodeIndex = currentRootNodeIndex_;
        }
    }
    else
    {
        _isFirstFrame = true;
        _prevTime = 0.0f;
        _prevRootMotionState = nullptr;
        previousRootMotionNodeIndex = -1;
        currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
        currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    }

    auto resetVisualRootPosition_ = [&](int nodeIndex_)
        {
            if (nodeIndex_ < 0 || finalPose.size() <= nodeIndex_) return;

            const auto& bindRootPose_ = model->GetBindNodePose(nodeIndex_);
            finalPose[nodeIndex_].position = bindRootPose_.position;
        };

    resetVisualRootPosition_(currentRootNodeIndex_);
    if (previousRootNodeIndex_ != currentRootNodeIndex_)
    {
        resetVisualRootPosition_(previousRootNodeIndex_);
    }
    if (isTransitioning && defaultRootNodeIndex_ != currentRootNodeIndex_ && defaultRootNodeIndex_ != previousRootNodeIndex_)
    {
        resetVisualRootPosition_(defaultRootNodeIndex_);
    }
}


void ModelAnimator::UpdateBlend(float et, GltfModelCommponennt* model, int indexA, int indexB, float alpha, bool loop, float speed)
{
    if (!model) return;

    // 1. 各アニメーションの時間を進める (簡易化のため共通の時間管理)

    float durationA = model->GetAnimationDuration(indexA);
    manualBlendTime += et * speed;
    if (loop && durationA > 0.0f) manualBlendTime = fmod(manualBlendTime, durationA);

    // 2. AとBのポーズをそれぞれ計算
    model->ComputeAnimation(indexA, manualBlendTime, tempPoseA);
    // BはAの時間に合わせるか、B自身のDurationで正規化し直す
    float durationB = model->GetAnimationDuration(indexB);
    float timeB = (durationA > 0.0f) ? (manualBlendTime / durationA) * durationB : 0.0f;
    model->ComputeAnimation(indexB, timeB, tempPoseB);

    // 3. alpha を用いて線形補間
    if (finalPose.size() != tempPoseA.size()) finalPose.resize(tempPoseA.size());

    for (size_t i = 0; i < tempPoseA.size(); ++i)
    {
        // Position / Scale は Lerp, Rotation は Slerp
        DirectX::XMVECTOR pA = DirectX::XMLoadFloat3(&tempPoseA[i].position);
        DirectX::XMVECTOR pB = DirectX::XMLoadFloat3(&tempPoseB[i].position);
        DirectX::XMStoreFloat3(&finalPose[i].position, DirectX::XMVectorLerp(pA, pB, alpha));

        DirectX::XMVECTOR qA = DirectX::XMLoadFloat4(&tempPoseA[i].rotation);
        DirectX::XMVECTOR qB = DirectX::XMLoadFloat4(&tempPoseB[i].rotation);
        DirectX::XMStoreFloat4(&finalPose[i].rotation, DirectX::XMQuaternionSlerp(qA, qB, alpha));

        DirectX::XMVECTOR sA = DirectX::XMLoadFloat3(&tempPoseA[i].scale);
        DirectX::XMVECTOR sB = DirectX::XMLoadFloat3(&tempPoseB[i].scale);
        DirectX::XMStoreFloat3(&finalPose[i].scale, DirectX::XMVectorLerp(sA, sB, alpha));
    }
}

void ModelAnimator::CrossFadeTo(const AnimState* newState, float duration, std::shared_ptr<GameObject> owner)
{
    if (!newState) return;
    if (current.state == newState) return;

    CallStateExitEndActions(current, owner);
    previous = current;

    current.state = newState;
    current.time = 0.0f;
    current.weight = 1.0f;
    current.sampleDuration = 0.0f;

    // 新しいステートに切り替わる際、イベントフラグをリセットしておく
    for (auto& ev : const_cast<AnimState*>(current.state)->events) {
        ev.isFired = false;
        ev.isEndFired = false;
    }

    fadeDuration = (duration > 0.0f) ? duration : 0.001f;
    fadeTimer = 0.0f;
    isMixing = true;
    isTransitioning = true;


}

void ModelAnimator::ComputeCustomAnimationPose(const CustomAnimation& customAnim, float time, const std::vector<SavedPose>& poseLibrary, std::vector<GltfModelCommponennt::NodePose>& outPose, GltfModelCommponennt* model)
{
    const size_t nodeCount_ = model->GetNodes().size();
    const auto& nodes_ = model->GetNodes();

    auto fillBindPose_ = [&](std::vector<GltfModelCommponennt::NodePose>& pose_)
        {
            pose_.resize(nodeCount_);
            for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount_; ++nodeIndex_)
            {
                pose_[nodeIndex_].position = nodes_[nodeIndex_].position;
                pose_[nodeIndex_].rotation = nodes_[nodeIndex_].rotation;
                pose_[nodeIndex_].scale = nodes_[nodeIndex_].scale;
            }
        };

    auto sampleModelAnimation_ = [&](int animationIndex_, float sampleTime_, std::vector<GltfModelCommponennt::NodePose>& pose_) -> bool
        {
            if (animationIndex_ < 0 || animationIndex_ >= static_cast<int>(model->GetAnimations().size())) return false;

            fillBindPose_(pose_);
            const float duration_ = model->GetAnimationDuration(animationIndex_);
            if (duration_ > 0.0f)
            {
                sampleTime_ = (std::max)(0.0f, sampleTime_);
                if (sampleTime_ > duration_) sampleTime_ = std::fmod(sampleTime_, duration_);
            }
            model->ComputeAnimation(animationIndex_, sampleTime_, pose_);
            return true;
        };

    auto sampleKeyframe_ = [&](const PoseKeyframe& keyframe_, std::vector<GltfModelCommponennt::NodePose>& pose_) -> bool
        {
            if (keyframe_.poseIndex >= 0 && keyframe_.poseIndex < static_cast<int>(poseLibrary.size()))
            {
                fillBindPose_(pose_);
                const auto& savedPose_ = poseLibrary[keyframe_.poseIndex].nodePoses;
                for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount_ && nodeIndex_ < savedPose_.size(); ++nodeIndex_)
                {
                    pose_[nodeIndex_] = savedPose_[nodeIndex_];
                }
                return true;
            }

            return sampleModelAnimation_(keyframe_.sourceAnimIdx, keyframe_.sourceTime, pose_);
        };

    std::vector<GltfModelCommponennt::NodePose> basePose_;
    std::vector<GltfModelCommponennt::NodePose> editPose_;
    const float baseTime_ = customAnim.baseAnimationStartTime + time * customAnim.baseAnimationSpeed;
    const bool hasBasePose_ = sampleModelAnimation_(customAnim.baseAnimationIndex, baseTime_, basePose_);
    bool hasEditPose_ = false;

    if (!customAnim.keyframes.empty())
    {
        int leftKey_ = -1;
        int rightKey_ = -1;
        for (int keyIndex_ = 0; keyIndex_ < static_cast<int>(customAnim.keyframes.size()); ++keyIndex_)
        {
            const float keyTime_ = customAnim.keyframes[keyIndex_].time;
            if (keyTime_ <= time && (leftKey_ < 0 || keyTime_ > customAnim.keyframes[leftKey_].time)) leftKey_ = keyIndex_;
            if (keyTime_ >= time && (rightKey_ < 0 || keyTime_ < customAnim.keyframes[rightKey_].time)) rightKey_ = keyIndex_;
        }
        if (leftKey_ < 0) leftKey_ = rightKey_;
        if (rightKey_ < 0) rightKey_ = leftKey_;

        std::vector<GltfModelCommponennt::NodePose> leftPose_;
        std::vector<GltfModelCommponennt::NodePose> rightPose_;
        const bool hasLeftPose_ = sampleKeyframe_(customAnim.keyframes[leftKey_], leftPose_);
        const bool hasRightPose_ = sampleKeyframe_(customAnim.keyframes[rightKey_], rightPose_);

        if (hasLeftPose_ && hasRightPose_)
        {
            editPose_.resize(nodeCount_);
            const float leftTime_ = customAnim.keyframes[leftKey_].time;
            const float rightTime_ = customAnim.keyframes[rightKey_].time;
            const float rawFactor_ = rightTime_ > leftTime_ ? (time - leftTime_) / (rightTime_ - leftTime_) : 0.0f;
            const float factor_ = ApplyPoseInterpolation(customAnim.keyframes[leftKey_].interpolation, rawFactor_);
            for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount_; ++nodeIndex_)
            {
                editPose_[nodeIndex_].position = DirectX::SimpleMath::Vector3::Lerp(leftPose_[nodeIndex_].position, rightPose_[nodeIndex_].position, factor_);
                editPose_[nodeIndex_].rotation = DirectX::SimpleMath::Quaternion::Slerp(leftPose_[nodeIndex_].rotation, rightPose_[nodeIndex_].rotation, factor_);
                editPose_[nodeIndex_].rotation.Normalize();
                editPose_[nodeIndex_].scale = DirectX::SimpleMath::Vector3::Lerp(leftPose_[nodeIndex_].scale, rightPose_[nodeIndex_].scale, factor_);
            }
            hasEditPose_ = true;
        }
        else if (hasLeftPose_)
        {
            editPose_ = leftPose_;
            hasEditPose_ = true;
        }
        else if (hasRightPose_)
        {
            editPose_ = rightPose_;
            hasEditPose_ = true;
        }
    }

    if (hasBasePose_ && hasEditPose_)
    {
        const float poseBlend_ = (std::max)(0.0f, (std::min)(1.0f, customAnim.poseBlend));
        outPose.resize(nodeCount_);
        for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount_; ++nodeIndex_)
        {
            outPose[nodeIndex_].position = DirectX::SimpleMath::Vector3::Lerp(basePose_[nodeIndex_].position, editPose_[nodeIndex_].position, poseBlend_);
            outPose[nodeIndex_].rotation = DirectX::SimpleMath::Quaternion::Slerp(basePose_[nodeIndex_].rotation, editPose_[nodeIndex_].rotation, poseBlend_);
            outPose[nodeIndex_].rotation.Normalize();
            outPose[nodeIndex_].scale = DirectX::SimpleMath::Vector3::Lerp(basePose_[nodeIndex_].scale, editPose_[nodeIndex_].scale, poseBlend_);
        }
    }
    else if (hasEditPose_)
    {
        outPose = editPose_;
    }
    else if (hasBasePose_)
    {
        outPose = basePose_;
    }
    else
    {
        fillBindPose_(outPose);
    }

    if (!customAnim.naturalMotion.enabled) return;

    const auto& settings_ = customAnim.naturalMotion;
    const float blend_ = (std::max)(0.0f, (std::min)(1.0f, settings_.blend));
    for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount_; ++nodeIndex_)
    {
        if (!MatchesNaturalMotionNode(settings_.nodeFilter, nodes_[nodeIndex_].name)) continue;

        const float wave_ = std::sin((time * settings_.frequency + settings_.phase + static_cast<float>(nodeIndex_) * 0.173f) * DirectX::XM_2PI);
        const float strength_ = wave_ * blend_;
        if (settings_.rotationAmplitudeDeg != 0.0f)
        {
            const float radians_ = DirectX::XMConvertToRadians(settings_.rotationAmplitudeDeg * strength_);
            const auto additionalRotation_ = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(radians_ * 0.25f, radians_ * 0.5f, radians_);
            DirectX::SimpleMath::Quaternion baseRotation_(outPose[nodeIndex_].rotation.x, outPose[nodeIndex_].rotation.y, outPose[nodeIndex_].rotation.z, outPose[nodeIndex_].rotation.w);
            outPose[nodeIndex_].rotation = baseRotation_ * additionalRotation_;
            outPose[nodeIndex_].rotation.Normalize();
        }
        if (settings_.positionAmplitude != 0.0f)
        {
            outPose[nodeIndex_].position.y += settings_.positionAmplitude * strength_;
        }
    }
}
void ModelAnimator::ActiveSlot::Update(float et, float duration, CurveComponent* curveComp, std::shared_ptr<GameObject> owner)
{
    if (!state) return;

    const AnimState* updatingState_ = state;

    float currentSpeed = state->playbackSpeed;

    // ★カーブの指定があり、コンポーネントが存在する場合
    if (curveComp && !state->speedCurveName.empty())
    {
        // アニメーションの正規化時間 (0.0 - 1.0) を計算
        float normalizedTime = (duration > 0.0f) ? (time / duration) : 0.0f;

        // 指定された名前のカーブから速度を取得
        currentSpeed = curveComp->GetValue(state->speedCurveName, normalizedTime);
    }

    for (const AnimEvent& event_ : state->events)
    {
        if (event_.overridePlaybackSpeed &&
            time >= event_.startTime &&
            time <= event_.endTime)
        {
            currentSpeed = (std::max)(event_.playbackSpeed, 0.01f);
        }
    }

    float prevTime = time;
    time += et * currentSpeed;
    if (et == 0.0f) return;

    // イベントの発火判定
    for (auto& ev : const_cast<AnimState*>(state)->events)
    {
        // 1. 開始イベント (startTime を跨いだ瞬間)
        if (!ev.isFired && time >= ev.startTime)
        {
            if (ev.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
            {
                AttackTemplateRuntime::Start(owner, ev);
            }
            if (ev.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::VFX_TEMPLATE ||
                !ev.meshVFXAssetPath.empty())
            {
                SpawnAnimationMeshVFXEvent(owner, ev);
            }
            ApplyAnimationBlackboardWrites(owner, ev.blackboardWrites);
            if (!ev.startAction.empty()) {
                CallAnimationAction(owner, ev.startAction);
                if (state != updatingState_) return;
            }
            ev.isFired = true;
        }

        // 2. 更新イベント (範囲内にいる間ずっと)
        if (time >= ev.startTime && time <= ev.endTime)
        {
            if (ev.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
            {
                AttackTemplateRuntime::Update(owner, ev, time - ev.startTime);
            }
            if (!ev.updateAction.empty()) {
                CallAnimationAction(owner, ev.updateAction);
                if (state != updatingState_) return;
            }
        }

        // 3. End event, fired when endTime is crossed.
        if (!ev.isEndFired && prevTime <= ev.endTime && time >= ev.endTime)
        {
            if (ev.eventTemplateType == ANIMATION_EVENT_TEMPLATE_TYPE::ATTACK_TEMPLATE)
            {
                AttackTemplateRuntime::End(owner, ev);
            }
            if (!ev.endAction.empty()) {
                CallAnimationAction(owner, ev.endAction);
                if (state != updatingState_) return;
            }
            ev.isEndFired = true;
        }
    }

    // ループ処理とフラグのリセット
    if (time >= duration)
    {
        if (state->isLoop) {
            time = fmod(time, duration);
            // ループしたので発火フラグをリセット
            for (auto& ev : const_cast<AnimState*>(state)->events) {
                ev.isFired = false;
                ev.isEndFired = false;
            }
        }
        else {
            time = duration;
        }
    }
}


