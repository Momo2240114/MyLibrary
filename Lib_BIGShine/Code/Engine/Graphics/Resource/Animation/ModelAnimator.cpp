#include "ModelAnimator.h"

void ModelAnimator::Update(float et, GltfModelCommponennt* model, CurveComponent* curvecomp)
{
    // 1. 時間の更新（イベント発火含む）
      // ※ブレンド時は基準となる A の Duration を使う例
    float baseDuration = model->GetAnimationDuration(
        current.state->type == AnimStateType::Single ? current.state->animationIndex : current.state->animIndexA
    );
    current.Update(et, baseDuration, curvecomp);

    // 2. ポーズの計算
    if (current.state->type == AnimStateType::Single) {
        model->ComputeAnimation(current.state->animationIndex, current.time, tempPoseA);
    }
    else if (current.state->type == AnimStateType::Blend1D) {
        // --- ブレンド計算 ---
        // Blackboard 等から alpha を取得する仕組みが必要（後述）
        float alpha = 0.5f; // 仮：本来は Controller の Blackboard から取得

        model->ComputeAnimation(current.state->animIndexA, current.time, tempPoseA);

        // B の時間は A と同期させる（Duration比を考慮）
        float durationB = model->GetAnimationDuration(current.state->animIndexB);
        float timeB = (current.time / baseDuration) * durationB;
        model->ComputeAnimation(current.state->animIndexB, timeB, tempPoseB);

        // tempPoseA をブレンド結果で上書き
        for (size_t i = 0; i < tempPoseA.size(); ++i) {
            DirectX::XMVECTOR pA = DirectX::XMLoadFloat3(&tempPoseA[i].position);
            DirectX::XMVECTOR pB = DirectX::XMLoadFloat3(&tempPoseB[i].position);
            DirectX::XMStoreFloat3(&tempPoseA[i].position, DirectX::XMVectorLerp(pA, pB, alpha));

            DirectX::XMVECTOR qA = DirectX::XMLoadFloat4(&tempPoseA[i].rotation);
            DirectX::XMVECTOR qB = DirectX::XMLoadFloat4(&tempPoseB[i].rotation);
            DirectX::XMStoreFloat4(&tempPoseA[i].rotation, DirectX::XMQuaternionSlerp(qA, qB, alpha));

            DirectX::XMVECTOR sA = DirectX::XMLoadFloat3(&tempPoseA[i].scale);
            DirectX::XMVECTOR sB = DirectX::XMLoadFloat3(&tempPoseB[i].scale);
            DirectX::XMStoreFloat3(&finalPose[i].scale, DirectX::XMQuaternionSlerp(sA, sB, alpha));
        }
    }

    // 2. クロスフェード（ミキシング）処理
    if (isMixing && previous.state)
    {
        fadeTimer += et;
        float t = fadeTimer / fadeDuration;

        if (t >= 1.0f)
        {
            // フェード完了
            isMixing = false;
            previous.state = nullptr;
            finalPose = tempPoseA;
        }
        else
        {
            // 前のアニメーションを計算
            float prevDuration = model->GetAnimationDuration(previous.state->animationIndex);
            previous.Update(et, prevDuration,curvecomp);
            model->ComputeAnimation(previous.state->animationIndex, previous.time, tempPoseB);

            // tempPoseB(前) と tempPoseA(次) をブレンドして finalPose に格納
            if (finalPose.size() != tempPoseA.size()) finalPose.resize(tempPoseA.size());

            for (size_t i = 0; i < tempPoseA.size(); ++i)
            {
                // 位置の補間 (Lerp)
                DirectX::XMVECTOR pPrev = DirectX::XMLoadFloat3(&tempPoseB[i].position);
                DirectX::XMVECTOR pCurr = DirectX::XMLoadFloat3(&tempPoseA[i].position);
                DirectX::XMStoreFloat3(&finalPose[i].position, DirectX::XMVectorLerp(pPrev, pCurr, t));

                // 回転の補間 (Slerp)
                DirectX::XMVECTOR qPrev = DirectX::XMLoadFloat4(&tempPoseB[i].rotation);
                DirectX::XMVECTOR qCurr = DirectX::XMLoadFloat4(&tempPoseA[i].rotation);
                DirectX::XMStoreFloat4(&finalPose[i].rotation, DirectX::XMQuaternionSlerp(qPrev, qCurr, t));

                // スケールの補間
                DirectX::XMVECTOR sPrev = DirectX::XMLoadFloat3(&tempPoseB[i].scale);
                DirectX::XMVECTOR sCurr = DirectX::XMLoadFloat3(&tempPoseA[i].scale);
                DirectX::XMStoreFloat3(&finalPose[i].scale, DirectX::XMVectorLerp(sPrev, sCurr, t));
            }
        }
    }
    else
    {
        // 単一アニメーション再生時
        finalPose = tempPoseA;
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
}

void ModelAnimator::ActiveSlot::Update(float et, float duration,CurveComponent* curveComp)
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
                ActionRegistry::Instance().Call(ev.startAction);
            }
            ev.isFired = true;
        }

        // 2. 更新イベント (範囲内にいる間ずっと)
        if (time >= ev.startTime && time <= ev.endTime)
        {
            if (!ev.updateAction.empty()) {
                ActionRegistry::Instance().Call(ev.updateAction);
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
