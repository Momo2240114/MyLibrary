#pragma once
#include "PlayerSkillDefinition.h"
#include "Engine/GamePlay/Combat/SkillExecution.h"

class GuardCollider;
class HitReactionComponent;

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
    // Player固有の状態は既存Playerカテゴリへミラーする。
    void SyncBlackboard();
    // GuardとArmorは開始前の値を保持し、Phase切替・終了・中断で復元する。
    void ApplyPhaseDefence(const SkillPhase& phase_);
    void RestoreDefence();
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
    bool active = false;
    bool finishing = false;
};
