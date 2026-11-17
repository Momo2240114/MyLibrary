#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/GamePlay/Combat/SkillDefinition.h"
#include "Engine/GamePlay/AI/AIBehaviorGraph.h"
#include "HumanBossMind.h"
#include "Engine/GamePlay/Core/Object/PlayerActionObservation.h"
#include <array>
#include <random>
#include <memory>
#include <string>
#include <vector>
#include "Engine/Graphics/RenderSystem/RadialBlurSystem.h"
#include "Engine/Graphics/RenderSystem/BloomSystem.h"

class HumanBossActionComponent;
class HumanBossAITraceComponent;
class CameraActionDirectorComponent;

// 観察から選択する行動の大分類。特殊演出も同じ実行経路へ接続する。
enum class HumanBossState { NEUTRAL, REACTION, OFFENSE, REPOSITION, SHADOW, DRAGON_SHADOW, SPECIAL };
// 通常攻撃と防御に加え、影移動や演出攻撃の拡張先を区別する。
enum class HumanBossActionType
{
    NORMAL_ATTACK_A, NORMAL_ATTACK_B, BACK_DODGE, SIDE_DODGE, GUARD, JUST_GUARD,
    COUNTER_ATTACK, DELAY_ATTACK, GUARD_BREAK, SHADOW_SHIFT_BACK, SHADOW_SHIFT_SIDE,
    SHADOW_SHIFT_BEHIND, SHADOW_SHIFT_APPROACH, DRAGON_HEAD_BREATH, DRAGON_BITE,
    DRAGON_CLAW, DRAGON_TAIL, DRAGON_WING, SPECIAL_ATTACK, SPECIAL_COUNTER, LIGHT_COUNTER, WHIFF_PUNISH, ATTACK_FEINT, HEAVY_SINGLE_ATTACK, SHADOW_SLASH, SHADOW_DIVE_01, SHADOW_DIVE_02, SHADOW_DOMAIN, SHADOW_CHARGE, SHADOW_ORB, SHADOW_RAGNAROK, GUARD_EXECUTION, COUNT
};

// Phaseごとの使用可否、重み、再使用時間と共通Skillを保持する行動定義。
struct HumanBossActionDefinition
{
    HumanBossActionType type = HumanBossActionType::NORMAL_ATTACK_A;
    HumanBossState category = HumanBossState::OFFENSE;
    SkillDefinition skill;
    std::array<bool, 3> enabled{ true, true, true };
    std::array<float, 3> weight{ 1.0f, 1.0f, 1.0f };
    std::array<float, 3> cooldown{ 1.0f, 1.0f, 1.0f };
    int shadowShiftMode = 3; // 派生前の影渡り方向。0:後退、1:横、2:背後、3:接近。
    float minDistance = 0.0f;
    float maxDistance = 4.0f;
};

// 攻撃Animationごとの到達範囲。未登録Stateは共通の近接範囲で判定する。
struct HumanBossThreatDefinition
{
    std::string animationState;
    float reach = 3.5f;
    float halfAngle = 65.0f;
};

// Playerの攻撃予兆と距離を観察し、行動終了ごとに読み合いへ戻るAI。
class HumanBossAIComponent : public Component, public IActionProvider
{
public:
    // 検証済みAnimationに対応する標準行動と、未実装行動の接続先を用意する。
    HumanBossAIComponent();
    const std::string& GetName() const override;
    // 実行担当とAction通知を接続し、戦闘中だけ使う状態を初期化する。
    void Start() override;
    // 実行中Actionを優先し、待機時に観察・反応・距離調整を進める。
    void Update(float deltaTime_) override;
    // 影渡りやRootMotion後の位置へ、暴走の発生源を追従させる。
    void LateUpdate(float deltaTime_) override;
    // シーン破棄時にも、独立したParticleを残さない。
    void OnDestroy() override;
    // 外部からの行動要求と被弾通知をOwnerのRegistryへ公開する。
    void RegisterActions() override;
    // 現在の判断材料と、Phase別の行動設定をEditorへ表示する。
    void Gui() override;
    // 実行履歴を除き、AI設定と共通Skill定義を保存する。
    void Serialize(nlohmann::json& json_) const override;
    // 省略項目の標準値を維持し、保存された設定を復元する。
    void Deserialize(const nlohmann::json& json_) override;
    // Shadow、DragonShadow、SPECIALを外部の演出やActionSequenceから要求する。
    bool RequestAction(HumanBossActionType type_);
    // 実行中の防御・攻撃を閉じ、通常判断を待機状態へ戻す。
    void Stop();
    // HPを引く前に閾値を予約し、最終攻防中だけ通常死亡を保留する。
    float ConstrainBattleDamage(float damage_);
    // 有効なPlayer攻撃が最終スタンへ命中した瞬間だけ、決着演出へ引き渡す。
    bool TryBeginFinalFinish(const std::shared_ptr<GameObject>& attacker_);
    // リトライ・HPリセット・形態終了時に、保存しない戦闘履歴を破棄する。
    void ResetBerserk();
    bool IsBerserkActive() const { return berserkStage >= 0; }
    // 連撃と攻撃間の隙間は完全無敵とし、反撃用スタンでのみ解除する。
    bool IsBerserkInvulnerable() const { return berserkStage >= 0 && !berserkStunned && !finishRequested; }
    // Owner階層を持たない独立範囲攻撃のジャストガードも同じ受付へ接続する。
    void NotifyBerserkJustGuard() { OnBerserkJustGuarded(); }
    void NotifyReflectedOrb() { reflectedOrbTimer = 5.0f; }
    // 既存BehaviorTree Editorへ、実行中の判断グラフと保存先を公開する。
    AIBehaviorGraph& GetBehaviorGraph() { return behaviorGraph; }
    std::string& GetBehaviorGraphAssetPath() { return behaviorGraphAssetPath; }
    // 実行中Actionを止め、Editorの実行状態を初期化する。
    void ResetBehaviorGraphRuntime();
    // 編集した判断グラフを既存のAsset形式で保存する。
    bool SaveBehaviorGraphAsset(const std::string& path_) const;
    // 読み込み成功時だけ現在の判断グラフを差し替える。
    bool LoadBehaviorGraphAsset(const std::string& path_);
    // 実行中Actionに対応するノードをEditorで強調表示する。
    std::string GetCurrentGraphActionName() const;
private:
    // 追跡専用Componentは状態を読み取るだけで、AIの選択や実行を変更しない。
    friend class HumanBossAITraceComponent;
    // 通常BehaviorGraphより優先する有限の連撃・受け流し・スタンを更新する。
    bool UpdateBerserk(float deltaTime_);
    // 通常AIと同じ戦闘許可を使い、単体テストでもHP閾値を予約する。
    bool CanRunBerserkBattle();
    // 予約された暴走を、通常攻撃・被弾待ち・JG硬直より優先して一度だけ開始する。
    bool TryStartBerserk();
    bool BeginBerserkAttack();
    void FinishBerserkAttack();
    void EnterBerserkStun();
    void OnBerserkJustGuarded();
    // 攻撃単位の終了処理と分離して、連撃全体のオーラを管理する。
    void StartBerserkAura();
    void StopBerserkAura();
    // 暴走開始の短い予備動作を非スケール時間で進め、終了・中断時に描画設定を戻す。
    void BeginBerserkEntrance();
    bool UpdateBerserkEntrance();
    void RestoreBerserkEntrance();
    // ダウン開始を最後まで再生した後、指定のダウンループへ切り替える。
    void UpdateBerserkDown(float deltaTime_);
    void DrawBerserkGui();
    void SerializeBerserk(nlohmann::json& json_) const;
    void DeserializeBerserk(const nlohmann::json& json_);
    void StartBloodEye();
    void StopBloodEye(bool immediate_ = true);
    void BeginSpecialIntro();
    void UpdateSpecialIntro();
    void RestoreSpecialIntro();
    std::array<int, 3> berserkAttackCounts{3,5,12};
    std::array<float, 3> berserkSpeeds{1.25f,1.5f,1.8f};
    std::array<float, 3> berserkStunDurations{3.0f,4.0f,5.0f};
    std::array<bool, 3> berserkPending{};
    std::array<bool, 3> berserkUsed{};
    int berserkStage = -1;
    int berserkStartedCount = 0;
    bool berserkAttackInFlight = false;
    bool berserkStunned = false;
    bool finalPhaseReserved = false;
    bool finishRequested = false;
    float berserkTimer = 0.0f;
    float berserkFailureTime = 0.0f;
    float berserkParryDelay = 0.16f;
    float berserkActionGap = 0.06f;
    float berserkMinimumTell = 0.18f;
    std::weak_ptr<GameObject> berserkStunEffect;
    std::weak_ptr<GameObject> berserkAuraEffect;
    std::weak_ptr<GameObject> berserkEntranceEffect;
    RadialBlurSystem::Parameters berserkPreviousBlur;
    // 色相・彩度・色バランスを含めて保存し、白黒演出の終了後に元の配色へ戻す。
    BloomSystem::CbColorFilter berserkPreviousFilter;
    float berserkEntranceRemaining = 0.0f;
    float berserkEntranceDuration = 0.6f;
    float berserkEntranceBlur = 0.035f;
    std::array<float, 3> berserkEntranceBlurMultipliers{1.0f,1.5f,2.0f};
    std::array<float, 3> berserkEntranceMonochrome{0.55f,0.8f,1.0f};
    float berserkDownElapsed = 0.0f;
    bool berserkBlurOwned = false;
    bool berserkDownLoop = false;
    std::string berserkStartReason = "Waiting for HP threshold";
    float bloodEyeHpThreshold = 0.55f;
    float bloodEyeDuration = 25.0f;
    float bloodEyeRemaining = 0.0f;
    bool bloodEyeUsed = false;
    std::array<std::weak_ptr<GameObject>, 2> bloodEyeEffects;
    std::weak_ptr<CameraActionDirectorComponent> specialIntroCamera;
    float specialIntroRemaining = 0.0f;
    bool specialIntroCameraReturning = false;
    bool specialIntroLocked = false;
    bool specialIntroPreviousPlayerLock = false;
    // 優先判断はBehaviorGraphへ渡し、行動の実行条件と終了だけをComponentで管理する。
    void EnsureDefaultBehaviorGraph();
    // 攻撃設定が広い場合はIntentの追跡距離にも反映し、古い共通距離で攻撃を遮断しない。
    float GetAttackDecisionDistance() const;
    void TickBehaviorGraph(float deltaTime_, bool finishCurrent_ = false);
    // ツリー内の候補へ使用条件とPhase別Weightを渡す。
    float GetGraphActionWeight(const std::string& actionName_, std::string* reason_ = nullptr) const;
    // ツリーで選ばれた接近・後退・観察を既存の実行担当へ渡す。
    void UpdateLocomotion(const std::string& actionName_);
    AIBehaviorGraph behaviorGraph;
    AIBehaviorContext behaviorContext;
    std::string behaviorGraphAssetPath = "Data/AI/BehaviorGraph/HumanBoss.json";
    std::string graphStatus;
    SkillDefinition justGuardReactionSkill;
    bool justGuardStunned = false; // 終了通知とクリップ終端が揃うまで通常のBehaviorTreeを更新しない。
    bool stoppingAction = false; // 中断Actionからの再入による新規行動の開始を防ぐ。
    // 履歴、予兆時間、攻撃の到達可能性を一度の観察で更新する。
    bool ObservePlayer();
    // Phase、距離、連続使用、Cooldown、必要素材の条件をまとめて判定する。
    bool IsAvailable(std::size_t index_, std::string* reason_ = nullptr) const;
    // 開始成功時だけ履歴とCooldownを確定する。
    bool BeginAction(std::size_t index_);
    // 行動終了後に観察時間と周回方向を選び直す。
    void EnterNeutral();
    // 弾かれた攻撃を完全に中断し、敵自身のSequenceで反撃可能な硬直へ入る。
    void BeginJustGuardRecoil();
    // Editorと追加Actionが参照できる判断情報をBlackboardへ公開する。
    void PublishDebug();
    // 保存値やGUI入力を、距離と時間の有効な範囲へ揃える。
    void NormalizeSettings();

    // Managerの観測を使い、履歴・反応予算・Intentを更新する。
    void UpdateMind(float deltaTime_);
    // 意図を選ぶ前に、観察履歴・反応待ち・反撃可能時間を更新する。
    void UpdateMindMemory(float deltaTime_);
    // 空中強攻撃の記憶がある場合に、着地への反撃または追跡を優先する。
    bool TrySelectAirHeavyIntent();
    // 現在の距離と観察傾向から候補を重み付けし、次の意図を選択する。
    void SelectIntent();
    void DrawMindGui();
    float GetIntentActionModifier(std::size_t index_) const;
    float GetReactionCost(HumanBossActionType type_) const;
    bool IsReactionReady() const;
    // 受け流しを開始できる強攻撃を判別し、受付直前まで他の防御へ逸れないようにする。
    bool IsHeavyJustGuardReady() const;
    HumanBossMindSettings mindSettings;
    PlayerActionObservation playerObservation;
    HumanBossIntent intent = HumanBossIntent::OBSERVE;
    float reactionBudget = 3.0f;
    float reactionDelayRemaining = 0.0f;
    float intentLockTimer = 0.0f;
    float whiffTimer = 0.0f;
    // DodgeやIdleを挟む反復も短い観測履歴として保持し、空中への攻撃は強制しない。
    float airHeavyPressureTimer = 0.0f;
    float airHeavyMemoryTimer = 0.0f;
    int airHeavyAttackChain = 0;
    std::uint64_t airHeavyAttackSerial = 0;
    float feintTimer = 0.0f;
    float baitTimer = 0.0f;
    float aggression = 0.5f;
    float confidence = 0.5f;
    float approachTendency = 0.0f;
    float retreatTendency = 0.0f;
    float rangedPressureTime = 0.0f; // 遠距離を維持する時間を選択重みへ反映する。
    DirectX::XMFLOAT3 previousPlayerPosition{ 0.0f, 0.0f, 0.0f };
    bool hasPreviousPlayerPosition = false;
    float counterWindowTimer = 0.0f;
    int reactionStreak = 0;
    int bossHitStreak = 0;
    float bossHitChainTimer = 0.0f;
    int guardProbeCount = 0;
    int dodgePunishRequests = 0;
    bool attackWasThreatening = false;
    bool lightCounterPending = false;
    bool comboDelayPending = false;
    bool debugForceIntent = false;
    bool debugForcePhase = false;
    int debugPhase = 0;
    bool debugFreezeBudget = false;
    std::uint64_t mindPlayerSerial = 0;
    std::uint64_t reactionDecisionSerial = 0;
    std::uint64_t whiffSerial = 0;
    std::uint64_t bossHitsObserved = 0;
    std::string intentReason;
    std::vector<HumanBossActionDefinition> actions;
    std::vector<HumanBossThreatDefinition> threats;
    // 実行履歴とCooldownは保存せず、シーン開始時に初期化する。
    std::vector<float> cooldowns;
    std::weak_ptr<HumanBossActionComponent> executor;
    std::weak_ptr<GameObject> target;
    std::mt19937 random{ std::random_device{}() };
    std::string playerObjectName = "PlayerModel";
    std::string idleState = "SS_Idle";
    std::string walkState = "HumanBossCombatWalk";
    std::string hitState = "SS_HitGround_Medium";
    float hitRecoveryDuration = 0.6f;
    float hitTimer = 0.0f;
    float comboBindTimer = 0.0f;
    float comboBindDuration = 0.5f;
    float playerCounterProtectionTimer = 0.0f;
    float reflectedOrbTimer = 0.0f;
    bool playerCounterStarted = false;
    bool playerCounterFromGuard = false;
    // 死亡や演出ロックによる停止を、行動中の待機とは区別して表示する。
    std::string suspensionReason;
    float minCombatDistance = 2.0f;
    float preferredCombatDistance = 3.0f;
    float maxCombatDistance = 6.0f;
    float waitMin = 0.5f;
    float waitMax = 1.5f;
    float walkSpeed = 2.0f;
    float strafeSpeed = 1.2f;
    float dodgeSpeed = 6.0f;
    float reactionCooldown = 0.8f;
    float dodgeCooldown = 1.2f;
    float attackCooldown = 1.0f;
    float reactionLookAhead = 0.35f;
    float justGuardLead = 0.22f;
    float playerAttackReach = 3.5f;
    float playerAttackHalfAngle = 65.0f;
    float threatHeightTolerance = 2.0f;
    float noneReactionWeight = 15.0f;
    float repositionTime = 0.6f;
    float phase2Hp = 0.7f;
    float phase3Hp = 0.35f;
    std::array<float, 3> phaseReactionMultiplier{ 1.0f, 0.85f, 0.7f };
    bool shadowEnabled = true;
    bool dragonShadowEnabled = false;
    bool specialEnabled = false;

    HumanBossState state = HumanBossState::NEUTRAL;
    HumanBossState lastActionCategory = HumanBossState::NEUTRAL;
    int phase = 0;
    int currentAction = -1;
    int lastAction = -1;
    int lastReaction = -1;
    int lastOffense = -1;
    int observedPlayerAction = -1;
    int observedCombo = -1;
    int sameActionRepeatCount = 0;
    unsigned int attackSerial = 0;
    unsigned int reactedSerial = 0;
    float previousPlayerAnimationTime = 0.0f;
    float waitTimer = 0.0f;
    float repositionTimer = 0.0f;
    float reactionTimer = 0.0f;
    float dodgeTimer = 0.0f;
    float attackTimer = 0.0f;
    float distance = 0.0f;
    float incomingTime = 0.0f;
    float strafeDirection = 1.0f;
    bool playerAttackDetected = false;
    bool counterPending = false;
    bool runtimeStarted = false;
    std::string recentPlayerAction;
    std::string observedAnimation;
    std::string reactionReason;
    // 小さな固定長履歴でGuard/Dodge傾向を公開し、分析Systemの重複を避ける。
    float guardFrequency = 0.0f;
    float dodgeFrequency = 0.0f;
    DirectX::XMFLOAT3 toPlayer{ 0.0f, 0.0f, 1.0f };
};
