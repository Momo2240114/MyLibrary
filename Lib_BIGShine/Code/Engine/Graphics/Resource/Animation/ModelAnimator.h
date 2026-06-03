#pragma once

#include "Engine/Graphics/Resource/GlTFModel.h"
#include "AnimationState.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "PoseLibraryResource.h" // 【追加】ポーズライブラリの定義を呼び出す



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


        void Update(float et, float duration, CurveComponent* curvecomp, std::shared_ptr<GameObject> owner);
    };
    struct RootMotionDelta {
        DirectX::SimpleMath::Vector3 deltaPosition = { 0, 0, 0 };
        DirectX::SimpleMath::Quaternion deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    };

    const RootMotionDelta& GetRootMotionDelta() const { return currentRootMotionDelta; }

    void Update(float deltaTime, GltfModelCommponennt* model, CurveComponent* curvecomp, const std::shared_ptr<PoseLibraryResource>& poseLib = nullptr, std::shared_ptr<GameObject> owner = nullptr);

    // ModelAnimator.h
    void UpdateBlend(float et, GltfModelCommponennt* model, int indexA, int indexB, float alpha, bool loop, float speed);

    // ノードエディタ側から呼ばれる命令
    void CrossFadeTo(const AnimState* newState, float duration);

    const std::vector<GltfModelCommponennt::NodePose>& GetCurrentPose() const { return finalPose; }
    float GetCurrentFadeTime() const { return current.time; }
    const AnimState* GetCurrentState() const { return current.state; }
private:
    // 【追加】カスタムアニメーション（ポーズの系譜）から一本のポーズを編み出す秘術
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
    DirectX::SimpleMath::Vector3 _prevRootPos = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion _prevRootRot = { 0, 0, 0 ,0};
    // アニメーションが切り替わったことを検知するフラグ
    bool _isFirstFrame = true;

    float _prevTime = 0.0f;
    float fadeTimer = 0.0f;
    float fadeDuration = 0.0f;
    bool isMixing = false;
    bool isTransitioning = false;
    GltfModelCommponennt::NodePose prevRootPose;
    const AnimState* _prevRootMotionState = nullptr;
    bool hasPrevPose = false;
    std::vector<GltfModelCommponennt::NodePose> finalPose;
    std::vector<GltfModelCommponennt::NodePose> tempPoseA;
    std::vector<GltfModelCommponennt::NodePose> tempPoseB;
};

