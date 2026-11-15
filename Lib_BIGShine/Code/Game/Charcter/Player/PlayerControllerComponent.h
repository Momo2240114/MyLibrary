#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "PlayerStateMachine.h"
#include "PlayerSkillDefinition.h"
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <array>

class ColliderComponent;
class BlackboardComponent;
class CameraLockOnComponent;
class CharacterMovementComponent;
class HurtBox;
class HitBox;
class JustDodgeCollider;

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

// 通常ガードの反応と盾演出だけを調整し、判定角度は既存GuardColliderへ任せる。
struct PlayerGuardSettings
{
    // ガード解除の見た目、受け止め後の入力再開、崩れ後の復帰時間を別々に調整する。
    float releaseBlendTime = 0.08f;
    float hitCancelTime = 0.04f;
    float breakRecoveryTime = 0.65f;
    float reactionMaximumTime = 2.5f;
    std::string hitAnimation = "Block_Hit_Seq";
    std::string breakAnimation = "Block_Hit_Break_Seq";
    std::string meshVFXAsset = "Data/VFX/PlayerGuardShieldEnergy.json";
    std::string shieldObjectName = "Shield";
    DirectX::XMFLOAT3 meshVFXOffset{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 meshVFXRotation{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 meshVFXScale{ 1.03f, 1.03f, 1.03f };
    std::string sparkParticleAsset = "Data/Particle/PlayerGuardSparks.json";
    float sparkLifeTime = 0.35f;
};

// 攻撃時にTargetへ向きや位置を補正するための設定。
struct PlayerAttackAssistSettings
{
    // 開始時の対象固定と、攻撃中・コンボ間での対象引き継ぎ方針。
    bool lockTargetOnAttackEnter = true;
    bool allowRetargetDuringAttack = false;
    bool nonLockOnUseNearestFallback = false;
    float nonLockOnMaximumTargetAngleDegrees = 75.0f;
    float lockOnMaximumTargetAngleDegrees = 180.0f;
    // 回転量の累積上限と、向き補正を適用するアニメーションの正規化時間。
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
    // 破棄時にも拘束中の移動制限と一時的な耐性を解除する。
    void OnDestroy() override;
    // 拘束を開始できる状態で、拘束・復帰用の再生Stateが登録されているか確認する。
    bool CanBeginEnemyExecution() const;
    // 専用反撃のひるみ・位置固定を開始し、再生要求はアニメーショングラフへ渡す。
    bool BeginEnemyExecution(const std::shared_ptr<GameObject>& attacker_, float holdTime_, float forwardDistance_, bool showRecoil_ = false);
    // 同じ敵による拘束だけを解除し、解除時の移動入力から復帰動作を選ぶ。
    void ReleaseEnemyExecution(const std::shared_ptr<GameObject>& attacker_);
    void Gui() override; // Player制御用のDebug UIを表示する。

    void InitializeBlackboardValues(); // Player/Anim用Blackboard値を初期化する。

    void Serialize(nlohmann::json& j_) const override; // Player設定をJSONへ保存する。
    void Deserialize(const nlohmann::json& j_) override; // JSONからPlayer設定を復元する。

    DirectX::XMFLOAT3 GetMoveInput() const; // カメラ基準の移動入力方向を取得する。
    // ワールド空間の入力を敵方向または自身の正面基準へ変換し、ガードのBlendへ渡す。
    DirectX::XMFLOAT3 GetGuardMoveInput(const DirectX::XMFLOAT3& worldMoveInput_) const;
    DirectX::XMFLOAT3 GetMoveInputAxis() const; // 生の移動入力Axisを取得する。
    DirectX::XMFLOAT3 GetCameraForwardDirection() const; // カメラの水平Forward方向を取得する。
    void SetFacingDirection(const DirectX::XMFLOAT3& direction_); // Playerの向きを指定方向へ向ける。
    // 有効なロックオン対象がある間、ガードの向きを対象へ合わせる。
    void ApplyGuardFacingToTarget();
    // 攻撃State間で引き継ぐ補正対象を弱参照で保持する。
    void SetAttackAssistTarget(const std::shared_ptr<GameObject>& target_);
    std::shared_ptr<GameObject> GetAttackAssistTarget() const { return attackAssistTarget.lock(); }
    // スキルの補正先をロックオン優先で選び、遠距離や背後の敵への吸い付きを避ける。
    std::shared_ptr<GameObject> ResolveSkillMotionWarpTarget() const;
    void ClearAttackAssistTarget() { attackAssistTarget.reset(); }
    void SetHorizontalVelocity(
        const DirectX::XMFLOAT3& dir_,
        float speed_,
        bool updateFacing_ = true); // Update終端で反映する水平速度を予約する。
    void AddHorizontalVelocityAssist(const DirectX::XMFLOAT3& dir_, float speed_); // 予約済み水平速度へ補助速度を加算する。

    ColliderComponent* GetCollider() const { return collider.get(); } // PlayerのColliderを返す。
    CharacterMovementComponent* GetMovement() const { return movement.get(); } // Playerの移動Componentを返す。
    bool IsJustDodgeColliderActive() const;
    bool IsGrounded() const;
    bool IsJumpInputPressed() const;
    bool IsCurrentAnimationState(const std::string& stateName_);
    // ジャンプや空中攻撃の開始時点から、離地・着地を追跡する。
    void BeginAirborneActionContext();
    // 着地後の空中扱いと、ジャンプへの復帰要求を解除する。
    void EndAirborneActionContext();
    // 空中行動の終了からJumpStateへ戻る要求を、一度だけ取り出す。
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
    // 死亡・復帰の境界で攻撃や拘束を中断し、移動予約と被弾状態を待機状態へ戻す。
    void ResetDeathPresentationState();
    // 専用Playerステートへ入り、再生要求はアニメーショングラフへ渡す。
    void BeginDeathState();
    void BeginReviveState();
    bool IsDeathAnimationFinished() const;
    bool IsReviveAnimationFinished() const;
    // 基本設定を保存したまま、戦闘中だけ衝撃耐性を上書きする。
    void SetRuntimeArmor(float armor_);
    // 一時的な上書きを解除し、基本設定の衝撃耐性へ戻す。
    void ResetRuntimeArmor();
    float GetCurrentArmor() const { return runtimeArmorOverrideActive ? runtimeArmor : armor; }
    bool HasRuntimeArmorOverride() const { return runtimeArmorOverrideActive; }
    bool ConsumeBufferedAction(); // 保存中の先行入力Actionを消費する。
    void EnterAction(PLAYER_ACTION action_); // Action開始時のRuntime状態を初期化する。
    // スキルの素材とCooldownを含む実行条件を一か所で判定する。
    bool CanExecuteSkill(PLAYER_ACTION action_) const;
    // HUDへ実際の入力表と受付状態を公開し、UI側で戦闘条件を再定義させない。
    const std::vector<PlayerSkillInputBinding>& GetSkillInputBindings() const { return skillInputs; }
    bool CanDisplayActionReady(PLAYER_ACTION action_) const;
    // HUDやEditorへ定義の秒数と、Assetへ保存しない現在の残り時間を公開する。
    float GetSkillCooldownDuration(PLAYER_ACTION action_) const;
    float GetRemainingSkillCooldown(PLAYER_ACTION action_) const;
    bool IsSkillCooldownReady(PLAYER_ACTION action_) const;
    // 共通実行の開始成功をStateから受け取り、そのSkillだけの待ち時間を開始する。
    void NotifySkillStarted(PLAYER_ACTION action_);
    void ResetSkillCooldowns();
    // Workspace保存後、実行中の定義を維持したまま待機時だけ再読込する。
    bool ReloadSkillDefinitions(const std::string& path_);
    // 実行中の共通Skillを中断し、既存State終了処理へ接続する。
    void StopSkill();
    const std::string& GetSkillDefinitionsPath() const { return skillDefinitionsPath; }
    // StateはControllerの定義を開始時に複製して使用する。
    const PlayerSkillDefinition* GetSkillDefinition(PLAYER_ACTION action_) const;
    // 本番SkillSelectも同じAPIで装備を変更し、スロットから使用要求を発行できる。
    bool SetEquippedSkill(std::size_t slotIndex_, const std::string& skillName_);
    const std::string& GetEquippedSkillName(std::size_t slotIndex_) const;
    bool RequestSkillSlot(std::size_t slotIndex_);
    bool CanEditSkillLoadout() const;
    const std::vector<PlayerSkillDefinition>& GetSkillCatalog() const { return skillDefinitions; }
    const PlayerSkillDefinition* GetSkillDefinitionByName(const std::string& skillName_) const;
    float GetRemainingSkillCooldown(const std::string& skillName_) const;
    // Player InspectorとSkill Workspaceで同じ装備編集UIを使用する。
    void DrawSkillLoadoutGui();
    void ProcessActionInput(); // 入力からAction要求を発行する。
    // MOVE内の加減速と開始・停止を更新し、戦闘への割り込みは既存の入力処理へ任せる。
    void UpdateGroundMovement(float deltaTime_);
    void SendMoveInputToBlackboard(const DirectX::XMFLOAT3& input_); // 移動入力をBlackboardへ送る。
    // ガード基準の移動入力をPlayerとAnimの両カテゴリへ同期する。
    void SendGuardMoveInputToBlackboard(const DirectX::XMFLOAT3& input_);
    // ガード入力の保持状態をPlayerとAnimの両カテゴリへ同期する。
    void SendGuardInputToBlackboard(bool guardInput_);
    bool IsGuardBroken() const { return guardBroken; }
    // JumpStateの進行段階を移動アニメーション用のフラグへ反映する。
    void SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE phase_);
    // 回避用の受付窓と遷移ルールを満たす、地上・空中回避入力を要求する。
    bool TryDodgeCancelInput();

    void NotifyJustDodge(); // ジャスト回避成功を現在のDodgeStateへ通知する。
    void GrantJustDodgeCounterTag(); // ジャスト回避成功後の反撃Tagを一定時間付与する。
    void GrantJustGuardCounterTag(); // ジャストガード成功後の反撃Tagと派生用Tagを一定時間付与する。
    bool IsAirborneForAction() const;
    float moveSpeed = 6.0f; // 通常移動速度。
    float guardMoveSpeed = 2.0f;
    // 地上の踏み切りと二段ジャンプで与える上向きインパルス。
    float jumpImpulse = 8.5f;
    float doubleJumpImpulse = 8.0f;
    // 空中移動の目標速度倍率と、入力あり・なしでの速度補間係数。
    float airControlRate = 0.72f;
    float airControlAcceleration = 8.0f;
    float airDeceleration = 4.5f;
    // 強攻撃を溜め攻撃へ切り替える保持時間と、溜め攻撃開始時の踏み込み速度。
    float strongAttackHoldThreshold = 0.35f;
    float strongChargeAttackSpeed = 14.0f;
    // 地上移動だけの応答と見た目を調整し、攻撃・回避のRootMotionへ影響させない。
    float groundAcceleration = 140.0f; // 地上の加速度。単位はワールド単位/秒の二乗。
    float groundReversalDeceleration = 330.0f; // 大きな切り返しだけに使う制動加速度。
    float groundDirectionTurnSpeed = 1440.0f; // 水平速度の向きを変更する最大角速度。
    float groundFullSpeedTurnMultiplier = 0.20f;
    float groundTurnBankAngle = 10.0f;
    float groundStopSpeedThreshold = 0.05f; // 微小な残速を停止とみなす速度。
    float groundDeceleration = 160.0f;
    float groundTurnSpeed = 1080.0f; // 毎秒の最大旋回角度。
    float groundInputStartThreshold = 0.18f;
    float groundInputReleaseThreshold = 0.10f;
    float groundAnimationReferenceSpeed = 16.0f; // 対応する移動付きLoopとモデル倍率から得た歩調基準。
    float groundStartBlendTime = 0.08f;
    float groundLoopBlendTime = 0.08f;
    float groundStopBlendTime = 0.06f;
    float runStartDuration = 0.26f; // Startアニメーションの目標再生秒数。
    float runEndDuration = 0.30f; // Stopアニメーション全体の目標再生秒数。
    float jumpEndDuration = 0.18f;
    float jumpImpulseFallbackDuration = 0.18f; // Sequencer未設定時に踏み切りを補完する待機時間。
    float timelineActionFallbackDuration = 1.5f;
    float groundDodgeCooldownDuration = 0.5f;
    float justDodgeInvincibilityDuration = 0.5f; // ジャスト回避成功後に保証する無敵秒数。
    // ジャスト成功時の短いスローと、BGMを強め、SEを軽めにこもらせる演出を調整する。
    void PlayJustSuccessPresentation(bool justGuard_);
    bool justSuccessPresentationEnabled = true;
    float justSuccessTimeScale = 0.25f;
    float justGuardTimeScale = 0.65f;
    float justSuccessHoldDuration = 0.25f;
    float justSuccessReturnDuration = 0.2f;
    float justSuccessMuffleCutoff = 1200.0f;
    float counterTagDuration = 5.0f; // ジャスト回避/ジャストガード成功後に反撃Tagを保持する秒数。
    // 基本の衝撃耐性と、被弾モーションの強さ・空中ノックダウンを分ける衝撃値。
    float armor = 5.0f;
    float hitMediumImpactThreshold = 5.0f;
    float hitHeavyImpactThreshold = 100.0f;
    float airHitKnockdownImpactThreshold = 100.0f;

private:
    void EnsurePlayerInputBindings(); // Player用InputActionの登録を保証する。
    int GetRequestedComboIndex(PLAYER_ACTION nextAction_) const; // 次に要求するコンボ段番号を計算する。
    void RegisterPlayerAnimationActions(); // Animation/Sequenceから呼ぶPlayer Actionを登録する。
    void SendActionToBlackboard(PLAYER_ACTION action_); // 現在Action情報をBlackboardへ送る。
    void UpdateLocomotionAnimationPhase(float deltaTime_);
    // 移動フェーズをPlayerとAnimへ同期し、同じ値の再通知を抑える。
    void SendLocomotionPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE phase_);
    // 接地状態が変わったときに、両カテゴリの接地フラグを更新する。
    void SendGroundedToBlackboard();
    // 強攻撃の解放または保持時間の満了から、通常・溜めの攻撃要求を作る。
    bool UpdateStrongAttackInput();
    void ResetStrongAttackInput();
    bool IsGroundOnlyAction(PLAYER_ACTION action_) const;
    bool IsAirAction(PLAYER_ACTION action_) const;

    bool IsPlayerControlLockedByGameMode() const;
    // 現在の再生名が行動に対応するか確認し、別行動の終了通知を区別する。
    bool IsAnimationStateForAction(PLAYER_ACTION action_, const std::string& stateName_) const;
    bool IsDodgeAction(PLAYER_ACTION action_) const;
    bool IsDodgeCancelAction(PLAYER_ACTION action_) const;
    // 地上回避の待ち時間と、着地までの空中回避の使用制限を確認する。
    bool CanStartDodgeAction(PLAYER_ACTION action_) const;
    PLAYER_ACTION ResolveCounterAttack(bool weakAttack_) const;
    // 反撃Tag成立前に保存された通常攻撃入力を、反撃Actionの先行入力へ差し替える。
    void PromoteBufferedAttackToCounterAttack();
    // 攻撃・地上回避の終了後に、ガード保持入力による復帰を許可するか判定する。
    bool ShouldEnterGuardAfterActionEnd() const;
    // 回避と通常派生の受付状態を、アニメーション側と共有する。
    void SyncCancelGateBlackboard();
    // 成功時に付いた反撃Tagを減算し、失効したときだけ表示状態を同期する。
    void UpdateCounterTags(float deltaTime_);
    // 反撃可能かと、その根拠となる回避・ガードの成功種別を公開する。
    void SyncCounterTagsToBlackboard();
    void ClearCounterTags();
    // 開いた受付窓の種類に合う予約入力だけを、現在の使用条件で再判定する。
    void TryConsumeBufferedActionForCancelGate(bool dodgeGate_);
    // 地上回避の待ち時間を進め、接地時に空中回避の使用済み状態を戻す。
    void UpdateDodgeRestrictions(float deltaTime_);
    void StartGroundDodgeCooldown();
    // 空中行動開始直後の接地と、実際の離地後の着地を区別する。
    void UpdateAirborneActionContext();
    // 共通Skillの終了管理を優先し、それ以外の終了通知漏れを再生時間で補う。
    void UpdateTimelineActionSafety(float deltaTime_);
    void EnsureDefaultCancelTransitions();
    // 入力・判定・演出を同じガードの開始と終了へ接続する。
    void SetNormalGuardEnabled(bool enabled_);
    // ガードを離れる全経路から、判定・演出・再生制御をまとめて解除する。
    void StopNormalGuard();
    // 通常ガード成功とガード崩れを分け、反応中の入力受付と再生を設定する。
    void HandleGuardImpact(bool broken_);
    // 通知・再生終端・上限時間からガード反応の終了を判定する。
    void UpdateGuardReaction(float deltaTime_);
    // 通常ガードは構えへ戻し、崩れ後は通常操作へ返して入力の再解放を待つ。
    void FinishGuardReaction();
    // 所有する盾の階層を確認して、追従するガード演出を生成する。
    void StartGuardVisual();
    // このControllerが所有するガード演出をPoolへ返す。
    void StopGuardVisual();
    // ガード時に記録された接触位置と法線を使って火花を生成する。
    void PlayGuardSparks();
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent() const;
    void ToggleLockOn();
    void ApplyPendingMovement(); // 予約された水平速度と向きをMovementへ反映する。
    // 地上移動の開始・中断時だけ実行情報を破棄し、他行動の移動やImpulseは変更しない。
    void ResetGroundMovement();
    void SetGroundBank(float angle_);

    bool ForceChangeAction(PLAYER_ACTION action_); // Debug用に強制Action遷移する。

    std::shared_ptr<ColliderComponent> collider; // Playerの物理Collider。
    std::shared_ptr<CharacterMovementComponent> movement; // Playerの移動制御Component。
    std::shared_ptr<HurtBox> hurtBox; // 被弾判定用HurtBox。
    std::shared_ptr<JustDodgeCollider> justDodgeCollider; // ジャスト回避判定用Collider。

    std::shared_ptr<BlackboardComponent> blackboard; // Player/Animへ状態を渡すBlackboard。
    // Scene JSONには定義ファイルの参照を保存し、内容は外部JSONで編集する。
    std::string skillDefinitionsPath = "Data/Skills/PlayerSkills.json";
    std::vector<PlayerSkillDefinition> skillDefinitions;
    PlayerSkillLoadout skillLoadout;
    bool hasExplicitSkillLoadout = false;
    // 古いSceneにはDefinitionのAction項目から初期装備を移行する。
    void ApplyLegacySkillLoadout();
    // Skill名をキーにした実行時情報。Serializeへは書き込まない。
    std::unordered_map<std::string, float> skillCooldowns;
    // 表示のたびに素材と全Phaseを再検証せず、装備別に検証結果を短時間だけ共有する。
    struct SkillHudAssetCache
    {
        std::string skillName;
        float recheckTime = -1.0f;
        bool available = false;
    };
    mutable std::array<SkillHudAssetCache, PlayerSkillLoadout::SLOT_COUNT> skillHudAssetCache;
    float hudObservationTime = 0.0f; // 検証結果の有効期間だけに使い、スキル残り時間には使用しない。
    // Cooldownと行動条件は即時参照し、素材の重い検証だけをキャッシュする。
    bool CanDisplaySkillReady(PLAYER_ACTION action_) const;
    void UpdateSkillCooldowns(float deltaTime_);
    std::vector<PlayerSkillInputBinding> skillInputs
    {
        { 1, "Player.SkillModifier", "Player.StrongAttack", 100 },
        { 0, "Player.SkillModifier", "Player.WeakAttack", 90 },
        { 2, "Player.SkillModifier", "Player.JustGuard", 80 }
    };
    PlayerStateMachine stateMachine; // PlayerのAction状態を管理するStateMachine。
    // 拘束と復帰の実行中だけ通常入力を止め、Scene保存へ途中状態を持ち越さない。
    bool enemyExecutionCaptured = false;
    bool enemyExecutionRecovering = false;
    bool enemyExecutionRecoveryStarted = false;
    float enemyExecutionElapsed = 0.0f;
    float enemyExecutionHoldTime = 1.92f;
    float enemyExecutionDistance = 1.8f;
    bool enemyExecutionRecoilActive = false; // 拘束位置へ移す前に、弾かれたひるみをその場で見せる。
    bool enemyExecutionPreviousArmorOverride = false;
    float enemyExecutionPreviousArmor = 0.0f;
    std::weak_ptr<GameObject> enemyExecutionAttacker;
    std::string enemyExecutionRecoveryState;
    // グラフの状態を監視し、固定・復帰・中断時の一時設定を管理する。
    bool UpdateEnemyExecution(float deltaTime_);
    void CancelEnemyExecution(bool returnToIdle_ = true);
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

    DirectX::XMFLOAT3 pendingHorizontalVelocity{ 0.0f, 0.0f, 0.0f }; // Update終端で反映する水平速度。
    DirectX::XMFLOAT3 pendingFacingDirection{ 0.0f, 0.0f, 1.0f }; // Update終端で反映する向き。

    bool hasPendingHorizontalVelocity = false; // 水平速度の反映予約があるか。
    bool hasPendingFacingDirection = false; // 向きの反映予約があるか。
    // 強攻撃を押し始めたときの空中状態を保ち、解放時の攻撃種別を決める。
    bool strongAttackHolding = false;
    bool strongAttackStartedAirborne = false;
    // 一度離地してから接地するまで、空中行動の継続と回数制限を管理する。
    bool airborneActionContextActive = false;
    bool airborneActionContextLeftGround = false;
    bool jumpLoopResumeRequested = false;
    bool airDodgeConsumed = false;
    // MOVEを抜けると破棄し、攻撃や回避の速度を通常移動へ持ち越さない。
    float groundMoveSpeed = 0.0f;
    float groundPhaseTimer = 0.0f;
    float groundPreviousDeltaTime = 0.0f;
    float groundBankAngle = 0.0f;
    float groundLastTurnSign = 1.0f;
    bool groundMoveInputActive = false;
    bool groundHasPreviousPosition = false;
    bool groundWasGrounded = false;
    bool groundHasActualVelocity = false;
    bool groundReversing = false;
    bool groundSoftStop = false;
    bool groundMovementBlocked = false;
    float groundTargetSpeed = 0.0f;
    // 目標、現在、物理へ渡した区間平均、衝突後の実速度を分離する。
    DirectX::XMFLOAT3 groundTargetVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 groundCurrentVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 groundRequestedVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 groundActualVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 groundMoveDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 groundPreviousPosition{ 0.0f, 0.0f, 0.0f };
    PLAYER_LOCOMOTION_PHASE groundMovePhase = PLAYER_LOCOMOTION_PHASE::IDLE;
    // 一時的な耐性を基本設定のarmorと区別し、終了時に基本値へ戻せるようにする。
    bool runtimeArmorOverrideActive = false;
    bool hasSentGrounded = false;
    bool lastSentGrounded = false;
    bool hasSentLocomotionPhase = false;
    float lastUpdateDeltaTime = 0.0f;
    float strongAttackHoldTimer = 0.0f;
    float timelineActionSafetyTimer = 0.0f;
    // 地上回避は終了後の待ち時間、反撃Tagは成功通知後の受付時間を保持する。
    float groundDodgeCooldownTimer = 0.0f;
    float justDodgeCounterTagTimer = 0.0f;
    float justGuardCounterTagTimer = 0.0f;
    float runtimeArmor = 5.0f;
    PLAYER_LOCOMOTION_PHASE lastSentLocomotionPhase = PLAYER_LOCOMOTION_PHASE::IDLE;
    std::weak_ptr<GameObject> attackAssistTarget;
    PlayerGuardSettings guardSettings;
    std::weak_ptr<GameObject> guardVisual;
    // 受け止め・崩れの反応中だけ、通常グラフによるアニメーション上書きを抑える。
    bool guardReactionActive = false;
    bool guardReactionEndRequested = false;
    bool guardAnimationControlled = false;
    bool guardBroken = false;
    bool normalGuardActive = false;
    bool guardRearmRequired = false; // ガードからの派生・GuardBreak後は入力解放で再受付する。
    float guardReactionTimer = 0.0f;

    PLAYER_ACTION debugSelectedAction = PLAYER_ACTION::IDLE; // Debug UIで選択中のAction。
};
