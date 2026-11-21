#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/GamePlay/Combat/SkillExecution.h"
#include <cstddef>
#include <vector>
#include <utility>
#include <array>
#include <random>

// 行動の防御受付を、既存Colliderの操作へ対応付ける。
enum class HumanBossDefense { NONE, DODGE, GUARD, JUST_GUARD };

// 影渡りの移動目的を分け、同じ安全確認と移動処理を共有する。
enum class HumanBossShadowShift { BACK, SIDE, BEHIND, APPROACH };

class HitReactionComponent;
class HurtBox;
class GltfModelCommponennt;
class CameraActionDirectorComponent;
struct MotionWarpPreset;
struct AttackTemplate;

// 防御受付と移動を既存Componentへ渡す、人型ボスの行動実行担当。
class HumanBossActionComponent : public Component, public IActionProvider
{
public:
    ~HumanBossActionComponent() override;
    const std::string& GetName() const override;
    // Animation EventとCombat Systemから受け取るActionを登録する。
    void Start() override;
    // 終了と成功の通知をweak参照でOwnerへ接続する。
    void RegisterActions() override;
    // 敵用の既存MotionWarpComponentと、未設定の初期プリセットを接続する。
    void InitializeAttackAssist();
    // 共通Skillの終了理由と素材不足を表示する。
    void Gui() override;
    void Serialize(nlohmann::json& json_) const override;
    void Deserialize(const nlohmann::json& json_) override;
    // 攻撃中と連続被弾後の短い区間だけ、弱い衝撃による行動中断を抑える。
    void UpdateHitProtection(float deltaTime_);
    // ダメージ受付を維持したまま、指定した衝撃耐性で短い復帰区間を確保する。
    void ProtectRecovery(float duration_, float minimumArmor_ = 0.0f);
    // AIから一度だけ進め、共通SkillExecutionと更新を重複させない。
    void Tick(float deltaTime_);
    // 死亡・Scene終了時に独立弾と予兆も所有者ごとに解除する。
    void OnDestroy() override;
    // 素材・経路・同時弾数を確認し、追加行動の開始可否を返す。
    bool CanStartRanged(const std::string& skillName_, const std::shared_ptr<GameObject>& target_) const;
    // 既存HP Phaseとは別に、HP条件と中央への安全な経路を確認する。
    bool CanUseRagnarok() const;

    bool WasRagnarokUsed() const
    {
        return ragnarokUsed;
    }

    const std::string& GetRagnarokBlockReason() const
    {
        return ragnarokBlockReason;
    }

    float GetRagnarokHpThreshold() const
    {
        return ragnarokHpThreshold;
    }

    // 行動を開始できない理由を、AI Inspectorと実行担当へ返す。
    const std::string& GetValidationError() const
    {
        return validationError;
    }

    // 実行に必要なAnimation、Sequence、防御Colliderの存在を確認する。
    bool CanStart(const SkillDefinition& skill_, HumanBossDefense defense_,
        const std::shared_ptr<GameObject>& target_ = nullptr) const;
    // 共通Skillを開始し、回避は方向だけを指定してRootMotionで移動させる。
    bool Begin(const SkillDefinition& skill_, HumanBossDefense defense_, const DirectX::XMFLOAT3& dodgeDirection_, const std::shared_ptr<GameObject>& target_ = nullptr);
    // 壁・足場と対象の間合いを確認して影渡りの候補を選ぶ。
    bool CanShadowShift(const SkillDefinition& skill_, HumanBossShadowShift mode_, const std::shared_ptr<GameObject>& target_, float side_) const;
    // 予備動作と復帰を共通Skillで管理し、影の移動だけを予約する。
    bool BeginShadowShift(const SkillDefinition& skill_, HumanBossShadowShift mode_,
        const std::shared_ptr<GameObject>& target_, float side_);
    // 共通Skillと防御窓を中断し、攻撃イベントを終了する。
    void Stop(const std::string& idleState_ = "SS_Idle");
    // 自身の武器と派生攻撃だけを無効化し、中断後の再有効化予約も閉じる。
    void StopAttackHitBoxes(bool clearTrail_ = true);

    // 連撃中はHP減少を維持し、被弾ひるみだけで連撃全体が消えないようにする。
    void SetBerserkProtection(bool protected_)
    {
        berserkProtection = protected_;
        UpdateHitProtection(0.0f);
    }

    // AIの接近目標にも、予定移動と現在の補正上限から見積もった開始範囲を渡す。
    float GetAttackStartReach(const SkillDefinition& skill_, const std::shared_ptr<GameObject>& target_) const;

    bool IsActive() const
    {
        return execution.IsActive();
    }

    // 短い通常技では曲を切り替えず、ラグナロクの実行区間だけ専用曲の対象にする。
    bool IsSpecialMusicActive() const
    {
        return execution.IsActive() && rangedSkill == "SHADOW_RAGNAROK";
    }

    // Combat Systemが確定した成功を一度だけAIへ返す。
    bool ConsumeJustGuardSuccess();
    // 通常Guardの実成立を、一度だけ軽反撃の判断へ渡す。
    bool ConsumeGuardSuccess();
    // Dodgeが観測された後、次の段間隔にだけ追加待機を適用する。
    bool ExtendComboDelay(float extension_, float maximum_);

    SkillExecutionResult GetResult() const
    {
        return execution.GetResult();
    }

    // 移動アニメーションと注視を引き受け、AIの判断から再生処理を分離する。
    void SetLocomotion(const std::string& state_, const DirectX::XMFLOAT3& velocity_,
        const DirectX::XMFLOAT3& facing_, const DirectX::XMFLOAT2& blendInput_);
    // 注視方向を既存Movementへ予約する。
    void FaceTarget(const DirectX::XMFLOAT3& direction_, bool immediate_ = false);

private:
    // 自身の攻撃に対するジャストガード・ジャスト回避から無敵解除Actionへつなぐ。
    void NotifyCounterOpportunity(std::shared_ptr<GameObject> attacker_);
    // 反撃を受け付けるため、影移動と部位のAction由来の無敵を解除する。
    void ReleaseCounterInvulnerability();
    // ガード継続への掴みは近距離・接地中だけ許可し、予兆後にも回避の成否を確認する。
    bool CanCaptureGuardingPlayer(const std::shared_ptr<GameObject>& target_) const;
    // ジャストガード反撃でPlayerを固定し、終了・死亡・中断時に必ず解除する。
    void BeginExecutionCapture();
    // ジャストガードで攻撃を弾かれたプレイヤーに、拘束前のひるみを再生させる。
    void BeginExecutionRecoil();
    void ReleaseExecutionCapture();
    // 専用反撃の開始時に、敵の向きを基準とした正面寄りのカメラActionを開始する。
    void PlayExecutionCamera();
    // Playerと敵を同じ倍率で短時間スローにし、拘束の手応えを振動で伝える。
    void PlayExecutionGrabFeedback();
    // 叩きつけのAnimation Eventから一度だけ追加ダメージとシェイクを発生させる。
    void PlayExecutionSlamFeedback();
    std::weak_ptr<GameObject> executionCapturedPlayer;
    std::weak_ptr<CameraActionDirectorComponent> executionCameraDirector;
    std::string activeExecutionCameraAction;
    bool executionGrabFeedbackPlayed = false;
    bool executionSlamFeedbackPlayed = false;
    // カメラの構図はCameraAction側、拘束と着地の手応えはこの設定から調整する。
    std::string executionCameraAction = "EnemyExecution";
    float executionSlowScale = 0.25f;
    float executionSlowHoldDuration = 0.2f;
    float executionGrabRumbleStrength = 0.7f;
    float executionSlamDamage = 50.0f; // 拘束が成立した叩きつけで一度だけ与える固定ダメージ。
    float executionSlamShakeStrength = 0.12f;
    float executionSlamShakeDuration = 0.24f;
    float executionSlamShakeFrequency = 28.0f;
    float executionHoldTime = 1.92f;
    float executionTargetDistance = 1.8f;
    // 対ガードの掴みを許可する水平距離。成功済みのジャストガード反撃には適用しない。
    float guardExecutionReach = 2.8f;
    // 共通SkillのPhase切替に合わせ、固定経路と独立弾を開始する。
    SkillDefinition PrepareRangedSkill(const SkillDefinition& skill_) const;
    void BeginRangedPhase(const SkillPhase& phase_);
    void UpdateRangedAttack();
    // CharacterやCombat Volumeを除外し、予兆を載せる実際の地面を取得する。
    bool ResolveRangedGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const;
    void ShowChargeArea();
    // 突進完了時に、予兆と同じ帯へ電撃と一定時間の攻撃判定を発生させる。
    void StrikeChargeArea();
    // 突進と特殊攻撃で共通の電撃を、指定した地面の線分へ走らせる。
    void SpawnElectricStrip(const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_,
        float length_, float sweepTime_);
    void SpawnShadowOrbs();
    // 円形ステージ内に収まる縦横の帯を、予兆と打撃で共有する。
    void BuildRagnarokGrid();
    // 逆回転した同じ帯情報を、予兆・HitBox・Particleで共有する。
    void RotateRagnarokGrid(float angle_);
    void SpawnRagnarokParticles();
    void UpdateRagnarokParticles(float deltaTime_);
    void ShowRagnarokArea();
    void StrikeRagnarokArea();
    void ClearRangedArea();
    void RegisterRangedActions();
    void DrawRangedGui();
    void SerializeRanged(nlohmann::json& json_) const;
    void DeserializeRanged(const nlohmann::json& json_);
    bool PlanCharge(const std::shared_ptr<GameObject>& target_, DirectX::XMFLOAT3& end_) const;
    bool StartFixedWarp(const SkillPhase& phase_, const DirectX::XMFLOAT3& end_);
    // 突進終了や中断時に、移動へ追従する攻撃判定を閉じる。
    void StopChargeHit();
    // 走り斬りの振り終わりまでに、予兆で示した移動を完了する。
    static constexpr float CHARGE_WARP_END_NORMALIZED_TIME = 0.25f;
    std::string rangedSkill;
    std::weak_ptr<GameObject> rangedDanger;
    std::weak_ptr<GameObject> chargeHit;
    std::vector<std::weak_ptr<GameObject>> shadowOrbs; // 弾は行動終了後も寿命まで継続する。
    bool orbSpawnCommitted = false; // 現在の詠唱で生成済みなら、Sequence通知で重複生成しない。
    DirectX::XMFLOAT3 chargeStart{};
    DirectX::XMFLOAT3 chargeGroundStart{};
    bool chargeWarningReady = false;
    float chargeWarningVisibleStartTime = -1.0f; // 描画準備後の予兆表示時間を管理する。
    std::string rangedProgress; // 実行中と直前の追加行動の到達段階を表示する。
    DirectX::XMFLOAT3 ragnarokGroundCenter{};
    DirectX::XMFLOAT3 chargeEnd{};
    float chargeYaw = 0.0f;
    float chargeLength = 0.0f;
    float chargeAreaWidth = 2.0f; // 予兆開始時の幅を、接触判定と突進跡でも共有する。
    float chargeMaximumDistance = 18.0f;
    float chargeDistanceMultiplier = 1.5f; // 対象までの距離に対して、予兆と突進経路を長くする。
    float chargeElectricDuration = 0.8f; // 突進後の電撃と攻撃判定を維持する時間。
    bool chargeElectricStruck = false; // 完了判定が複数フレーム続いても生成は一度だけ行う。
    float chargeWidth = 2.0f;
    float chargeGrowthTime = 1.0f;
    std::string chargeGrowthCurve = "HumanBoss.ChargeGrowth";
    std::string rangedAttackSoundPath; // 専用SEが未設定の場合は再生しない。
    int orbCount = 3;
    int orbMaximumCount = 6;
    float orbSideOffset = 1.5f;
    float orbHeight = 3.0f;
    float orbBackOffset = 1.5f;
    float orbWaitTime = 1.5f;
    float orbInterval = 0.4f;
    float orbSpeed = 3.0f;
    float orbAcceleration = 0.5f;
    float orbTurnSpeed = 25.0f;
    float orbLifeTime = 8.0f;
    // 旧JSONでも周回と高速発射を使い、従来の基準速度は倍率調整用に残す。
    float orbOrbitRadius = 3.2f;
    float orbOrbitTilt = 35.0f;
    float orbOrbitSpeed = 180.0f;
    float orbOrbitDuration = 2.4f;
    float orbLaunchSpeedMultiplier = 4.0f;
    float orbReflectSpeedMultiplier = 1.5f;
    bool ragnarokEnabled = true;
    mutable std::string ragnarokBlockReason; // 他行動の検証に上書きされない特殊攻撃の開始不可理由。
    bool ragnarokUsed = false; // HP Phaseとは独立した初回使用履歴。
    float ragnarokHpThreshold = 0.5f;
    int ragnarokAttackCount = 4;
    int ragnarokRound = 0;
    float ragnarokAdvanceSpeed = 8.0f; // 中央への準備移動はCharacterMovementで行う。
    float ragnarokAdvanceMultiplier = 4.0f; // 旧JSONの移動速度にも影移動用の倍率を適用する。
    float ragnarokRadius = 48.0f;
    float ragnarokAngle = 180.0f;
    int ragnarokGridLineCount = 9;
    float ragnarokGridWidth = 1.4f;
    float ragnarokGridSpacing = 9.0f;

    // 予兆開始時の寸法を保持し、途中のGUI編集でも打撃範囲を変えない。
    struct RagnarokGridStrip
    {
        DirectX::XMFLOAT3 position{};
        float yaw = 0.0f;
        float halfWidth = 0.0f;
        float length = 0.0f;
        float offset = 0.0f;
        int axis = 0;
    };

    std::vector<RagnarokGridStrip> ragnarokGrid;
    float ragnarokWarningDuration = 0.0f;
    float ragnarokRotationSpeed = 120.0f;
    float ragnarokRotationTime = 1.0f;
    float ragnarokTellTime = 1.25f;
    float ragnarokRecoveryTime = 1.6f;
    float ragnarokParticleSweepTime = 0.16f;
    float ragnarokParticleDensity = 0.55f;
    float electricDensityMultiplier = 4.5f; // 泥の量は維持し、共通の電撃粒子だけを増やす。
    int ragnarokParticleLineLimit = 24;
    std::string ragnarokParticleAsset = "Data/Particle/HumanBossGridElectricMud.json";

    // 電撃の線分移動を管理し、ダメージ判定は別の攻撃Effectで制御する。
    struct RagnarokParticleSweep
    {
        std::weak_ptr<GameObject> object;
        DirectX::XMFLOAT3 start{};
        DirectX::XMFLOAT3 end{};
        float time = 0.0f;
        float duration = 0.16f; // 発生元ごとの電撃が線分を走り切る時間。
    };

    std::vector<RagnarokParticleSweep> ragnarokParticleSweeps;
    float ragnarokHitDuration = 0.2f;
    float ragnarokYaw = 0.0f;
    float ragnarokRoundStartYaw = 0.0f;
    DirectX::XMFLOAT3 rangedStageCenter{};
    float rangedStageRadius = 50.0f;
    // 予兆とHitBoxは同じAttackTemplateの半径を使用する。
    void ShowShadowArea();
    // 予兆で固定した中心へ一度だけ範囲攻撃を発生させる。
    void StrikeShadowArea();
    // 追跡可能な短い区間だけ、空中の水平移動を通常Movementへ予約する。
    void UpdateShadowDive(float deltaTime_);
    // 物理Queryで実際の着地点の高さと足場を確認する。
    bool ResolveShadowGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const;
    // プレイヤーの正面に、着地時の間合いを確保した地面位置を求める。
    bool ResolveShadowDiveGround(DirectX::XMFLOAT3& ground_) const;
    // 正面の着地点から上空への出現経路に天井や障害物がないことを確認する。
    bool CanRaiseShadowDive(float height_) const;
    // 派生中断時に予兆と演出を所有者ごとに閉じる。
    void ClearShadowArea();
    // 派生前の移動中だけ所有階層のモデルを隠し、中断時も元の描画状態へ戻す。
    void SetShadowVisible(bool visible_);
    // 影渡りの先頭Phaseだけ、所有階層の被弾を完全に拒否する。
    void SetShadowInvulnerable(bool invulnerable_);
    // 移動開始時の対象位置で経路を固定し、移動中の無制限な追尾を避ける。
    bool BuildShadowPath(HumanBossShadowShift mode_, const std::shared_ptr<GameObject>& target_,
        float side_, std::vector<DirectX::XMFLOAT3>& waypoints_) const;
    // 開始時に乱数・カーブ・安全な軌道を確定し、実距離に合った所要時間を求める。
    bool PlanShadowTrajectory(HumanBossShadowShift mode_, const std::vector<DirectX::XMFLOAT3>& waypoints_);
    // 検証した軌道の累積距離から移動位置を補間する。
    DirectX::XMFLOAT3 SampleShadowTrajectory(float distance_) const;
    // Inspectorで基準経路・蛇行経路・現在位置を確認する。
    void DrawShadowTrajectoryPreview();
    // 影渡り中もCharacterMovement経由で移動し、経路の変化で停止する。
    void UpdateShadowShift(float deltaTime_);
    // 敵専用のMeshVFXを現在位置へ発生させる。
    void SpawnShadowEffect();
    // 予定RootMotion、補正上限、Colliderと経路から攻撃開始の可否を共有判定する。
    bool BuildAttackAssistPlan(const SkillPhase& phase_, const std::shared_ptr<GameObject>& target_,
        DirectX::XMFLOAT3& plannedMotion_, float& stopDistance_, float& endNormalizedTime_,
        float& playbackDuration_, bool checkReachability_ = true, float reachMargin_ = 0.0f) const;
    // Phase切替時だけWarpを開始し、選択したTarget位置を振り終わるまで固定する。
    void BeginAttackAssist(const SkillPhase& phase_);
    // 段終了時に実Targetと固定した狙いを調べ、届かない次の打撃は開始しない。
    bool CanContinueAttack(const SkillPhase& phase_);
    // CharacterControllerと同じQuery Filterで、壁と足場のない移動経路を拒否する。
    bool CanWarpPath(const DirectX::XMFLOAT3& start_, const DirectX::XMFLOAT3& end_) const;
    // ボスの武器Trailを切り替え、攻撃開始・中断時には履歴も解除する。
    void SetWeaponTrails(bool emitting_, bool clearExisting_);
    // 行動の中断・終了時に、ボス自身の防御窓だけを閉じる。
    void CloseDefense();
    // パターン番号と段数を分けて公開し、終了・中断時には表示用状態も解除する。
    void SyncComboState();
    std::shared_ptr<AttackTemplate> shadowAttackSnapshot;
    std::string shadowAreaTemplate;
    bool shadowChain = false;
    bool shadowHideModels = true;
    bool shadowModelsHidden = false;
    std::vector<std::pair<std::weak_ptr<GltfModelCommponennt>, bool>> hiddenShadowModels;
    bool shadowLandingLocked = false;
    bool shadowAreaStruck = false;
    float shadowDiveFallSpeed = 27.6f; // Skillの落下速度から予兆表示の時間を見積もる。
    DirectX::XMFLOAT3 shadowGroundPosition{};
    DirectX::XMFLOAT3 shadowLandingPosition{};
    // 表示色と着地点追跡時間は敵専用の設定として保存する。
    std::string shadowDangerMaterial = "Data/VFXMaterials/HumanBossShadowDanger.vfxmat";
    float shadowLandingTrackTime = 0.25f;
    float shadowApproachDistance = 14.0f;
    float shadowBackDistance = 8.0f;
    float shadowSideDistance = 7.5f;
    float shadowMoveSpeed = 32.0f;
    float shadowMoveStartTime = 0.18f;
    float shadowMoveEndTime = 1.08f;
    float shadowMaximumPathDistance = 24.0f;
    float shadowReappearDuration = 0.3f; // 到着後にも姿を見せる時間を確保する。
    float shadowEffectInterval = 0.1f;
    float shadowEffectSpacing = 1.2f;
    DirectX::XMFLOAT3 shadowLastEffectPosition{};
    bool shadowHasEffectPosition = false;
    bool shadowSwayEnabled = true;
    float shadowSwayWidth = 1.0f;
    std::array<float, 4> shadowSwayStrength{ 0.2f, 0.7f, 0.65f, 0.4f }; // BACK、SIDE、BEHIND、APPROACH。
    float shadowSwayCycles = 1.25f;
    float shadowSwayRandomness = 0.2f;
    float shadowSwayBias = 0.0f;
    float shadowConvergence = 2.0f;
    std::string shadowSwayCurve = "HumanBoss.ShadowSwayEnvelope";
    std::string shadowSpeedCurve = "HumanBoss.ShadowMoveSpeed";
    std::mt19937 shadowRandom{ std::random_device{}() };
    std::vector<DirectX::XMFLOAT3> shadowTrajectory;
    std::vector<float> shadowTrajectoryDistances;
    std::vector<float> shadowSpeedProgress;
    std::vector<DirectX::XMFLOAT3> shadowBaseWaypoints;
    DirectX::XMFLOAT3 shadowPendingPosition{};
    float shadowPendingDistance = 0.0f;
    float shadowTravelDistance = 0.0f;
    float shadowPlannedDuration = 0.0f;
    float shadowActualWidth = 0.0f;
    bool shadowPendingMove = false;
    bool shadowPathBlocked = false;
    bool shadowPathValidated = false;
    int shadowPreviewMode = 3;
    DirectX::XMFLOAT3 shadowPreviousTargetPosition{};
    std::vector<std::weak_ptr<GameObject>> shadowVisuals;
    bool shadowInvulnerabilityApplied = false;
    std::vector<std::weak_ptr<HurtBox>> shadowProtectedHurtBoxes;
    bool shadowShiftActive = false;
    bool shadowArrived = false;
    float shadowElapsed = 0.0f;
    float shadowEffectTimer = 0.0f;
    std::size_t shadowWaypoint = 0;
    std::vector<DirectX::XMFLOAT3> shadowWaypoints; // 開始時に検証した経路だけを順番に通る。
    SkillExecution execution;
    bool attackAssistEnabled = true;
    bool attackAssistSkill = false;
    bool hasAttackTargetPosition = false;
    std::weak_ptr<GameObject> attackTarget;
    // 独立した予備動作でも追従上限を共有し、打撃Phaseへ固定位置を渡す。
    std::string windupWarpPresetName;
    bool windupFacingTracked = false;
    bool windupTargetLocked = false;
    float windupRotationCorrection = 0.0f;
    DirectX::XMFLOAT3 attackStartPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 attackInitialTargetPosition{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 attackTargetPosition{ 0.0f, 0.0f, 0.0f };
    mutable float debugPlannedDistance = 0.0f;
    mutable float debugRequiredCorrection = 0.0f;
    float assistFrameDeltaTime = 1.0f / 60.0f; // 現在の更新間隔から1フレーム上限も到達判定へ反映する。
    bool justGuardSucceeded = false;
    bool guardSucceeded = false;
    // 予備動作の間だけPlayerを追い、振り始めた後は攻撃方向を固定する。
    bool windupTracking = false;
    bool attackActive = false;
    bool berserkProtection = false;
    HumanBossDefense currentDefense = HumanBossDefense::NONE;
    std::string status;
    mutable std::string validationError;
    float flinchArmor = 10.0f;
    float recoveryProtectionTimer = 0.0f;
    float recoveryArmor = 0.0f; // 復帰保護中だけ使い、終了時に通常の耐性へ戻す。
    float previousReactionArmor = 0.0f;
    bool armorApplied = false;
    std::weak_ptr<HitReactionComponent> protectedReaction;
};
