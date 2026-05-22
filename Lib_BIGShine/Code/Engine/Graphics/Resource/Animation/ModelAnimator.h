#pragma once

#include "Engine/Graphics/Resource/GlTFModel.h"
#include "AnimationState.h"
#include "Engine/Math/Curve/CurveComponent.h"

class ModelAnimator
{
public:
    // 現在計算中のポーズデータ
    struct ActiveSlot 
    {
        const AnimState* state = nullptr;
        float time = 0.0f;
        float weight = 1.0f; // ブレンド重み


        void Update(float et, float duration, CurveComponent* curvecomp);
    };


    void Update(float deltaTime, GltfModelCommponennt* model, CurveComponent* curvecomp);

    // ModelAnimator.h
    void UpdateBlend(float et, GltfModelCommponennt* model, int indexA, int indexB, float alpha, bool loop, float speed);

    // ノードエディタ側から呼ばれる命令
    void CrossFadeTo(const AnimState* newState, float duration);

    const std::vector<GltfModelCommponennt::NodePose>& GetCurrentPose() const { return finalPose; }
    float GetCurrentFadeTime() const { return current.time; }
    const AnimState* GetCurrentState() const { return current.state; }
private:
    ActiveSlot current;
    ActiveSlot previous; // フェードアウト用

    float fadeTimer = 0.0f;
    float fadeDuration = 0.0f;
    bool isMixing = false;

    std::vector<GltfModelCommponennt::NodePose> finalPose;
    std::vector<GltfModelCommponennt::NodePose> tempPoseA;
    std::vector<GltfModelCommponennt::NodePose> tempPoseB;
};