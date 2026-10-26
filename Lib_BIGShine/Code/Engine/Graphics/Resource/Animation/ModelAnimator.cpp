#include "PrecompiledHeader.h"

#include "ModelAnimator.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/GamePlay/Combat/AttackTemplateRuntime.h"
#include "Engine/GamePlay/System/EffectManager.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>


namespace
{
    float NormalizeAngleDegrees(float degrees_)
    {
        // 回転差分を扱いやすいよう、角度を-180〜180度の範囲へ寄せる。
        while (degrees_ > 180.0f) degrees_ -= 360.0f;
        while (degrees_ < -180.0f) degrees_ += 360.0f;
        return degrees_;
    }

    float GetYawDegreesFromQuaternion(
        const DirectX::SimpleMath::Quaternion& rotation_)
    {
        // ルートモーションでは水平回転だけが必要なので、QuaternionからYaw成分だけを取り出す。
        return NormalizeAngleDegrees(
            DirectX::XMConvertToDegrees(
                2.0f * std::atan2(rotation_.y, rotation_.w)));
    }
}


static bool CallLocalAnimationAction(const std::shared_ptr<GameObject>& owner_, const std::string& actionName_)
{
    if (!owner_ || actionName_.empty()) return false;

    // まず所有オブジェクトの命名規則込みのAction名を探し、なければ素の名前でも探す。
    // これにより、汎用イベント名とオブジェクト固有イベント名の両方を扱える。
    const std::string resolvedActionName_ = owner_->BuildActionName(actionName_);
    if (owner_->localRegistry.HasAction(resolvedActionName_)) {
        owner_->localRegistry.Call(resolvedActionName_);
        return true;
    }

    if (owner_->localRegistry.HasAction(actionName_)) {
        owner_->localRegistry.Call(actionName_);
        return true;
    }

    return false;
}

static std::shared_ptr<GameObject> GetParentObject(const std::shared_ptr<GameObject>& owner_)
{
    if (!owner_ || !owner_->transform) return nullptr;

    // Transform階層から親GameObjectを取り出し、アニメイベントの伝搬先を探す。
    auto parentTransform_ = owner_->transform->parent.lock();
    return parentTransform_ ? parentTransform_->gameObject.lock() : nullptr;
}

static bool CallAncestorAnimationAction(const std::shared_ptr<GameObject>& owner_, const std::string& actionName_)
{
    // 武器や子モデル側のアニメから、親キャラクター側に登録されたActionを呼ぶために祖先を辿る。
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

    // 親モデル側のアニメイベントから、武器や子オブジェクト側のActionも拾えるように再帰探索する。
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

    // アニメーションイベントからEffectManager用のリクエストへ変換し、VFX生成処理を一元化する。
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

    // ローカル、親階層、子階層の順に探し、それでも見つからなければグローバルActionへ委譲する。
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

    // アニメーションイベントからAI/行動制御用のBlackboard値を書き換える。
    // 型ごとにSet関数を分け、JSON側の値型とランタイム側の保存先を一致させる。
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

    // ステートを途中で抜けた場合でも、開始済みイベントの終了処理だけは取りこぼさない。
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

    // エディタで選んだ補間タイプを、0〜1のブレンド率へ変換する。
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
    // フィルタが空なら全ノード対象、文字列があれば名前に含むノードだけを自然揺れ対象にする。
    return filter.empty() || nodeName.find(filter) != std::string::npos;
}


static float ResolveBlendWeight(
    const AnimState* state_,
    const BlackboardCategory* blackboard_)
{
    if (!state_) return 0.0f;

    // ブレンド用Blackboardがない場合は、ステートに設定されたデフォルトウェイトを使う。
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
    const float inputLength_ = std::sqrt(input_.x * input_.x + input_.y * input_.y);
    if (state_->blend2DUseIdleStop &&
        inputLength_ <= state_->blend2DIdleThreshold &&
        state_->blend2DIdleAnimationIndex >= 0)
    {
        selection_.animationIndexA = state_->blend2DIdleAnimationIndex;
        selection_.phaseOffsetA = state_->blend2DIdlePhaseOffset;
        selection_.weightB = 0.0f;
        return selection_;
    }

    if (absoluteX_ + absoluteY_ <= 0.000001f)
    {
        input_ = { state_->blend2DDefaultInput.x, state_->blend2DDefaultInput.y };
        absoluteX_ = std::abs(input_.x);
        absoluteY_ = std::abs(input_.y);
    }

    if (state_->blend2DUseForwardBackwardLateral)
    {
        struct DirectionClip
        {
            int animationIndex = -1;
            float phaseOffset = 0.0f;
        };

        const bool useBackward_ = input_.y < 0.0f;
        const bool useLeft_ = input_.x < 0.0f;
        const DirectionClip centerClip_ = useBackward_
            ? DirectionClip{ state_->blend2DBackwardAnimationIndex, state_->blend2DBackwardPhaseOffset }
            : DirectionClip{ state_->blend2DForwardAnimationIndex, state_->blend2DForwardPhaseOffset };
        const DirectionClip lateral45Clip_ = useBackward_
            ? (useLeft_
                ? DirectionClip{ state_->blend2DBackwardLeftAnimationIndex, state_->blend2DBackwardLeftPhaseOffset }
                : DirectionClip{ state_->blend2DBackwardRightAnimationIndex, state_->blend2DBackwardRightPhaseOffset })
            : (useLeft_
                ? DirectionClip{ state_->blend2DForwardLeftAnimationIndex, state_->blend2DForwardLeftPhaseOffset }
                : DirectionClip{ state_->blend2DForwardRightAnimationIndex, state_->blend2DForwardRightPhaseOffset });
        const DirectionClip lateral90Clip_ = useBackward_
            ? (useLeft_
                ? DirectionClip{ state_->blend2DBackwardLeft90AnimationIndex, state_->blend2DBackwardLeft90PhaseOffset }
                : DirectionClip{ state_->blend2DBackwardRight90AnimationIndex, state_->blend2DBackwardRight90PhaseOffset })
            : (useLeft_
                ? DirectionClip{ state_->blend2DForwardLeft90AnimationIndex, state_->blend2DForwardLeft90PhaseOffset }
                : DirectionClip{ state_->blend2DForwardRight90AnimationIndex, state_->blend2DForwardRight90PhaseOffset });

        constexpr float HALF_PI_ = 1.57079632679f;
        const float lateralRate_ = std::clamp(
            std::atan2(absoluteX_, (std::max)(absoluteY_, 0.000001f)) / HALF_PI_,
            0.0f,
            1.0f);
        if (lateralRate_ <= 0.5f)
        {
            selection_.animationIndexA = centerClip_.animationIndex;
            selection_.phaseOffsetA = centerClip_.phaseOffset;
            selection_.animationIndexB = lateral45Clip_.animationIndex;
            selection_.phaseOffsetB = lateral45Clip_.phaseOffset;
            selection_.weightB = lateralRate_ * 2.0f;
            return selection_;
        }

        selection_.animationIndexA = lateral45Clip_.animationIndex;
        selection_.phaseOffsetA = lateral45Clip_.phaseOffset;
        selection_.animationIndexB = lateral90Clip_.animationIndex;
        selection_.phaseOffsetB = lateral90Clip_.phaseOffset;
        selection_.weightB = (lateralRate_ - 0.5f) * 2.0f;
        return selection_;
    }

    if (state_->blend2DUseEightDirections)
    {
        struct DirectionClip
        {
            int animationIndex = -1;
            float phaseOffset = 0.0f;
        };

        const DirectionClip clips_[] =
        {
            { state_->blend2DForwardAnimationIndex, state_->blend2DForwardPhaseOffset },
            { state_->blend2DForwardRightAnimationIndex, state_->blend2DForwardRightPhaseOffset },
            { state_->blend2DRightAnimationIndex, state_->blend2DRightPhaseOffset },
            { state_->blend2DBackwardRightAnimationIndex, state_->blend2DBackwardRightPhaseOffset },
            { state_->blend2DBackwardAnimationIndex, state_->blend2DBackwardPhaseOffset },
            { state_->blend2DBackwardLeftAnimationIndex, state_->blend2DBackwardLeftPhaseOffset },
            { state_->blend2DLeftAnimationIndex, state_->blend2DLeftPhaseOffset },
            { state_->blend2DForwardLeftAnimationIndex, state_->blend2DForwardLeftPhaseOffset }
        };

        constexpr float TWO_PI_ = 6.28318530718f;
        constexpr float SEGMENT_ANGLE_ = TWO_PI_ / 8.0f;
        float angle_ = std::atan2(input_.x, input_.y);
        if (angle_ < 0.0f) angle_ += TWO_PI_;

        const float segment_ = angle_ / SEGMENT_ANGLE_;
        const int indexA_ = static_cast<int>(std::floor(segment_)) % 8;
        const int indexB_ = (indexA_ + 1) % 8;

        selection_.animationIndexA = clips_[indexA_].animationIndex;
        selection_.phaseOffsetA = clips_[indexA_].phaseOffset;
        selection_.animationIndexB = clips_[indexB_].animationIndex;
        selection_.phaseOffsetB = clips_[indexB_].phaseOffset;
        selection_.weightB = segment_ - std::floor(segment_);
        return selection_;
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

static const AnimState* ResolveSequenceReferencedState(
    const AnimSequenceClip& clip_,
    const std::unordered_map<std::string, std::unique_ptr<AnimState>>* states_)
{
    if (!states_ || clip_.stateName.empty()) return nullptr;

    const auto iterator_ = states_->find(clip_.stateName);
    if (iterator_ == states_->end() || !iterator_->second) return nullptr;

    return iterator_->second.get();
}

static float ResolveSequenceDuration(const AnimState* state_)
{
    if (!state_ || state_->sequenceClips.empty()) return 1.0f;

    float duration_ = 0.0f;
    for (const AnimSequenceClip& clip_ : state_->sequenceClips)
    {
        duration_ = (std::max)(duration_, clip_.endTime);
    }
    return (std::max)(duration_, 0.0001f);
}

static float ResolveStateDuration(
    const AnimState* state_,
    GltfModelCommponennt* model_,
    const std::shared_ptr<PoseLibraryResource>& poseLibrary_,
    const BlackboardCategory* blackboard_,
    const std::unordered_map<std::string, std::unique_ptr<AnimState>>* states_,
    int depth_)
{
    if (!state_ || !model_ || depth_ > 8) return 1.0f;

    if (state_->type == AnimStateType::Custom &&
        poseLibrary_ &&
        state_->customAnimIndex >= 0 &&
        state_->customAnimIndex < static_cast<int>(poseLibrary_->customAnims.size()))
    {
        return (std::max)(poseLibrary_->customAnims[state_->customAnimIndex].duration, 0.0001f);
    }
    if (state_->type == AnimStateType::Single)
    {
        return IsValidAnimationIndex(model_, state_->animationIndex)
            ? (std::max)(model_->GetAnimationDuration(state_->animationIndex), 0.0001f)
            : 1.0f;
    }
    if (state_->type == AnimStateType::Blend1D)
    {
        return ResolveBlendDuration(state_, model_, blackboard_);
    }
    if (state_->type == AnimStateType::BLEND_2D)
    {
        return ResolveBlend2DDuration(state_, model_, blackboard_);
    }
    if (state_->type == AnimStateType::SEQUENCE)
    {
        return ResolveSequenceDuration(state_);
    }

    return 1.0f;
}

static const AnimSequenceClip* ResolveSequenceClipAtTime(
    const AnimState* state_,
    float time_)
{
    if (!state_) return nullptr;

    const AnimSequenceClip* firstClip_ = nullptr;
    const AnimSequenceClip* previousClip_ = nullptr;
    for (const AnimSequenceClip& clip_ : state_->sequenceClips)
    {
        if (!firstClip_ || clip_.startTime < firstClip_->startTime)
        {
            firstClip_ = &clip_;
        }
        if (clip_.startTime <= time_ &&
            (!previousClip_ || clip_.startTime > previousClip_->startTime))
        {
            previousClip_ = &clip_;
        }

        if (time_ >= clip_.startTime && time_ <= clip_.endTime)
        {
            return &clip_;
        }
    }

    return previousClip_ ? previousClip_ : firstClip_;
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
    attackTelegraphs.Reset();
    current.time = (std::max)(0.0f, time_);
    previous = current;
    isTransitioning = false;
    isMixing = false;
    fadeTimer = 0.0f;

    // シークは連続再生ではないため、直前のRoot Motionとの差分を適用しない。
    currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    isFirstFrame = true;
    prevTime = current.time;
    _prevRootMotionState = nullptr;
    prevRootRotation = DirectX::SimpleMath::Quaternion::Identity;
    prevRootYawDegrees = 0.0f;
    previousRootMotionNodeIndex = -1;
    hasPrevPose = false;
}

void ModelAnimator::Update(float et, GltfModelCommponennt* model, CurveComponent* curvecomp, const std::shared_ptr<PoseLibraryResource>& poseLib, std::shared_ptr<GameObject> owner, const BlackboardCategory* blackboard_, const std::unordered_map<std::string, std::unique_ptr<AnimState>>* states_)
{
    BS_PROFILE_SCOPE("ModelAnimator::Update");
    if (!current.state || !model) return;

    const size_t nodeCount = model->GetNodes().size();
    BS_PROFILE_PLOT("Animation.UpdatedNodeCount", static_cast<int64_t>(nodeCount));
#if BS_PROFILE_DETAILED_ENABLED
    {
        char profileText_[256]{};
        const int textLength_ = std::snprintf(
            profileText_,
            sizeof(profileText_),
            "State: %s\nNodeCount: %zu\nTransitioning: %s\nMixing: %s",
            current.state ? current.state->name.c_str() : "None",
            nodeCount,
            isTransitioning ? "true" : "false",
            isMixing ? "true" : "false");
        if (textLength_ > 0)
        {
            BS_PROFILE_TEXT(
                profileText_,
                static_cast<size_t>((std::min)(textLength_, static_cast<int>(sizeof(profileText_) - 1))));
        }
    }
#endif

    if (tempPoseA.size() != nodeCount) tempPoseA.resize(nodeCount);
    if (tempPoseB.size() != nodeCount) tempPoseB.resize(nodeCount);
    if (blendPoseScratch.size() != nodeCount) blendPoseScratch.resize(nodeCount);
    if (finalPose.size() != nodeCount) finalPose.resize(nodeCount);
    if (rootMotionLoopStartPose.size() != nodeCount) rootMotionLoopStartPose.resize(nodeCount);
    if (rootMotionLoopEndPose.size() != nodeCount) rootMotionLoopEndPose.resize(nodeCount);

    // ------------------------------------------------------------
    // 補助: 状態から pose をサンプルする
    // ------------------------------------------------------------
    std::function<void(const AnimState*, float, float, std::vector<GltfModelCommponennt::NodePose>&, int)> SamplePose;
    SamplePose = [&](const AnimState* state, float time, float sampleDuration_, std::vector<GltfModelCommponennt::NodePose>& outPose, int depth_)
        {
            BS_PROFILE_SCOPE("ModelAnimator::SampleAnimation");
            if (!state || depth_ > 8) return;
            BS_PROFILE_TEXT(state->name.c_str(), state->name.size());

            auto fillBindPose_ = [&]()
                {
                    const auto& nodes_ = model->GetNodes();
                    outPose.resize(nodes_.size());
                    for (size_t nodeIndex_ = 0; nodeIndex_ < nodes_.size(); ++nodeIndex_)
                    {
                        outPose[nodeIndex_].position = nodes_[nodeIndex_].position;
                        outPose[nodeIndex_].rotation = nodes_[nodeIndex_].rotation;
                        outPose[nodeIndex_].scale = nodes_[nodeIndex_].scale;
                    }
                };

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
                else
                {
                    fillBindPose_();
                }
                break;

            case AnimStateType::Single:
                if (IsValidAnimationIndex(model, state->animationIndex))
                {
                    model->ComputeAnimation(state->animationIndex, time, outPose);
                }
                else
                {
                    fillBindPose_();
                }
                break;

            case AnimStateType::SEQUENCE:
            {
                BS_PROFILE_SCOPE("ModelAnimator::SequenceAnimation");
                const AnimSequenceClip* clip_ = ResolveSequenceClipAtTime(state, time);
                if (!clip_)
                {
                    fillBindPose_();
                    break;
                }

                const AnimState* referencedState_ = ResolveSequenceReferencedState(*clip_, states_);
                const float sectionDuration_ = (std::max)(clip_->endTime - clip_->startTime, 0.0001f);
                float sourceDuration_ = 1.0f;
                if (referencedState_)
                {
                    sourceDuration_ = ResolveStateDuration(
                        referencedState_,
                        model,
                        poseLib,
                        blackboard_,
                        states_,
                        depth_ + 1);
                }
                else if (IsValidAnimationIndex(model, clip_->animationIndex))
                {
                    sourceDuration_ = (std::max)(
                        model->GetAnimationDuration(clip_->animationIndex),
                        0.0001f);
                }
                else
                {
                    fillBindPose_();
                    break;
                }

                const float normalizedTime_ = (std::clamp)(
                    (time - clip_->startTime) / sectionDuration_,
                    0.0f,
                    1.0f);
                const float fitDuration_ = (std::max)(sourceDuration_ - clip_->clipStartTime, 0.0001f);
                const float localTime_ = clip_->fitToRange
                    ? clip_->clipStartTime + normalizedTime_ * fitDuration_
                    : clip_->clipStartTime + (time - clip_->startTime) * clip_->playbackSpeed;
                const bool sourceLoop_ = referencedState_ ? referencedState_->isLoop : state->isLoop;
                const float sampleTime_ = sourceLoop_ && sourceDuration_ > 0.0f
                    ? std::fmod(localTime_, sourceDuration_)
                    : (std::clamp)(localTime_, 0.0f, sourceDuration_);

                if (referencedState_ && referencedState_ != state)
                {
                    SamplePose(referencedState_, sampleTime_, sourceDuration_, outPose, depth_ + 1);
                }
                else if (IsValidAnimationIndex(model, clip_->animationIndex))
                {
                    model->ComputeAnimation(clip_->animationIndex, sampleTime_, outPose);
                }
                else
                {
                    fillBindPose_();
                }
            }
            break;

            case AnimStateType::Blend1D:
            {
                BS_PROFILE_SCOPE("ModelAnimator::BlendAnimation");
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
                BS_PROFILE_SCOPE("ModelAnimator::BlendAnimation");
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
    else if (current.state->type == AnimStateType::SEQUENCE)
    {
        baseDuration = ResolveSequenceDuration(current.state);
    }

    // ------------------------------------------------------------
    // 2. 時間更新
    // ------------------------------------------------------------
    {
        BS_PROFILE_SCOPE("ModelAnimator::AdvanceAnimationTime");
        if (current.state->type == AnimStateType::Blend1D ||
            current.state->type == AnimStateType::BLEND_2D ||
            current.state->type == AnimStateType::SEQUENCE)
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
        current.Update(et, baseDuration, curvecomp, owner, &attackTelegraphs);
    }

    // ------------------------------------------------------------
    // 3. 現在ポーズ計算
    // ------------------------------------------------------------
    {
        BS_PROFILE_SCOPE("ModelAnimator::EvaluateCurrentAnimation");
        SamplePose(current.state, current.time, current.sampleDuration, tempPoseA, 0);
    }

    // ------------------------------------------------------------
    // 4. クロスフェード
    // ------------------------------------------------------------
    float transitionAlpha_ = 1.0f;
    {
        BS_PROFILE_SCOPE("ModelAnimator::EvaluateBlendAnimation");
        if (isTransitioning)
        {
            fadeTimer += et;
            transitionAlpha_ = fadeDuration > 0.0f ? (fadeTimer / fadeDuration) : 1.0f;
            if (transitionAlpha_ >= 1.0f)
            {
                transitionAlpha_ = 1.0f;
                isTransitioning = false;
            }

            if (previous.state)
            {
                SamplePose(previous.state, previous.time, previous.sampleDuration, tempPoseB, 0);
            }
        }
    }



    // ------------------------------------------------------------
     // 5. RootMotion extraction
     // 現在のRootノード姿勢と前フレーム姿勢の差から、GameObjectへ渡す移動・回転差分を求める。
     // ------------------------------------------------------------
    currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    // ステート指定のRootMotionノードを優先し、未指定時はモデル設定または"root"ノードを使用する。
    auto resolveRootNodeIndex_ = [&](const AnimState* state_)
        {
            // 演出の移動固定は、GameObjectへのRootMotion適用とは独立して処理する。
            if (!state_ || (!state_->rootMotion.enable && !state_->rootMotion.cutVisualWhenDisabled)) return -1;

            const std::string rootName_ = state_->rootMotion.sourceNodeName.empty()
                ? model->GetRootMotionName()
                : state_->rootMotion.sourceNodeName;

            if (!rootName_.empty())
            {
                const int rootNodeIndex_ = model->GetNodeIndex(rootName_.c_str());
                if (rootNodeIndex_ != -1)
                {
                    return rootNodeIndex_;
                }
            }

            return state_->rootMotion.sourceNodeName.empty()
                ? model->GetNodeIndex("root")
                : -1;
        };

    const int currentRootNodeIndex_ = resolveRootNodeIndex_(current.state);
    const int previousRootNodeIndex_ = isTransitioning ? resolveRootNodeIndex_(previous.state) : -1;
    const int defaultRootNodeIndex_ = model->GetRootMotionName().empty()
        ? -1
        : model->GetNodeIndex(model->GetRootMotionName().c_str());
    const auto& nodes_ = model->GetNodes();
    // ノード姿勢からローカル行列を作り、Rootノードの親階層変換を算出するために使用する。
    auto makeNodeLocalMatrix_ = [](const GltfModelCommponennt::NodePose& pose_)
        {
            using namespace DirectX::SimpleMath;
            return Matrix::CreateScale(pose_.scale)
                * Matrix::CreateFromQuaternion(pose_.rotation)
                * Matrix::CreateTranslation(pose_.position);
        };
    // Rootノードより上の親階層をたどり、Root移動をモデル基準へ変換する行列を組み立てる。
    auto getParentGlobalMatrix_ = [&](const std::vector<GltfModelCommponennt::NodePose>& pose_, int nodeIndex_)
        {
            if (nodeIndex_ < 0 || nodeIndex_ >= static_cast<int>(nodes_.size()))
            {
                return DirectX::SimpleMath::Matrix::Identity;
            }

            const int parentIndex_ = nodes_[nodeIndex_].parentIndex;
            DirectX::SimpleMath::Matrix parentGlobalMatrix_ = DirectX::SimpleMath::Matrix::Identity;
            int currentIndex_ = parentIndex_;
            while (currentIndex_ >= 0 &&
                currentIndex_ < static_cast<int>(nodes_.size()) &&
                currentIndex_ < static_cast<int>(pose_.size()))
            {
                parentGlobalMatrix_ = parentGlobalMatrix_ * makeNodeLocalMatrix_(pose_[currentIndex_]);
                currentIndex_ = nodes_[currentIndex_].parentIndex;
            }

            return parentGlobalMatrix_;
        };

    if (currentRootNodeIndex_ != -1 && tempPoseA.size() > currentRootNodeIndex_)
    {
        using namespace DirectX::SimpleMath;

        const Vector3 rawPos_ = tempPoseA[currentRootNodeIndex_].position;
        const bool looped_ = (current.time < prevTime);
        const bool stateChanged_ = (_prevRootMotionState != current.state);
        const bool sourceNodeChanged_ = (previousRootMotionNodeIndex != currentRootNodeIndex_);
        const bool resetBasePosition_ = isFirstFrame || stateChanged_ || sourceNodeChanged_;
        // ステート固有倍率にカーブ値を掛け、時間帯ごとのRootMotion強度を決定する。
        float rootMotionScale_ = current.state->rootMotion.enable ? current.state->rootMotionScale : 0.0f;
        if (curvecomp && !current.state->rootMotionScaleCurveName.empty())
        {
            float normalizedTime_ = (baseDuration > 0.0f) ? (current.time / baseDuration) : 0.0f;
            normalizedTime_ = (std::max)(0.0f, (std::min)(1.0f, normalizedTime_));
            rootMotionScale_ *= curvecomp->GetValue(current.state->rootMotionScaleCurveName, normalizedTime_);
        }

        // 初回・ステート変更・抽出元ノード変更時は差分を出さず、比較用の基準値だけを更新する。
        if (isFirstFrame || stateChanged_ || sourceNodeChanged_)
        {
            prevRootPos = rawPos_;
            prevRootRotation = tempPoseA[currentRootNodeIndex_].rotation;
            prevRootYawDegrees = GetYawDegreesFromQuaternion(prevRootRotation);
            if (resetBasePosition_)
            {
                rootMotionBasePosition = rawPos_;
                rootMotionBaseRotation = prevRootRotation;
            }
            prevTime = current.time;
            _prevRootMotionState = current.state;
            previousRootMotionNodeIndex = currentRootNodeIndex_;
            isFirstFrame = false;
        }
        else
        {
            Vector3 deltaPos_;
            const Quaternion rawRotation_ = tempPoseA[currentRootNodeIndex_].rotation;
            // ループ境界では「前フレーム→終端」と「先頭→現在」の差分を合成し、移動の途切れを防ぐ。
            if (looped_)
            {
                const float loopEndTime_ = (std::max)(0.0f, baseDuration - 0.0001f);
                SamplePose(current.state, 0.0f, baseDuration, rootMotionLoopStartPose, 0);
                SamplePose(current.state, loopEndTime_, baseDuration, rootMotionLoopEndPose, 0);

                const Vector3 loopStartPos_ = rootMotionLoopStartPose[currentRootNodeIndex_].position;
                const Vector3 loopEndPos_ = rootMotionLoopEndPose[currentRootNodeIndex_].position;
                deltaPos_ = (loopEndPos_ - prevRootPos) + (rawPos_ - loopStartPos_);
            }
            else
            {
                deltaPos_ = rawPos_ - prevRootPos;
            }

            // ステート設定で許可されていない軸の移動差分を除外する。
            if (!current.state->rootMotion.applyPositionX) deltaPos_.x = 0.0f;
            if (!current.state->rootMotion.applyPositionY) deltaPos_.y = 0.0f;
            if (!current.state->rootMotion.applyPositionZ) deltaPos_.z = 0.0f;

            const Matrix parentGlobalMatrix_ = getParentGlobalMatrix_(tempPoseA, currentRootNodeIndex_);
            // Rootノードの親階層方向を反映し、最終的なローカルRootMotion移動差分として保存する。
            currentRootMotionDelta.deltaPosition =
                Vector3::TransformNormal(deltaPos_, parentGlobalMatrix_) * rootMotionScale_;
            const float rawYawDegrees_ = GetYawDegreesFromQuaternion(rawRotation_);
            const float deltaYawDegrees_ =
                NormalizeAngleDegrees(rawYawDegrees_ - prevRootYawDegrees) * rootMotionScale_;

              // Quaternion全体ではなくYaw差分だけを抽出し、キャラクターの水平回転へ使用する。
            currentRootMotionDelta.deltaRotation =
                Quaternion::CreateFromYawPitchRoll(
                    DirectX::XMConvertToRadians(deltaYawDegrees_),
                    0.0f,
                    0.0f);

            prevRootPos = rawPos_;
            prevRootRotation = rawRotation_;
            prevRootYawDegrees = rawYawDegrees_;
            prevTime = current.time;
            _prevRootMotionState = current.state;
            previousRootMotionNodeIndex = currentRootNodeIndex_;
        }
    }
    else
    {
        isFirstFrame = true;
        prevTime = 0.0f;
        _prevRootMotionState = nullptr;
        prevRootRotation = DirectX::SimpleMath::Quaternion::Identity;
        prevRootYawDegrees = 0.0f;
        previousRootMotionNodeIndex = -1;
        currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
        currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    }

    auto resetVisualRootPosition_ = [&](
        std::vector<GltfModelCommponennt::NodePose>& pose_,
        const AnimState* state_,
        int nodeIndex_,
        const DirectX::SimpleMath::Vector3& basePosition_)
        {
            if (nodeIndex_ < 0 || pose_.size() <= nodeIndex_) return;

            const bool resetX_ = !state_ || state_->rootMotion.applyPositionX || state_->rootMotion.cutVisualPositionX;
            const bool resetY_ = !state_ || state_->rootMotion.applyPositionY || state_->rootMotion.cutVisualPositionY;
            const bool resetZ_ = !state_ || state_->rootMotion.applyPositionZ || state_->rootMotion.cutVisualPositionZ;
            if (resetX_) pose_[nodeIndex_].position.x = basePosition_.x;
            if (resetY_) pose_[nodeIndex_].position.y = basePosition_.y;
            if (resetZ_) pose_[nodeIndex_].position.z = basePosition_.z;
        };

    auto resetVisualRootRotation_ = [&](
        std::vector<GltfModelCommponennt::NodePose>& pose_,
        const AnimState* state_,
        int nodeIndex_,
        const DirectX::SimpleMath::Quaternion& baseRotation_)
        {
            if (!state_ || nodeIndex_ < 0 || pose_.size() <= nodeIndex_) return;

            if (state_->rootMotion.applyRotationX ||
                state_->rootMotion.applyRotationY ||
                state_->rootMotion.applyRotationZ)
            {
                pose_[nodeIndex_].rotation = baseRotation_;
                pose_[nodeIndex_].rotation.Normalize();
            }
        };
    // GameObjectへ移動を適用した分だけモデルRootを基準姿勢へ戻し、二重移動を防ぐ。
    auto applyVisualRootMotionCut_ = [&](
        std::vector<GltfModelCommponennt::NodePose>& pose_,
        const AnimState* state_,
        int sourceNodeIndex_,
        const DirectX::SimpleMath::Vector3& basePosition_,
        const DirectX::SimpleMath::Quaternion& baseRotation_)
        {
            if (!state_ || sourceNodeIndex_ < 0 || pose_.size() <= sourceNodeIndex_) return;

            if (sourceNodeIndex_ == defaultRootNodeIndex_)
            {
                resetVisualRootPosition_(pose_, state_, sourceNodeIndex_, basePosition_);
                resetVisualRootRotation_(pose_, state_, sourceNodeIndex_, baseRotation_);
                return;
            }

            using namespace DirectX::SimpleMath;
            Matrix parentGlobalMatrix_ = getParentGlobalMatrix_(pose_, sourceNodeIndex_);
            Matrix inverseParentGlobalMatrix_ = parentGlobalMatrix_.Invert();
            Vector3 currentGlobalPosition_ = Vector3::Transform(pose_[sourceNodeIndex_].position, parentGlobalMatrix_);
            const Vector3 baseGlobalPosition_ = Vector3::Transform(basePosition_, parentGlobalMatrix_);

            if (state_->rootMotion.applyPositionX || state_->rootMotion.cutVisualPositionX) currentGlobalPosition_.x = baseGlobalPosition_.x;
            if (state_->rootMotion.applyPositionY || state_->rootMotion.cutVisualPositionY) currentGlobalPosition_.y = baseGlobalPosition_.y;
            if (state_->rootMotion.applyPositionZ || state_->rootMotion.cutVisualPositionZ) currentGlobalPosition_.z = baseGlobalPosition_.z;

            pose_[sourceNodeIndex_].position = Vector3::Transform(currentGlobalPosition_, inverseParentGlobalMatrix_);
            resetVisualRootRotation_(pose_, state_, sourceNodeIndex_, baseRotation_);
        };

    {
        BS_PROFILE_SCOPE("ModelAnimator::ApplyPose");
        applyVisualRootMotionCut_(
            tempPoseA,
            current.state,
            currentRootNodeIndex_,
            rootMotionBasePosition,
            rootMotionBaseRotation);
        if (isTransitioning && previous.state)
        {
            applyVisualRootMotionCut_(
                tempPoseB,
                previous.state,
                previousRootNodeIndex_,
                previousRootMotionBasePosition,
                previousRootMotionBaseRotation);
            for (size_t nodeIndex_ = 0; nodeIndex_ < nodeCount; ++nodeIndex_)
            {
                finalPose[nodeIndex_].position = DirectX::SimpleMath::Vector3::Lerp(
                    tempPoseB[nodeIndex_].position,
                    tempPoseA[nodeIndex_].position,
                    transitionAlpha_);
                finalPose[nodeIndex_].rotation = DirectX::SimpleMath::Quaternion::Slerp(
                    tempPoseB[nodeIndex_].rotation,
                    tempPoseA[nodeIndex_].rotation,
                    transitionAlpha_);
                finalPose[nodeIndex_].rotation.Normalize();
                finalPose[nodeIndex_].scale = DirectX::SimpleMath::Vector3::Lerp(
                    tempPoseB[nodeIndex_].scale,
                    tempPoseA[nodeIndex_].scale,
                    transitionAlpha_);
            }
        }
        else
        {
            finalPose = tempPoseA;
        }
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

void ModelAnimator::CrossFadeTo(const AnimState* newState, float duration, std::shared_ptr<GameObject> owner, bool restart_, float startTime_)
{
    if (!newState) return;
    if (current.state == newState && !restart_) return;

    // 同じStateの再開始でも、旧コンボの未発生予兆を新しい区間へ引き継がない。
    attackTelegraphs.Reset();

    CallStateExitEndActions(current, owner);
    previousRootMotionBasePosition = rootMotionBasePosition;
    previousRootMotionBaseRotation = rootMotionBaseRotation;
    previous = current;

    // 同じStateの再開始も新しい移動区間として扱い、前回の終端からの差分を持ち越さない。
    isFirstFrame = true;
    _prevRootMotionState = nullptr;
    previousRootMotionNodeIndex = -1;
    prevTime = 0.0f;
    currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;

    current.state = newState;
    // 再生開始位置は新しい区間の基準とし、前段の移動差分や補間解除を持ち込まない。
    current.time = (std::max)(0.0f, startTime_);
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
void ModelAnimator::ActiveSlot::Update(float et, float duration, CurveComponent* curveComp, std::shared_ptr<GameObject> owner, AttackTelegraphRuntime* telegraphs_)
{
    if (!state) return;

    const AnimState* updatingState_ = state;

    float currentSpeed = state->playbackSpeed;

    // ★カーブの指定があり、コンポーネントが存在する場合
    // Trackが無いActorでは設定済み再生速度へ戻し、GetValueの既定値0による停止を防ぐ。
    if (curveComp && !state->speedCurveName.empty() && curveComp->HasTrack(state->speedCurveName))
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

    // 速度カーブ・イベント速度・TimeScaleを反映済みの、攻撃判定と同じ時間で予兆を判定する。
    if (telegraphs_) telegraphs_->Advance(owner, *state, prevTime, time);

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
