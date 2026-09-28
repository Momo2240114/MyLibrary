#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "PlayerStateMachine.h"
#include <algorithm>
#include <vector>

class ColliderComponent;
class BlackboardComponent;
class CameraLockOnComponent;
class CharacterMovementComponent;
class HurtBox;
class HitBox;
class JustDodgeCollider;
class GuardCollider;

// PlayerAction中のキャンセル、先行入力、コンボ受付状態を保持する。
struct PlayerActionRuntime
{
    bool canDodgeCancel = true; // 回避Actionへ派生できるか。
    bool canActionBranch = true; // 強攻撃やSkillなど別Actionへ派生できるか。
    bool canBufferInput = false; // 次の入力を先行入力として保存できるか。
    bool canCombo = false; // コンボ遷移を受け付けるか。
    bool actionEnded = false; // Timeline駆動Actionが終了したか。
    PLAYER_ACTION bufferedAction = PLAYER_ACTION::NONE; // 保存中の先行入力Action。
    int bufferedComboIndex = 0; // 保存中のコンボ段番号。
};

// あるActionから別Actionへキャンセルできるルール。
struct PlayerCancelTransition
{
    PLAYER_ACTION from = PLAYER_ACTION::WEAK_ATTACK; // キャンセル元Action。
    PLAYER_ACTION to = PLAYER_ACTION::DODGE; // キャンセル先Action。
};

// 攻撃時にTargetへ向きや位置を補正するための設定。
struct PlayerAttackAssistSettings
{
    bool lockTargetOnAttackEnter = true;
    bool allowRetargetDuringAttack = false;
    bool nonLockOnUseNearestFallback = false;
    float nonLockOnMaximumTargetAngleDegrees = 50.0f;
    float lockOnMaximumTargetAngleDegrees = 180.0f;
    float maximumTotalRotationDegrees = 35.0f;
    float facingAssistStartNormalizedTime = 0.0f;
    float facingAssistFullStrengthEndNormalizedTime = 0.15f;
    float facingAssistEndNormalizedTime = 0.35f;
    float minimumInputMagnitude = 0.15f;
    bool preferMoveInputDirection = true;
    bool preferCameraForwardDirection = true;
    bool preserveTargetAcrossCombo = true;
    bool stopAssistWhenTargetInvalid = true;

    bool enabled = true; // 攻撃補正を有効にするか。
    float strength = 0.75f; // 向き補正の強さ。
    float turnSpeed = 10.0f; // 向き補正の追従速度。
    float searchDistance = 12.0f; // 補正対象を探す距離。
    float cameraCenterAngle = 30.0f; // カメラ中央方向を優先する角度。

    float positionStrength = 0.35f; // 位置補正の強さ。
    float positionSpeed = 4.0f; // 位置補正の追従速度。
    float maxPositionCorrection = 0.6f; // 1回の補正で動かす最大距離。
    float targetStopDistance = 1.2f; // Target手前で止まる距離。
};

// 入力、移動、StateMachine、Blackboardをまとめて制御するPlayer用Component。
class PlayerControllerComponent : public Component
{
public:
    // Component名を返す。
    const std::string& GetName() const override
    {
        static std::string name = "PlayerControllerComponent";
        return name;
    }

    void Start() override; // 必要ComponentとStateMachineを初期化する。
    void Update(float dt_) override; // State更新、入力受付、移動反映を行う。
    void Gui() override; // Player制御用のDebug UIを表示する。

    void InitializeBlackboardValues(); // Player/Anim用Blackboard値を初期化する。

    void Serialize(nlohmann::json& j_) const override; // Player設定をJSONへ保存する。
    void Deserialize(const nlohmann::json& j_) override; // JSONからPlayer設定を復元する。

    DirectX::XMFLOAT3 GetMoveInput() const; // カメラ基準の移動入力方向を取得する。
    DirectX::XMFLOAT3 GetGuardMoveInput(const DirectX::XMFLOAT3& worldMoveInput_) const;
    DirectX::XMFLOAT3 GetMoveInputAxis() const; // 生の移動入力Axisを取得する。
    DirectX::XMFLOAT3 GetCameraForwardDirection() const; // カメラの水平Forward方向を取得する。
    void SetFacingDirection(const DirectX::XMFLOAT3& direction_); // Playerの向きを指定方向へ向ける。
    void ApplyGuardFacingToTarget();
    void SetAttackAssistTarget(const std::shared_ptr<GameObject>& target_);
    std::shared_ptr<GameObject> GetAttackAssistTarget() const { return attackAssistTarget.lock(); }
    void ClearAttackAssistTarget() { attackAssistTarget.reset(); }
    void SetHorizontalVelocity(
        const DirectX::XMFLOAT3& dir_,
        float speed_,
        bool updateFacing_ = true); // 次フレームに反映する水平速度を設定する。
    void AddHorizontalVelocityAssist(const DirectX::XMFLOAT3& dir_, float speed_); // 予約済み水平速度へ補助速度を加算する。

    ColliderComponent* GetCollider() const { return collider.get(); } // PlayerのColliderを返す。
    CharacterMovementComponent* GetMovement() const { return movement.get(); } // Playerの移動Componentを返す。
    bool IsJustDodgeColliderActive() const;
    bool IsGrounded() const;
    bool IsJumpInputPressed() const;
    bool IsCurrentAnimationState(const std::string& stateName_);
    void BeginAirborneActionContext();
    void EndAirborneActionContext();
    bool ConsumeJumpLoopResumeRequest();
    PLAYER_ACTION GetCurrentAction() const { return stateMachine.GetCurrentAction(); } // 現在Actionを返す。
    int GetComboIndex() const { return comboIndex; } // 現在のコンボ段番号を返す。
    const std::vector<PlayerCancelTransition>& GetDodgeCancelTransitions() const { return dodgeCancelTransitions; }
    const std::vector<PlayerCancelTransition>& GetActionBranchTransitions() const { return actionBranchTransitions; } // Action間のキャンセルルール一覧を返す。
    const PlayerAttackAssistSettings& GetAttackAssistSettings() const { return attackAssistSettings; } // 攻撃補正設定を返す。

    bool RequestAction(PLAYER_ACTION nextAction_); // 指定Actionへの遷移を要求する。
    bool HasCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_) const; // 指定キャンセルルールが存在するか確認する。
    bool AddCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_); // キャンセルルールを追加する。
    bool RemoveCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_);
    bool HasDodgeCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_) const;
    bool AddDodgeCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_);
    bool RemoveDodgeCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_);
    bool HasActionBranchTransition(PLAYER_ACTION from_, PLAYER_ACTION to_) const;
    bool AddActionBranchTransition(PLAYER_ACTION from_, PLAYER_ACTION to_);
    bool RemoveActionBranchTransition(PLAYER_ACTION from_, PLAYER_ACTION to_); // キャンセルルールを削除する。
    void NotifyGuardedAttack(); // ガード成功時にジャストガードへ遷移させる。
    void SetCanCancel(bool enabled_); // 共通のActionBranch受付状態を設定する。
    void SetCanDodgeCancel(bool enabled_); // 回避キャンセル受付状態を設定する。
    void SetCanActionBranch(bool enabled_); // 攻撃やSkill派生受付状態を設定する。
    void SetCanBufferInput(bool enabled_); // 先行入力受付状態を設定する。
    void SetCanCombo(bool enabled_); // コンボ受付状態を設定する。
    void NotifyComboTransition(); // 保存済みコンボ入力へ遷移する。
    void NotifyActionEnd(); // Timeline駆動Actionの終了を通知する。
    void NotifyJumpLoop(); // Jump系AnimationからJumpLoopへ戻る通知を受ける。
    void NotifyJumpImpulse(); // Jump系Animationから踏み切りインパルス適用を受ける。
    void HandleHitReaction(); // 被弾時の状態リセットとHit遷移を行う。
    void SetRuntimeArmor(float armor_);
    void ResetRuntimeArmor();
    float GetCurrentArmor() const { return runtimeArmorOverrideActive ? runtimeArmor : armor; }
    bool ConsumeBufferedAction(); // 保存中の先行入力Actionを消費する。
    void EnterAction(PLAYER_ACTION action_); // Action開始時のRuntime状態を初期化する。
    void ProcessActionInput(); // 入力からAction要求を発行する。
    void SendMoveInputToBlackboard(const DirectX::XMFLOAT3& input_); // 移動入力をBlackboardへ送る。
    void SendGuardMoveInputToBlackboard(const DirectX::XMFLOAT3& input_);
    void SendGuardInputToBlackboard(bool guardInput_);
    void SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE phase_);
    bool TryDodgeCancelInput();

    void NotifyJustDodge(); // ジャスト回避成功を現在のDodgeStateへ通知する。

    float moveSpeed = 6.0f; // 通常移動速度。
    float guardMoveSpeed = 2.0f;
    float jumpImpulse = 8.5f;
    float doubleJumpImpulse = 8.0f;
    float airControlRate = 0.72f;
    float airControlAcceleration = 8.0f;
    float airDeceleration = 4.5f;
    float strongAttackHoldThreshold = 0.35f;
    float strongChargeAttackSpeed = 14.0f;
    float runStartDuration = 0.18f;
    float runEndDuration = 0.18f;
    float jumpEndDuration = 0.18f;
    float jumpImpulseFallbackDuration = 0.18f; // Sequencer未設定時に踏み切りを補完する待機時間。
    float timelineActionFallbackDuration = 1.5f;
    float groundDodgeCooldownDuration = 0.5f;
    float justDodgeInvincibilityDuration = 0.5f; // ジャスト回避成功後に保証する無敵秒数。
    float armor = 5.0f;
    float hitMediumImpactThreshold = 5.0f;
    float hitHeavyImpactThreshold = 100.0f;
    float airHitKnockdownImpactThreshold = 100.0f;

private:
    // パリィのアニメーションと前方判定を開始する。
    void BeginParry();
    // パリィの受付時間を進め、期限切れの判定を閉じる。
    void UpdateParry(float deltaTime_);
    // パリィ成功時に無敵と演出Actionを一度だけ発生させる。
    void NotifyParrySucceeded();
    // パリィの時間設定と成功時のAction選択を表示する。
    void DrawParryGui();

    std::shared_ptr<GuardCollider> parryCollider; // プレイヤー前方のパリィ判定。
    float parryWindowDuration = 0.25f; // 動作開始から攻撃を受け付ける秒数。
    float parryInvincibilityDuration = 0.35f; // 成功時に付与する無敵秒数。
    float parryWindowTimer = 0.0f;
    bool parrySucceeded = false; // 同じ動作で成功イベントを重複発火させない。
    bool parryReactionGlobal = false;
    std::string parryReactionCategory = "Sequence";
    std::string parryReactionAction; // 成功時に実行するEditor選択Action。

    void EnsurePlayerInputBindings(); // Player用InputActionの登録を保証する。
    int GetRequestedComboIndex(PLAYER_ACTION nextAction_) const; // 次に要求するコンボ段番号を計算する。
    void RegisterPlayerAnimationActions(); // Animation/Sequenceから呼ぶPlayer Actionを登録する。
    void SendActionToBlackboard(PLAYER_ACTION action_); // 現在Action情報をBlackboardへ送る。
    void UpdateLocomotionAnimationPhase(float deltaTime_);
    void SendLocomotionPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE phase_);
    void SendGroundedToBlackboard();
    bool UpdateStrongAttackInput();
    void ResetStrongAttackInput();
    bool IsGroundOnlyAction(PLAYER_ACTION action_) const;
    bool IsAirAction(PLAYER_ACTION action_) const;
    bool IsAirborneForAction() const;
    bool IsAnimationStateForAction(PLAYER_ACTION action_, const std::string& stateName_) const;
    bool IsDodgeAction(PLAYER_ACTION action_) const;
    bool IsDodgeCancelAction(PLAYER_ACTION action_) const;
    bool CanStartDodgeAction(PLAYER_ACTION action_) const;
    bool ShouldEnterGuardAfterActionEnd() const;
    void SyncCancelGateBlackboard();
    void TryConsumeBufferedActionForCancelGate(bool dodgeGate_);
    void UpdateDodgeRestrictions(float deltaTime_);
    void StartGroundDodgeCooldown();
    void UpdateAirborneActionContext();
    void UpdateTimelineActionSafety(float deltaTime_);
    void EnsureDefaultCancelTransitions();
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent() const;
    void ToggleLockOn();
    void ApplyPendingMovement(); // 予約された水平速度と向きをMovementへ反映する。

    bool ForceChangeAction(PLAYER_ACTION action_); // Debug用に強制Action遷移する。

    std::shared_ptr<ColliderComponent> collider; // Playerの物理Collider。
    std::shared_ptr<CharacterMovementComponent> movement; // Playerの移動制御Component。
    std::shared_ptr<HurtBox> hurtBox; // 被弾判定用HurtBox。
    std::shared_ptr<JustDodgeCollider> justDodgeCollider; // ジャスト回避判定用Collider。

    std::shared_ptr<BlackboardComponent> blackboard; // Player/Animへ状態を渡すBlackboard。
    PlayerStateMachine stateMachine; // PlayerのAction状態を管理するStateMachine。
    PlayerActionRuntime actionRuntime; // 現在Action中の入力受付とキャンセル状態。
    PlayerAttackAssistSettings attackAssistSettings; // 攻撃時の向き・位置補正設定。

    std::vector<PlayerCancelTransition> dodgeCancelTransitions
    {
        { PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::DODGE }
    };
    std::vector<PlayerCancelTransition> actionBranchTransitions; // Action間のキャンセル可能ルール一覧。

    int comboIndex = 0; // 現在再生中のコンボ段番号。
    int pendingComboIndex = 0; // 次に開始するActionへ渡すコンボ段番号。
    PLAYER_ACTION previousAction = PLAYER_ACTION::NONE;
    PLAYER_ACTION locomotionPhaseAction = PLAYER_ACTION::NONE;

    DirectX::XMFLOAT3 pendingHorizontalVelocity{ 0.0f, 0.0f, 0.0f }; // 次フレームに反映する水平速度。
    DirectX::XMFLOAT3 pendingFacingDirection{ 0.0f, 0.0f, 1.0f }; // 次フレームに反映する向き。

    bool hasPendingHorizontalVelocity = false; // 水平速度の反映予約があるか。
    bool hasPendingFacingDirection = false; // 向きの反映予約があるか。
    bool strongAttackHolding = false;
    bool strongAttackStartedAirborne = false;
    bool airborneActionContextActive = false;
    bool airborneActionContextLeftGround = false;
    bool jumpLoopResumeRequested = false;
    bool airDodgeConsumed = false;
    bool runEndActive = false;
    bool runtimeArmorOverrideActive = false;
    bool hasSentGrounded = false;
    bool lastSentGrounded = false;
    bool hasSentLocomotionPhase = false;
    float lastUpdateDeltaTime = 0.0f;
    float strongAttackHoldTimer = 0.0f;
    float locomotionPhaseTimer = 0.0f;
    float runEndTimer = 0.0f;
    float timelineActionSafetyTimer = 0.0f;
    float groundDodgeCooldownTimer = 0.0f;
    float runtimeArmor = 5.0f;
    PLAYER_LOCOMOTION_PHASE lastSentLocomotionPhase = PLAYER_LOCOMOTION_PHASE::IDLE;
    std::weak_ptr<GameObject> attackAssistTarget;

    PLAYER_ACTION debugSelectedAction = PLAYER_ACTION::IDLE; // Debug UIで選択中のAction。
};
