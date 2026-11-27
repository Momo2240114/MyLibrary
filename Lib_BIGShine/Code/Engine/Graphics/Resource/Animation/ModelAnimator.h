#pragma once

#include "Engine/Graphics/Resource/GlTFModel.h"
#include "AnimationState.h"
#include "Engine/Math/Curve/CurveComponent.h"
#include "PoseLibraryResource.h"
#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"
#include "Engine/GamePlay/Combat/AttackTelegraphRuntime.h"

#include <unordered_map>


class GameObject;

class ModelAnimator
{
public:
    // 現在計算中のポーズデータ
    struct ActiveSlot 
    {
        // 再生中のAnimation State。nullptrならこのSlotは未使用。
        const AnimState* state = nullptr;

        // state内での現在再生時間。
        float time = 0.0f;
        float weight = 1.0f; // ブレンド重み

        // 直近サンプルに使ったAnimation尺。外部表示や同期に使う。
        float sampleDuration = 0.0f;


        // 再生時間を進め、Curve/Blackboardによる速度補正も反映する。
        void Update(float et, float duration, CurveComponent* curvecomp, std::shared_ptr<GameObject> owner, AttackTelegraphRuntime* telegraphs_ = nullptr, const BlackboardCategory* blackboard_ = nullptr, float* advancedTime_ = nullptr);
    };

    struct RootMotionDelta {
        // このフレームにRoot Motionから発生した移動差分。
        DirectX::SimpleMath::Vector3 deltaPosition = { 0, 0, 0 };

        // このフレームにRoot Motionから発生した回転差分。
        DirectX::SimpleMath::Quaternion deltaRotation = DirectX::SimpleMath::Quaternion::Identity;
    };

    const RootMotionDelta& GetRootMotionDelta() const
    {
        return currentRootMotionDelta;
    }

    void Update(
        float deltaTime_,
        GltfModelCommponennt* model_,
        CurveComponent* curveComponent_,
        const std::shared_ptr<PoseLibraryResource>& poseLibrary_ = nullptr,
        std::shared_ptr<GameObject> owner_ = nullptr,
        const BlackboardCategory* blackboard_ = nullptr,
        const std::unordered_map<std::string, std::unique_ptr<AnimState>>* states_ = nullptr);

    // ModelAnimator.h
    // 2つのAnimation Indexを指定して、alphaで手動ブレンドした姿勢を作る。
    void UpdateBlend(float et, GltfModelCommponennt* model, int indexA, int indexB, float alpha, bool loop, float speed);

    // ノードエディタ側から呼ばれる命令
    // 現在のStateをpreviousへ退避し、新しいStateへduration秒でCrossFadeする。
    // 同じAnimationを続けるPhaseではrestart_で終了処理とEvent初期化をやり直す。
    void CrossFadeTo(const AnimState* newState, float duration, std::shared_ptr<GameObject> owner = nullptr, bool restart_ = false, float startTime_ = 0.0f);

    const std::vector<GltfModelCommponennt::NodePose>& GetCurrentPose() const
    {
        return finalPose;
    }

    float GetCurrentFadeTime() const
    {
        return current.time;
    }

    float GetCurrentSampleDuration() const
    {
        return current.sampleDuration;
    }

    // AnimationEventやEditorのシークから、現在Stateの再生時間を直接設定する。
    void SetCurrentTime(float time_);

    const AnimState* GetCurrentState() const
    {
        return current.state;
    }

    // Controllerが最新のブレンド姿勢をModelへ反映してから予兆を発生させる。
    // 判定窓をまたいだフレームも、途中の武器姿勢をサンプリングして判定へ渡す。
    void SampleWeaponHitBoxes(const std::shared_ptr<GameObject>& owner_, GltfModelCommponennt* model_);

    void FlushAttackTelegraphs(const std::shared_ptr<GameObject>& owner_)
    {
        attackTelegraphs.Flush(owner_);
    }

    // グラフの再生を維持した中断では、現在区間の予兆予約だけを停止する。
    void CancelAttackTelegraphs()
    {
        attackTelegraphs.Cancel(current.state);
    }

private:
    // Animatorごとに保持し、同じState資産を使う別の敵との発生履歴共有を避ける。
    AttackTelegraphRuntime attackTelegraphs;
    // カスタムアニメーション（ポーズの系譜）から一本のポーズを編み出す秘術
    void ComputeCustomAnimationPose(
        const CustomAnimation& customAnim,
        float time,
        const std::vector<SavedPose>& poseLibrary,
        std::vector<GltfModelCommponennt::NodePose>& outPose,
        GltfModelCommponennt* model
    );
    RootMotionDelta currentRootMotionDelta;

    // currentはフェードイン中の姿勢、previousはフェードアウト中の姿勢。
    const AnimState* weaponSampleState = nullptr;
    float weaponSampleStartTime = 0.0f;
    float weaponSampleEndTime = 0.0f;
    ActiveSlot current;
    ActiveSlot previous; // フェードアウト用

    // 前フレームの座標を保持する変数
    // Root Motion差分を「前回Root姿勢との差」として計算するための履歴。
    DirectX::SimpleMath::Vector3 prevRootPos = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion prevRootRotation = DirectX::SimpleMath::Quaternion::Identity;
    float prevRootYawDegrees = 0.0f;
    DirectX::SimpleMath::Vector3 rootMotionBasePosition = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion rootMotionBaseRotation = DirectX::SimpleMath::Quaternion::Identity;
    DirectX::SimpleMath::Vector3 previousRootMotionBasePosition = { 0, 0, 0 };
    DirectX::SimpleMath::Quaternion previousRootMotionBaseRotation = DirectX::SimpleMath::Quaternion::Identity;
    int previousRootMotionNodeIndex = -1;
    // アニメーションが切り替わったことを検知するフラグ
    // 初回フレームは差分を出さず、基準姿勢だけを記録する。
    bool isFirstFrame = true;

    float prevTime = 0.0f;
    float fadeTimer = 0.0f;
    float fadeDuration = 0.0f;
    bool isMixing = false;

    // CrossFade中かどうか。trueの間はcurrentとpreviousをweightで混ぜる。
    bool isTransitioning = false;
    float manualBlendTime = 0.0f;
    GltfModelCommponennt::NodePose prevRootPose;
    const AnimState* _prevRootMotionState = nullptr;
    bool usePreviousBlendedPose = false;

    // 最終的にModelへ渡す姿勢と、ブレンド計算用の一時配列。
    std::vector<GltfModelCommponennt::NodePose> finalPose;
    std::vector<GltfModelCommponennt::NodePose> tempPoseA;
    std::vector<GltfModelCommponennt::NodePose> tempPoseB;
    std::vector<GltfModelCommponennt::NodePose> blendPoseScratch;
    std::vector<GltfModelCommponennt::NodePose> rootMotionLoopStartPose;
    std::vector<GltfModelCommponennt::NodePose> rootMotionLoopEndPose;
};
