#include "ModelAnimator.h"
#include "Engine/GamePlay/Core/Object/GameObject.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include <algorithm>
#include <cmath>

static void CallAnimationAction(const std::shared_ptr<GameObject>& owner, const std::string& actionName)
{
    if (actionName.empty()) return;

    if (owner && owner->localRegistry.HasAction(actionName)) {
        owner->localRegistry.Call(actionName);
        return;
    }

    ActionRegistry::Instance().Call(actionName);
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
void ModelAnimator::SetCurrentTime(float time_)
{
    current.time = (std::max)(0.0f, time_);
    previous = current;
    isTransitioning = false;
    fadeTimer = 0.0f;
}

void ModelAnimator::Update(float et, GltfModelCommponennt* model, CurveComponent* curvecomp, const std::shared_ptr<PoseLibraryResource>& poseLib, std::shared_ptr<GameObject> owner)
{
    if (!current.state || !model) return;

    const size_t nodeCount = model->GetNodes().size();
    if (tempPoseA.size() != nodeCount) tempPoseA.resize(nodeCount);
    if (tempPoseB.size() != nodeCount) tempPoseB.resize(nodeCount);
    if (finalPose.size() != nodeCount) finalPose.resize(nodeCount);

    // ------------------------------------------------------------
    // 補助: 状態から pose をサンプルする
    // ------------------------------------------------------------
    auto SamplePose = [&](const AnimState* state, float time, std::vector<GltfModelCommponennt::NodePose>& outPose)
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
                float alpha = 0.5f; // TODO: Blackboard 等から取得

                model->ComputeAnimation(state->animIndexA, time, outPose);

                float durationA = model->GetAnimationDuration(state->animIndexA);
                float durationB = model->GetAnimationDuration(state->animIndexB);
                float timeB = (durationA > 0.0f) ? (time / durationA) * durationB : 0.0f;

                model->ComputeAnimation(state->animIndexB, timeB, tempPoseB);

                for (size_t i = 0; i < nodeCount; ++i)
                {
                    outPose[i].position = DirectX::SimpleMath::Vector3::Lerp(outPose[i].position, tempPoseB[i].position, alpha);
                    outPose[i].rotation = DirectX::SimpleMath::Quaternion::Slerp(outPose[i].rotation, tempPoseB[i].rotation, alpha);
                    outPose[i].rotation.Normalize();
                    outPose[i].scale = DirectX::SimpleMath::Vector3::Lerp(outPose[i].scale, tempPoseB[i].scale, alpha);
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
        baseDuration = model->GetAnimationDuration(current.state->animIndexA);
    }

    // ------------------------------------------------------------
    // 2. 時間更新
    // ------------------------------------------------------------
    current.Update(et, baseDuration, curvecomp, owner);

    // ------------------------------------------------------------
    // 3. 現在ポーズ計算
    // ------------------------------------------------------------
    SamplePose(current.state, current.time, tempPoseA);

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
            SamplePose(previous.state, previous.time, tempPoseB);
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
    // 5. RootMotion 抽出
    // ------------------------------------------------------------
    currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;


    int rootNodeIdx = -1;
    const std::string rootName = model->GetRootMotionName();

    if (current.state->rootMotion.enable && !rootName.empty())
    {
        rootNodeIdx = model->GetNodeIndex(rootName.c_str());
    }

    if (rootNodeIdx != -1 && finalPose.size() > rootNodeIdx)
    {
        using namespace DirectX::SimpleMath;

        Vector3 rawPos = finalPose[rootNodeIdx].position;
        Quaternion rawRot = finalPose[rootNodeIdx].rotation;
        rawRot.Normalize();

        const bool looped = (current.time < _prevTime);
        const bool stateChanged = (_prevRootMotionState != current.state);

        if (_isFirstFrame || looped || stateChanged)
        {
            _prevRootPos = rawPos;
            _prevRootRot = rawRot;
            _prevTime = current.time;
            _prevRootMotionState = current.state;
            _isFirstFrame = false;
        }
        else
        {
            Vector3 deltaPos = rawPos - _prevRootPos;

            if (!current.state->rootMotion.applyPositionX) deltaPos.x = 0.0f;
            if (!current.state->rootMotion.applyPositionY) deltaPos.y = 0.0f;
            if (!current.state->rootMotion.applyPositionZ) deltaPos.z = 0.0f;

            currentRootMotionDelta.deltaPosition = deltaPos;



            _prevRootPos = rawPos;
            _prevRootRot = rawRot;
            _prevTime = current.time;
            _prevRootMotionState = current.state;
        }

        // Root ノードは位置も回転もモデル空間で固定
        finalPose[rootNodeIdx].position = Vector3::Zero;
      
        finalPose[rootNodeIdx].rotation *= model->GetRootRotationOffset();
        finalPose[rootNodeIdx].rotation.Normalize();
    }
    else
    {
        // RootMotion 無効時は内部状態をリセット
        _isFirstFrame = true;
        _prevTime = 0.0f;
        _prevRootMotionState = nullptr;
        currentRootMotionDelta.deltaPosition = DirectX::SimpleMath::Vector3::Zero;
        currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    }
}

void ModelAnimator::UpdateBlend(float et, GltfModelCommponennt* model, int indexA, int indexB, float alpha, bool loop, float speed)
{
    if (!model) return;

    // 1. 各アニメーションの時間を進める (簡易化のため共通の時間管理)
    static float internalTime = 0.0f;
    float durationA = model->GetAnimationDuration(indexA);
    internalTime += et * speed;
    if (loop) internalTime = fmod(internalTime, durationA);

    // 2. AとBのポーズをそれぞれ計算
    model->ComputeAnimation(indexA, internalTime, tempPoseA);
    // BはAの時間に合わせるか、B自身のDurationで正規化し直す
    float durationB = model->GetAnimationDuration(indexB);
    float timeB = (internalTime / durationA) * durationB;
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
        DirectX::XMStoreFloat3(&finalPose[i].scale, DirectX::XMQuaternionSlerp(sA, sB, alpha));
    }
}

void ModelAnimator::CrossFadeTo(const AnimState* newState, float duration)
{
    if (!newState) return;
    if (current.state == newState) return;

    previous = current;

    current.state = newState;
    current.time = 0.0f;
    current.weight = 1.0f;

    // 新しいステートに切り替わる際、イベントフラグをリセットしておく
    for (auto& ev : const_cast<AnimState*>(current.state)->events) {
        ev.isFired = false;
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

    float currentSpeed = state->playbackSpeed;

    // ★カーブの指定があり、コンポーネントが存在する場合
    if (curveComp && !state->speedCurveName.empty())
    {
        // アニメーションの正規化時間 (0.0 - 1.0) を計算
        float normalizedTime = (duration > 0.0f) ? (time / duration) : 0.0f;

        // 指定された名前のカーブから速度を取得
        currentSpeed = curveComp->GetValue(state->speedCurveName, normalizedTime);
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
            if (!ev.startAction.empty()) {
                CallAnimationAction(owner, ev.startAction);
            }
            ev.isFired = true;
        }

        // 2. 更新イベント (範囲内にいる間ずっと)
        if (time >= ev.startTime && time <= ev.endTime)
        {
            if (!ev.updateAction.empty()) {
                CallAnimationAction(owner, ev.updateAction);
            }
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
            }
        }
        else {
            time = duration;
        }
    }
}


