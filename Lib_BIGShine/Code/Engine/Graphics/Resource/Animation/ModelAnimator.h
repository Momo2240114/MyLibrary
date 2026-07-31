#pragma once

#include "Engine/Graphics/Resource/GlTFModel.h"
#include "AnimationState.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "PoseLibraryResource.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"



class GameObject;

class ModelAnimator
{
public:
    // 現在計算中のポーズデータ
    struct ActiveSlot 
    {
        const AnimState* state = nullptr;
        float time = 0.0f;
        float weight = 1.0f; // ブレンド重み
        float sampleDuration = 0.0f;


        void Update(float et, float duration, CurveComponent* curvecomp, std::shared_ptr<GameObject> owner);
    };
    struct RootMotionDelta {
        DirectX::SimpleMath::Vector3 deltaPosition = { 0, 0, 0 };
        DirectX::SimpleMath::Quaternion deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    };

    const RootMotionDelta& GetRootMotionDelta() const { return currentRootMotionDelta; }

    void Update(
        float deltaTime_,
        GltfModelCommponennt* model_,
        CurveComponent* curveComponent_,
        const std::shared_ptr<PoseLibraryResource>& poseLibrary_ = nullptr,
        std::shared_ptr<GameObject> owner_ = nullptr,
        const BlackboardCategory* blackboard_ = nullptr);

    // ModelAnimator.h
    void UpdateBlend(float et, GltfModelCommponennt* model, int indexA, int indexB, float alpha, bool loop, float speed);

    // ノードエディタ側から呼ばれる命令
    void CrossFadeTo(const AnimState* newState, float duration, std::shared_ptr<GameObject> owner = nullptr);

    const std::vector<GltfModelCommponennt::NodePose>& GetCurrentPose() const { return finalPose; }
    float GetCurrentFadeTime() const { return current.time; }
    float GetCurrentSampleDuration() const { return current.sampleDuration; }
    void SetCurrentTime(float time_);
    const AnimState* GetCurrentState() const { return current.state; }
private:
    // カスタムアニメーション（ポーズの系譜）から一本のポーズを編み出す秘術
    void ComputeCustomAnimationPose(
        const CustomAnimation& customAnim,
        float time,
        const std::vector<SavedPose>& poseLibrary,
        std::vector<GltfModelCommponennt::NodePose>& outPose,
        GltfModelCommponennt* model
    );
    RootMotionDelta currentRootMotionDelta;
    ActiveSlot current;
    ActiveSlot previous; // フェードアウト用

    // 前フレームの座標を保持する変数
    DirectX::SimpleMath::Vector3 prevRootPos = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion prevRootRotation = DirectX::SimpleMath::Quaternion::Identity;
    float prevRootYawDegrees = 0.0f;
    DirectX::SimpleMath::Vector3 rootMotionBasePosition = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion rootMotionBaseRotation = DirectX::SimpleMath::Quaternion::Identity;
    DirectX::SimpleMath::Vector3 previousRootMotionBasePosition = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion previousRootMotionBaseRotation = DirectX::SimpleMath::Quaternion::Identity;
    int previousRootMotionNodeIndex = -1;
    // アニメーションが切り替わったことを検知するフラグ
    bool isFirstFrame = true;

    float prevTime = 0.0f;
    float fadeTimer = 0.0f;
    float fadeDuration = 0.0f;
    bool isMixing = false;
    bool isTransitioning = false;
    float manualBlendTime = 0.0f;
    GltfModelCommponennt::NodePose prevRootPose;
    const AnimState* _prevRootMotionState = nullptr;
    bool hasPrevPose = false;
    std::vector<GltfModelCommponennt::NodePose> finalPose;
    std::vector<GltfModelCommponennt::NodePose> tempPoseA;
    std::vector<GltfModelCommponennt::NodePose> tempPoseB;
    std::vector<GltfModelCommponennt::NodePose> blendPoseScratch;
    std::vector<GltfModelCommponennt::NodePose> rootMotionLoopStartPose;
    std::vector<GltfModelCommponennt::NodePose> rootMotionLoopEndPose;
};

