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
    // 行動を開始できない理由を、AI Inspectorと実行担当へ返す。
    const std::string& GetValidationError() const { return validationError; }
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
    // AIの接近目標にも、予定移動と現在の補正上限から見積もった開始範囲を渡す。
    float GetAttackStartReach(const SkillDefinition& skill_, const std::shared_ptr<GameObject>& target_) const;
    bool IsActive() const { return execution.IsActive(); }
    // Combat Systemが確定した成功を一度だけAIへ返す。
    bool ConsumeJustGuardSuccess();
    // 通常Guardの実成立を、一度だけ軽反撃の判断へ渡す。
    bool ConsumeGuardSuccess();
    // Dodgeが観測された後、次の段間隔にだけ追加待機を適用する。
    bool ExtendComboDelay(float extension_, float maximum_);
    SkillExecutionResult GetResult() const { return execution.GetResult(); }
    // 移動アニメーションと注視を引き受け、AIの判断から再生処理を分離する。
    void SetLocomotion(const std::string& state_, const DirectX::XMFLOAT3& velocity_,
        const DirectX::XMFLOAT3& facing_, const DirectX::XMFLOAT2& blendInput_);
    // 注視方向を既存Movementへ予約する。
    void FaceTarget(const DirectX::XMFLOAT3& direction_, bool immediate_ = false);
private:
    // 予兆とHitBoxは同じAttackTemplateの半径を使用する。
    void ShowShadowArea();
    // 予兆で固定した中心へ一度だけ範囲攻撃を発生させる。
    void StrikeShadowArea();
    // 追跡可能な短い区間だけ、空中の水平移動を通常Movementへ予約する。
    void UpdateShadowDive(float deltaTime_);
    // 物理Queryで実際の着地点の高さと足場を確認する。
    bool ResolveShadowGround(const DirectX::XMFLOAT3& position_, DirectX::XMFLOAT3& ground_) const;
    // 上空への出現経路に天井や障害物がないことを確認する。
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
    // Phase切替時だけWarpを開始し、後段は初段で固定したTarget位置を引き継ぐ。
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
