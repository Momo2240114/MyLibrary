#pragma once

#include "PlayerState.h"

class CameraComponent;
class CameraLockOnComponent;
class GameObject;

// 攻撃Actionを実行し、攻撃中の向き補正・位置補正・コンボ補助を扱うState。
class PlayerAttackState : public PlayerState
{
public:
    PlayerAttackState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_,
        PLAYER_ACTION action_);

    const char* GetName() const override { return GetPlayerActionName(action); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

private:
    // ロックオン、入力方向、カメラ正面、距離を評価して攻撃補正対象を探す。
    std::shared_ptr<GameObject> FindAssistTarget();
    // 攻撃開始時点の補正対象と基準方向を確定する。
    void ResolveAttackAssistTarget();
    // 攻撃中の向き補正と位置補正をまとめて適用する。
    void ApplyAttackAssist(float elapsedTime_);
    // 攻撃中の向きをTarget方向へ補間する。a
    void ApplyAttackFacingAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    // 攻撃が届きやすい距離まで水平移動を補助する。
    void ApplyAttackPositionAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    // Scene内のCameraLockOnComponentを取得し、次回以降のために保持する。
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();
    // 現在攻撃Animationの正規化時間を返す。
    float GetAttackNormalizedTime();
    // 反撃専用の広めなMotionWarpを開始する。
    void StartCounterMotionWarp();
    // 反撃終了時にMotionWarpを停止する。
    void StopCounterMotionWarp();

    // このStateが担当する攻撃Action種別。
    PLAYER_ACTION action = PLAYER_ACTION::NONE;
    // ロックオン状態を参照するためのCameraLockOnComponent。
    std::weak_ptr<CameraLockOnComponent> cameraLockOnComponent;
    // 今回の攻撃補正対象。
    std::weak_ptr<GameObject> attackAssistTarget;
    // 攻撃開始時のPlayer前方方向。
    DirectX::XMFLOAT3 attackStartForward{ 0.0f, 0.0f, 1.0f };
    // 攻撃中に基準として保持する補正方向。
    DirectX::XMFLOAT3 lockedAssistDirection{ 0.0f, 0.0f, 1.0f };
    // 1回の攻撃中に既に補正したYaw角度の合計。
    float accumulatedFacingCorrectionDegrees = 0.0f;
    // Animation時間を取れない場合に使う攻撃経過時間。
    float attackElapsedTime = 0.0f;
    // lockedAssistDirectionが有効な基準方向を持っているか。
    bool hasLockedAssistDirection = false;
    // 攻撃補正Targetの解決を済ませたか。
    bool attackAssistTargetResolved = false;
    // この攻撃Stateで反撃用MotionWarpを開始したか。
    bool counterMotionWarpActive = false;
};
