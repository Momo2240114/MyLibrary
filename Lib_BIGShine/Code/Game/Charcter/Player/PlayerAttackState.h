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

    const char* GetName() const override
    {
        return GetPlayerActionName(action);
    }

    // 攻撃ごとの補正基準を確定し、攻撃種別に応じた移動と反撃用Warpを開始する。
    void Enter() override;
    // グラフが該当段を再生し始めてから、旧段の時間を使わずにWarpを開始する。
    void NotifyWeakAnimationStarted();
    // 補正対象への追従と入力受付を更新し、行動の派生はControllerへ渡す。
    void Execute(float elapsedTime_) override;
    // この攻撃で開始したWarpを閉じ、コンボで維持する対象と終了する対象を整理する。
    void Exit() override;

private:
    // ロックオン、入力方向、カメラ正面、距離を評価して攻撃補正対象を探す。
    std::shared_ptr<GameObject> FindAssistTarget();
    // 攻撃開始時点の補正対象と基準方向を確定する。
    void ResolveAttackAssistTarget();
    // 攻撃中の向き補正と位置補正をまとめて適用する。
    void ApplyAttackAssist(float elapsedTime_);
    void UpdateRushApproach(float elapsedTime_);
    void UpdateRushStrike(float elapsedTime_);
    bool IsRushTargetValid(const std::shared_ptr<GameObject>& target_) const;
    float GetRushStopDistance(const std::shared_ptr<GameObject>& target_) const;
    // 攻撃中の向きをTarget方向へ補間する。
    void ApplyAttackFacingAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    // 攻撃が届きやすい距離まで水平移動を補助する。
    void ApplyAttackPositionAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    // Scene内のCameraLockOnComponentを取得し、次回以降のために保持する。
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();
    // 現在攻撃Animationの正規化時間を返す。
    float GetAttackNormalizedTime();
    // 弱コンボの判定前区間と予定移動量を既存のWarpへ渡す。
    void PrepareWeakComboMotionWarp();
    void StartWeakComboMotionWarp();
    // 生存状態、到達可能距離、壁を確認して同じ段の対象を固定する。
    bool IsWeakComboTargetValid(const std::shared_ptr<GameObject>& target_, bool checkFacing_) const;
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
    bool rushApproaching = false;
    bool rushApproachHasTarget = false;
    // 各段の実行情報は保存せず、次段・中断の境界で作り直す。
    std::string weakMotionWarpPresetName;
    DirectX::XMFLOAT3 weakPlannedRootMotion{ 0.0f, 0.0f, 0.0f };
    float weakWarpEndNormalizedTime = 0.0f;
    float weakStopDistance = 1.2f;
};
