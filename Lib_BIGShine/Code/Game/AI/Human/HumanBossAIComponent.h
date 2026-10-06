#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "Engine/GamePlay/EventSystem/IActionProvider.h"
#include "Engine/GamePlay/Combat/SkillDefinition.h"
#include <array>
#include <random>
#include <memory>
#include <string>
#include <vector>

class HumanBossActionComponent;

// 観察から選択する行動の大分類。特殊演出も同じ実行経路へ接続する。
enum class HumanBossState { NEUTRAL, REACTION, OFFENSE, REPOSITION, SHADOW, DRAGON_SHADOW, SPECIAL };
// 通常攻撃と防御に加え、影移動や演出攻撃の拡張先を区別する。
enum class HumanBossActionType
{
    NORMAL_ATTACK_A, NORMAL_ATTACK_B, BACK_DODGE, SIDE_DODGE, GUARD, JUST_GUARD,
    COUNTER_ATTACK, DELAY_ATTACK, GUARD_BREAK, SHADOW_SHIFT_BACK, SHADOW_SHIFT_SIDE,
    SHADOW_SHIFT_BEHIND, SHADOW_SHIFT_APPROACH, DRAGON_HEAD_BREATH, DRAGON_BITE,
    DRAGON_CLAW, DRAGON_TAIL, DRAGON_WING, SPECIAL_ATTACK, SPECIAL_COUNTER, COUNT
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
private:
    // 履歴、予兆時間、攻撃の到達可能性を一度の観察で更新する。
    bool ObservePlayer();
    // Phase、距離、連続使用、Cooldown、必要素材の条件をまとめて判定する。
    bool IsAvailable(std::size_t index_) const;
    // 開始成功時だけ履歴とCooldownを確定する。
    bool BeginAction(std::size_t index_);
    // 抽選は同じ目的の候補内で行い、反応・攻撃・特殊行動を混ぜない。
    bool SelectAction(HumanBossState category_, bool reaction_ = false);
    // 行動終了後に観察時間と周回方向を選び直す。
    void EnterNeutral();
    // Editorと追加Actionが参照できる判断情報をBlackboardへ公開する。
    void PublishDebug();
    // 保存値やGUI入力を、距離と時間の有効な範囲へ揃える。
    void NormalizeSettings();

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
    float minCombatDistance = 2.0f;
    float preferredCombatDistance = 3.0f;
    float maxCombatDistance = 5.5f;
    float waitMin = 0.5f;
    float waitMax = 1.5f;
    float walkSpeed = 2.0f;
    float strafeSpeed = 1.2f;
    float dodgeSpeed = 6.0f;
    float reactionCooldown = 0.8f;
    float dodgeCooldown = 1.2f;
    float attackCooldown = 1.0f;
    float reactionLookAhead = 0.35f;
    float justGuardLead = 0.15f;
    float playerAttackReach = 3.5f;
    float playerAttackHalfAngle = 65.0f;
    float threatHeightTolerance = 2.0f;
    float noneReactionWeight = 15.0f;
    float repositionTime = 0.6f;
    float phase2Hp = 0.7f;
    float phase3Hp = 0.35f;
    std::array<float, 3> phaseReactionMultiplier{ 1.0f, 0.85f, 0.7f };
    bool shadowEnabled = false;
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
    std::vector<int> playerHistory;
    float guardFrequency = 0.0f;
    float dodgeFrequency = 0.0f;
    DirectX::XMFLOAT3 toPlayer{ 0.0f, 0.0f, 1.0f };
};
