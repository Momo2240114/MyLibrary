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
    size_t nodeCount = model->GetNodes().size();
    if (outPose.size() != nodeCount) outPose.resize(nodeCount);

    // キーフレームが皆無、あるいはライブラリが空なら初期姿勢（バインドポーズ）を返す
    if (customAnim.keyframes.empty() || poseLibrary.empty()) {
        const auto& nodes = model->GetNodes();
        for (size_t i = 0; i < nodeCount; ++i) {
            // Nodeが記憶している初期のトランスフォームを、NodePoseの器へと写し取る
            outPose[i].position = nodes[i].position;
            outPose[i].rotation = nodes[i].rotation;
            outPose[i].scale = nodes[i].scale;
        }
        return;
    }

    // 1つしかキーフレームがない場合は、そのポーズをそのまま維持する
    if (customAnim.keyframes.size() == 1) {
        int idx = customAnim.keyframes[0].poseIndex;
        if (idx >= 0 && idx < (int)poseLibrary.size()) {
            outPose = poseLibrary[idx].nodePoses;
        }
        return;
    }

    // 現在の時間が、どの2つのキーフレームの間にあるかを探索する
    int lKey = -1;
    int rKey = -1;

    for (size_t i = 0; i < customAnim.keyframes.size() - 1; ++i) {
        if (time >= customAnim.keyframes[i].time && time <= customAnim.keyframes[i + 1].time) {
            lKey = static_cast<int>(i);
            rKey = static_cast<int>(i + 1);
            break;
        }
    }

    // もし時間の果て（末尾を越えた）に達していたなら、最後のキーフレームで固定
    if (lKey == -1) {
        if (time < customAnim.keyframes.front().time) {
            rKey = 0; lKey = 0;
        }
        else {
            lKey = static_cast<int>(customAnim.keyframes.size() - 1);
            rKey = lKey;
        }
    }

    // 二つの鍵（キーフレーム）が同じ、あるいは時間の差がないなら、そのポーズをそのまま写し取る
    if (lKey == rKey) {
        int pIdx = customAnim.keyframes[lKey].poseIndex;
        if (pIdx >= 0 && pIdx < (int)poseLibrary.size()) {
            outPose = poseLibrary[pIdx].nodePoses;
        }
        return;
    }

    // 二つのポーズの間を滑らかに繋ぐ重み（t）を算出する
    float tL = customAnim.keyframes[lKey].time;
    float tR = customAnim.keyframes[rKey].time;
    float rawFactor = (time - tL) / (tR - tL);
    float factor = ApplyPoseInterpolation(customAnim.keyframes[lKey].interpolation, rawFactor);

    int pIdxL = customAnim.keyframes[lKey].poseIndex;
    int pIdxR = customAnim.keyframes[rKey].poseIndex;

    if (pIdxL >= 0 && pIdxL < (int)poseLibrary.size() && pIdxR >= 0 && pIdxR < (int)poseLibrary.size()) {
        const auto& poseL = poseLibrary[pIdxL].nodePoses;
        const auto& poseR = poseLibrary[pIdxR].nodePoses;

        for (size_t i = 0; i < nodeCount; ++i) {
            if (i < poseL.size() && i < poseR.size()) {
                outPose[i].position = DirectX::SimpleMath::Vector3::Lerp(poseL[i].position, poseR[i].position, factor);
                outPose[i].rotation = DirectX::SimpleMath::Quaternion::Slerp(poseL[i].rotation, poseR[i].rotation, factor);
                outPose[i].rotation.Normalize();
                outPose[i].scale = DirectX::SimpleMath::Vector3::Lerp(poseL[i].scale, poseR[i].scale, factor);
            }
        }

        if (customAnim.naturalMotion.enabled)
        {
            const auto& settings = customAnim.naturalMotion;
            const auto& nodes = model->GetNodes();
            const float envelope = std::sin((std::max)(0.0f, (std::min)(1.0f, rawFactor)) * DirectX::XM_PI);
            const float blend = (std::max)(0.0f, (std::min)(1.0f, settings.blend));

            for (size_t i = 0; i < nodeCount && i < nodes.size(); ++i)
            {
                if (!MatchesNaturalMotionNode(settings.nodeFilter, nodes[i].name)) continue;

                const float wave = std::sin((time * settings.frequency + settings.phase + (float)i * 0.173f) * DirectX::XM_2PI);
                const float strength = wave * envelope * blend;

                if (settings.rotationAmplitudeDeg != 0.0f)
                {
                    const float rad = DirectX::XMConvertToRadians(settings.rotationAmplitudeDeg * strength);
                    auto addRot = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(rad * 0.25f, rad * 0.5f, rad);
                    DirectX::SimpleMath::Quaternion baseRot(outPose[i].rotation.x, outPose[i].rotation.y, outPose[i].rotation.z, outPose[i].rotation.w);
                    outPose[i].rotation = baseRot * addRot;
                    outPose[i].rotation.Normalize();
                }

                if (settings.positionAmplitude != 0.0f)
                {
                    outPose[i].position.y += settings.positionAmplitude * strength;
                }
            }
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


