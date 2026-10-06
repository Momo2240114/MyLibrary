#pragma once

#include "SkillDefinition.h"
#include <functional>
#include <memory>

class GameObject;
class CharacterMovementComponent;
class ColliderComponent;

// 正常終了と、中断・素材喪失による停止を呼び出し側へ伝える。
enum class SkillExecutionResult
{
    NONE,
    COMPLETED,
    INTERRUPTED,
    PHASE_START_FAILED,
    TIMEOUT,
    ANIMATION_CHANGED
};

// 入力やAIのStateに依存せず、共通DefinitionのPhaseを実行する。
// Updateは所有側のState/AIから一度だけ呼び、既存の更新順を維持する。
class SkillExecution
{
public:
    using PhaseCallback = std::function<void(const SkillPhase&)>;
    SkillExecution() = default;
    // 実行中の制御権と予約Sequenceを複数の所有側へ複製しない。
    SkillExecution(const SkillExecution&) = delete;
    SkillExecution& operator=(const SkillExecution&) = delete;
    ~SkillExecution();
    // 素材とActionSequenceの接続を静的な定義から検証する。
    static bool CanExecute(const std::shared_ptr<GameObject>& owner_,
        const SkillDefinition& definition_, std::string& error_);
    // 実行データを複製し、呼び出し側固有の受付窓だけCallbackへ渡す。
    bool Start(const std::shared_ptr<GameObject>& owner_, const SkillDefinition& definition_,
        PhaseCallback phaseCallback_ = {});
    // 既存Animationの時間、通知、Phase秒数から次のPhaseへ進める。
    void Update(float deltaTime_);
    // 中断時にも自分の予約Sequenceと設定済みCleanupActionを閉じる。
    void Stop(SkillExecutionResult result_ = SkillExecutionResult::INTERRUPTED);
    // Animation Event/ActionSequenceのActionEndを現在のPhaseに渡す。
    void NotifyPhaseEnd();
    bool IsActive() const { return active; }
    int GetPhaseIndex() const { return phaseIndex; }
    float GetPhaseTime() const { return phaseTime; }
    const std::string& GetSkillName() const { return definition.name; }
    SkillExecutionResult GetResult() const { return result; }
    const std::string& GetError() const { return error; }
    const SkillPhase* GetCurrentPhase() const
    {
        return active && phaseIndex >= 0 && phaseIndex < static_cast<int>(definition.phases.size())
            ? &definition.phases[phaseIndex] : nullptr;
    }

private:
    // CallbackとActionで中断されても、古いPhaseの開始処理を続けない。
    bool BeginPhase();
    // 開始済みPhaseを閉じ、別のSequenceの予約は維持する。
    void EndPhase();
    // Owner Localを優先し、既存Global Actionへフォールバックする。
    void CallAction(const std::string& name_);
    // 共通Skillカテゴリへ実行状況を書き、BehaviorGraphとEditorから参照できるようにする。
    void SyncBlackboard();

    // Phaseごとの移動予約を既存Movementへ渡し、中断時に元の重力設定を戻す。
    void BeginPhaseMovement(const SkillPhase& phase_);
    void UpdateMovement(float deltaTime_);
    void RestoreMovement();

    std::weak_ptr<CharacterMovementComponent> movement;
    std::weak_ptr<ColliderComponent> movementCollider;
    bool ownsMovement = false;
    bool originalUseGravity = false;
    float originalGravityScale = 1.0f;
    bool gravitySuspended = false;
    float pendingVerticalImpulse = 0.0f;
    std::weak_ptr<GameObject> owner;
    SkillDefinition definition;
    PhaseCallback phaseCallback;
    int phaseIndex = -1;
    float phaseTime = 0.0f;
    float groundedTime = 0.0f; // 一時的な接地フラグで着地攻撃へ進まないための連続確認時間。
    float previousAnimationTime = 0.0f;
    bool active = false;
    bool phaseStarted = false;
    bool phaseEndRequested = false;
    bool stopping = false;
    bool ownsSequence = false;
    unsigned int sequenceGeneration = 0;
    unsigned int generation = 0;
    SkillExecutionResult result = SkillExecutionResult::NONE;
    std::string error;
};
