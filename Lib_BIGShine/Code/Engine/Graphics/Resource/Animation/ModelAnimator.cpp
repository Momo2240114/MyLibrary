#include "ModelAnimator.h"

void ModelAnimator::Update(float et, GltfModelCommponennt* model, CurveComponent* curvecomp, const std::shared_ptr<PoseLibraryResource>& poseLib)
{
    if (!current.state || !model) return;

    // ーーー 秘術の仕込み：過去の刻におけるルートノードの姿勢を記録 ーーー
    DirectX::SimpleMath::Vector3 prevRootPos;
    DirectX::SimpleMath::Quaternion prevRootRot;
    int rootNodeIdx = -1;

    std::string rootName = model->GetRootMotionName();
    if (!rootName.empty() && current.state->rootMotion.enable) {
        rootNodeIdx = model->GetNodeIndex(rootName.c_str());
        if (rootNodeIdx != -1) {
            // 単純化のため、ここでは「現在計算されている姿勢」を前フレームの姿として記憶する
            // （厳密には前の時間を元にComputeAnimationを叩く手もあるが、前回更新結果のfinalPoseを使えば容易い）
            if (finalPose.size() > rootNodeIdx) {
                prevRootPos = finalPose[rootNodeIdx].position;
                prevRootRot = finalPose[rootNodeIdx].rotation;
            }
        }
    }

    // --- 1. 時間の更新（イベント発火含む） ---
    float baseDuration = 1.0f;

    // 新たなる分岐：もし状態が「カスタムアニメーション」を求めており、ライブラリが存在するなら
    if (current.state->type == AnimStateType::Custom && poseLib && current.state->customAnimIndex >= 0)
    {
        if (current.state->customAnimIndex < (int)poseLib->customAnims.size()) {
            baseDuration = poseLib->customAnims[current.state->customAnimIndex].duration;
        }
    }
    else
    {
        // 通常のSingle / Blend1D の再生時間を取得
        baseDuration = model->GetAnimationDuration(
            current.state->type == AnimStateType::Single ? current.state->animationIndex : current.state->animIndexA
        );
    }

    // ループ処理などを考慮しつつ時間を進める
    current.Update(et, baseDuration, curvecomp);

    // --- 2. ポーズの計算 ---
    size_t nodeCount = model->GetNodes().size();
    if (tempPoseA.size() != nodeCount) tempPoseA.resize(nodeCount);
    if (tempPoseB.size() != nodeCount) tempPoseB.resize(nodeCount);
    if (finalPose.size() != nodeCount) finalPose.resize(nodeCount);

    // A) カスタムアニメーションの再生処理
    if (current.state->type == AnimStateType::Custom && poseLib && current.state->customAnimIndex >= 0)
    {
        if (current.state->customAnimIndex < (int)poseLib->customAnims.size()) {
            // ライブラリの記憶から、現在の時間に合わせたポーズをtempPoseAに抽出する
            ComputeCustomAnimationPose(poseLib->customAnims[current.state->customAnimIndex], current.time, poseLib->poseLibrary, tempPoseA, model);
        }
    }
    // B) 通常の単体アニメーション再生
    else if (current.state->type == AnimStateType::Single) {
        model->ComputeAnimation(current.state->animationIndex, current.time, tempPoseA);
    }
    // C) 1Dブレンドアニメーション再生
    else if (current.state->type == AnimStateType::Blend1D) {
        float alpha = 0.5f; // 本来はBlackboard等から取得
        model->ComputeAnimation(current.state->animIndexA, current.time, tempPoseA);

        float durationB = model->GetAnimationDuration(current.state->animIndexB);
        float timeB = (baseDuration > 0.0f) ? (current.time / baseDuration) * durationB : 0.0f;
        model->ComputeAnimation(current.state->animIndexB, timeB, tempPoseB);

        // tempPoseA と tempPoseB をブレンド
        for (size_t i = 0; i < nodeCount; ++i) {
            tempPoseA[i].position = DirectX::SimpleMath::Vector3::Lerp(tempPoseA[i].position, tempPoseB[i].position, alpha);
            tempPoseA[i].rotation = DirectX::SimpleMath::Quaternion::Lerp(tempPoseA[i].rotation, tempPoseB[i].rotation, alpha);
            tempPoseA[i].scale = DirectX::SimpleMath::Vector3::Lerp(tempPoseA[i].scale, tempPoseB[i].scale, alpha);
        }
    }

    // --- 3. クロスフェード（状態遷移）の処理 ---
    if (isTransitioning)
    {
        fadeTimer += et;
        float alpha = fadeTimer / fadeDuration;
        if (alpha >= 1.0f) {
            alpha = 1.0f;
            isTransitioning = false;
        }

        // 過去の遺志（previous）のポーズを tempPoseB に計算
        if (previous.state) {
            // 過去がカスタムだった場合
            if (previous.state->type == AnimStateType::Custom && poseLib && previous.state->customAnimIndex >= 0) {
                ComputeCustomAnimationPose(poseLib->customAnims[previous.state->customAnimIndex], previous.time, poseLib->poseLibrary, tempPoseB, model);
            }
            else {
                model->ComputeAnimation(previous.state->animationIndex, previous.time, tempPoseB);
            }
        }

        // 過去（tempPoseB）から現在（tempPoseA）へと肉体を滑らかに変化させる
        for (size_t i = 0; i < nodeCount; ++i) {
            finalPose[i].position = DirectX::SimpleMath::Vector3::Lerp(tempPoseB[i].position, tempPoseA[i].position, alpha);
            finalPose[i].rotation = DirectX::SimpleMath::Quaternion::Lerp(tempPoseB[i].rotation, tempPoseA[i].rotation, alpha);
            finalPose[i].scale = DirectX::SimpleMath::Vector3::Lerp(tempPoseB[i].scale, tempPoseA[i].scale, alpha);
        }
    }
    else
    {
        // 遷移中でなければ、現在のポーズがそのまま最終的な肉体となる
        finalPose = tempPoseA;
    }

    // ーーー 修正されたデルタ抽出と打ち消し処理 ーーー
    currentRootMotionDelta.deltaPosition = { 0, 0, 0 };
    currentRootMotionDelta.deltaRotation = DirectX::SimpleMath::Quaternion::Identity;

    if (rootNodeIdx != -1 && finalPose.size() > rootNodeIdx)
    {
        DirectX::SimpleMath::Vector3 currentRawPos = finalPose[rootNodeIdx].position;
        DirectX::SimpleMath::Quaternion currentRawRot = finalPose[rootNodeIdx].rotation;

        // ループ判定：時間が戻った場合は差分を0とする（もしくは補完する）
        bool isLooping = (current.time < _prevTime);

        if (_isFirstFrame || isLooping) {
            _prevRootPos = currentRawPos;
            _prevRootRot = currentRawRot;
            _isFirstFrame = false;
        }

        // --- 1. 位置の差分算出 ---
        DirectX::SimpleMath::Vector3 deltaPos = currentRawPos - _prevRootPos;

        // 軸封印
        if (!current.state->rootMotion.applyPositionX) deltaPos.x = 0.0f;
        if (!current.state->rootMotion.applyPositionY) deltaPos.y = 0.0f;
        if (!current.state->rootMotion.applyPositionZ) deltaPos.z = 0.0f;

        // --- 2. 回転の差分算出 (Quaternionの逆数を使って差分を抽出) ---
        DirectX::SimpleMath::Quaternion prevInv;
        _prevRootRot.Inverse(prevInv);
        DirectX::SimpleMath::Quaternion deltaRot = prevInv * currentRawRot;

        // 回転結果を格納
        currentRootMotionDelta.deltaPosition = deltaPos;
        currentRootMotionDelta.deltaRotation = deltaRot;

        // --- 3. 【重要】打ち消し ---
        // モデルのルートノードは「常に前フレームの姿勢」に戻すことでその場に固定される
        // ※ループ時は不連続を避けるため、現在の位置を前フレーム位置として保持し続けるのが最も安全だ
        finalPose[rootNodeIdx].position = _prevRootPos;
        finalPose[rootNodeIdx].rotation = _prevRootRot;

        // 4. 次フレームのための記憶更新
        if (!isLooping) {
            _prevRootPos = currentRawPos;
            _prevRootRot = currentRawRot;
        }

        _prevTime = current.time;
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
    float factor = (time - tL) / (tR - tL);

    int pIdxL = customAnim.keyframes[lKey].poseIndex;
    int pIdxR = customAnim.keyframes[rKey].poseIndex;

    if (pIdxL >= 0 && pIdxL < (int)poseLibrary.size() && pIdxR >= 0 && pIdxR < (int)poseLibrary.size()) {
        const auto& poseL = poseLibrary[pIdxL].nodePoses;
        const auto& poseR = poseLibrary[pIdxR].nodePoses;

        // 左のポーズと右のポーズを、算出された重みで線形補間（エルプ）する
        for (size_t i = 0; i < nodeCount; ++i) {
            if (i < poseL.size() && i < poseR.size()) {
                outPose[i].position = DirectX::SimpleMath::Vector3::Lerp(poseL[i].position, poseR[i].position, factor);
                outPose[i].rotation = DirectX::SimpleMath::Quaternion::Lerp(poseL[i].rotation, poseR[i].rotation, factor);
                outPose[i].scale = DirectX::SimpleMath::Vector3::Lerp(poseL[i].scale, poseR[i].scale, factor);
            }
        }
    }
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
