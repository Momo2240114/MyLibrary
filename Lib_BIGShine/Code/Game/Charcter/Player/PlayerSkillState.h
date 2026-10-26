#pragma once
#include "PlayerSkillDefinition.h"
#include "Engine/GamePlay/Combat/SkillExecution.h"

class GuardCollider;
class HitReactionComponent;
class MotionWarpComponent;

// 共通SkillExecutionと、PlayerのState終了・キャンセル受付を接続する。
class PlayerSkillState : public PlayerState
{
public:
    // 既存SkillA/B/CのAction識別子を維持して共通実行へ接続する。
    PlayerSkillState(const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_, PLAYER_ACTION action_);
    ~PlayerSkillState() override;
    const char* GetName() const override { return "PlayerSkillState"; }
    // Playerの開始状態を初期化し、共通Definitionを実行する。
    void Enter() override;
    // 共通Phase更新後にPlayer入力を受け付け、終了だけをStateMachineへ通知する。
    void Execute(float elapsedTime_) override;
    // 中断時にも共通実行の予約Actionと後処理を閉じる。
    void Exit() override;
    // 既存ActionEnd通知を共通実行へ渡す。
    void NotifyPhaseEnd();
    // 編集プレビューを中断し、通常のAction終了経路へ戻す。
    void StopSkill();
    bool IsActive() const { return active; }
    bool IsFinishing() const { return finishing; }
    bool PreventsHitReaction() const;
    int GetPhaseIndex() const { return execution.GetPhaseIndex(); }
    const std::string& GetSkillName() const { return execution.GetSkillName(); }

private:
    // Phaseのプリセットを使い、対象までの不足分だけを既存MotionWarpへ渡す。
    void StartPhaseMotionWarp(const SkillPhase& phase_);
    // 自分が開始した補正だけを停止し、中断や次段へ要求を持ち越さない。
    void StopPhaseMotionWarp();
    std::weak_ptr<MotionWarpComponent> motionWarp;
    std::string motionWarpRequestName;
    // Player固有の状態は既存Playerカテゴリへミラーする。
    void SyncBlackboard();
    // GuardとArmorは開始前の値を保持し、Phase切替・終了・中断で復元する。
    void ApplyPhaseDefence(const SkillPhase& phase_);
    // 防御を使わないPhaseとして処理し、保存したガードと耐性の設定へ戻す。
    void RestoreDefence();
    // 子オブジェクトを含む各ガード判定の、スキル適用前の状態を保存する。
    struct GuardSnapshot
    {
        std::weak_ptr<GuardCollider> collider;
        bool enabled = false;
        bool justGuard = false;
    };
    std::vector<GuardSnapshot> guardSnapshots;
    bool guardApplied = false;
    bool armorApplied = false;
    bool previousArmorOverride = false;
    float previousArmor = 0.0f;
    std::weak_ptr<HitReactionComponent> protectedHitReaction;
    float previousReactionArmor = 0.0f;
    PLAYER_ACTION action;
    SkillExecution execution;
    // Stateとしての実行期間を示し、SkillExecution終了後の終了通知まで保持する。
    bool active = false;
    // Phase終了通知とスキル全体の終了通知を区別し、終了処理の再入を防ぐ。
    bool finishing = false;
};
