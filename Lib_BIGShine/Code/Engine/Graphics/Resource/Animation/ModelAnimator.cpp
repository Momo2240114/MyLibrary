#include "ModelAnimator.h"

void ModelAnimator::Update(float et, GltfModelCommponennt* model)
{
    if (!model || !current.state) return;

    // 1. 現在のアニメーションの時間を更新
    float currentDuration = model->GetAnimationDuration(current.state->animationIndex);
    current.Update(et, currentDuration);
    model->ComputeAnimation(current.state->animationIndex, current.time, tempPoseA);

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
            previous.Update(et, prevDuration);
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

void ModelAnimator::ActiveSlot::Update(float et, float duration)
{
    if (!state) return;

    float prevTime = time;
    time += et * state->playbackSpeed;

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
