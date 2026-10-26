#pragma once

#include "Engine/GamePlay/Core/Component/BlackboardComponent.h"

#include <algorithm>
#include <string>

enum class GameProgressState
{
    GAME_SCENE_START,
    BOSS_INTRO,
    PHASE_1_BATTLE,
    PHASE_1_DEFEAT,
    PHASE_2_INTRO,
    PHASE_2_BATTLE,
    BOSS_DEFEATED,
    RESULT
};

enum class BossBattlePhase
{
    NONE = 0,
    PHASE_1 = 1,
    PHASE_2 = 2,
    DEFEATED = 3
};

class GameObjectManager;

// ゲーム全体で共有する進行状態とGlobalBlackboardを管理する軽量Manager。
// 移動、演出、AI判断などのSystem側が参照するロック状態やAI方針を集約する。
class GameModeManager
{
public:
    GameModeManager() = default;
    ~GameModeManager() = default;

    // 初期状態をGlobalBlackboardへ書き込む。
    void Start();
    // 毎フレーム共有状態をGlobalBlackboardへ同期する。
    void Update(float deltaTime_);
    // 確定した形態・暴走・大技の状態から、戦闘中だけBGMを選ぶ。
    void UpdateBattleMusic(GameObjectManager& manager_);
    // 戦闘進行、HP割合、AI方針を初期値へ戻す。
    void ResetBattleState();
    // GameScene開始後の縦進行を開始する。
    void BeginGameSceneFlow();

    // 他Systemが参照する共有Blackboardを返す。
    BlackboardComponent& GetGlobalBlackboard() { return globalBlackboard; }
    const BlackboardComponent& GetGlobalBlackboard() const { return globalBlackboard; }

    GameProgressState GetCurrentGameState() const { return currentGameState; }
    const std::string& GetCurrentSequenceName() const { return currentSequenceName; }
    const char* GetCurrentGameStateName() const;

    bool IsGameplayLocked() const { return cinematicPreview || gameplayLocked; }
    bool IsPlayerControlLocked() const { return cinematicPreview || playerControlLocked; }
    bool IsEnemyAILocked() const { return cinematicPreview || enemyAILocked; }
    bool IsPresentationPlaying() const { return cinematicPreview || presentationPlaying; }
    bool IsBattlePaused() const { return cinematicPreview || battlePaused; }
    // 任意Sequenceのプレビュー中は、Actionによる解除と独立して戦闘入力を抑制する。
    void BeginCinematicPreview(GameProgressState context_);
    bool IsPhaseTransitionPreview() const { return phaseTransitionPreview; }

    void SetGameplayLocked(bool locked_);
    void SetPlayerControlLocked(bool locked_);
    void SetEnemyAILocked(bool locked_);
    void SetPresentationPlaying(bool playing_);
    void SetBattlePaused(bool paused_);

    int GetCurrentBattlePhase() const { return currentBattlePhase; }
    void SetCurrentBattlePhase(int phase_);

    float GetPlayerHpRate() const { return playerHpRate; }
    void SetPlayerHpRate(float rate_);

    float GetBossHpRate() const { return bossHpRate; }
    void SetBossHpRate(float rate_);

    void SetAIDirectorPreferDelayedAttack(bool enabled_);
    void SetAIDirectorPreferFeintAttack(bool enabled_);
    void SetAIDirectorPreferGuardBreak(bool enabled_);
    void SetAIDirectorReduceAggression(bool enabled_);

    void SetAIDirectorAttackDelayRate(float rate_);
    void SetAIDirectorFeintRate(float rate_);
    void SetAIDirectorGuardBreakRate(float rate_);
    void SetAIDirectorAggressionRate(float rate_);

    bool GetAIDirectorPreferDelayedAttack() const { return preferDelayedAttack; }
    bool GetAIDirectorPreferFeintAttack() const { return preferFeintAttack; }
    bool GetAIDirectorPreferGuardBreak() const { return preferGuardBreak; }
    bool GetAIDirectorReduceAggression() const { return reduceAggression; }

    float GetAIDirectorAttackDelayRate() const { return attackDelayRate; }
    float GetAIDirectorFeintRate() const { return feintRate; }
    float GetAIDirectorGuardBreakRate() const { return guardBreakRate; }
    float GetAIDirectorAggressionRate() const { return aggressionRate; }

    // 現在保持している状態値をカテゴリ別にGlobalBlackboardへ反映する。
    void WriteBlackboard();
    // GameModeManagerへ向けたGlobal Actionを登録する。
    void RegisterGlobalActions();
    // 登録済みのGameMode用Global Actionを解除する。
    void UnregisterGlobalActions();

    // 演出Sequenceの終了通知を受け取り、現在の進行状態から次の状態へ進める。
    void NotifySequenceFinished();
    // 第一形態Bossの撃破通知を受け取り、形態移行演出へ進める。
    void NotifyPhase1BossDefeated();
    // 第二形態Bossの撃破通知を受け取り、Boss撃破演出へ進める。
    void NotifyPhase2BossDefeated();
    // Boss登場演出を開始する。
    void BeginBossIntro();
    // 第一形態撃破から形態移行演出を開始する。
    void BeginPhase1Defeat();
    // 第二形態開始演出を開始する。
    void BeginPhase2Intro();
    // Boss撃破演出を開始する。
    void BeginBossDefeated();

private:
    // HP割合やAI重みなど、0から1の範囲で扱う値を補正する。
    float ClampRate(float value_) const;
    // 進行状態を切り替え、対応するロック状態を反映する。
    void SetGameState(GameProgressState state_);
    // 現在の進行状態に応じてPlayer操作とEnemy AIのロックを設定する。
    void ApplyStateLocks();
    // 指定されたSequenceを次のUpdateで開始するよう予約する。
    void RequestSequence(const std::string& sequenceName_);
    // 登録済みActionを確認し、予約中のSequenceを開始する。
    bool TryStartRequestedSequence();
    // 第一形態の通常戦闘へ移行する。
    void TransitionToPhase1Battle();
    // 第二形態の通常戦闘へ移行する。
    void TransitionToPhase2Battle();
    // Result状態へ移行する。
    void TransitionToResult();

private:
    // ゲーム全体で共有するBlackboard本体。
    BlackboardComponent globalBlackboard;

    GameProgressState currentGameState = GameProgressState::GAME_SCENE_START;
    std::string currentSequenceName;
    bool sequenceStartRequested = false;
    bool sequenceStartAttempted = false;
    bool globalActionsRegistered = false;
    bool phaseTransitionPreview = false; // 第一形態戦闘中の単体演出プレビューだけを区別する。
    bool cinematicPreview = false;

    // Gameplay全体、Player操作、Enemy AI、演出、戦闘停止のロック状態。
    bool gameplayLocked = false;
    bool playerControlLocked = false;
    bool enemyAILocked = false;
    bool presentationPlaying = false;
    bool battlePaused = false;

    // 現在の戦闘フェーズと、Player/BossのHP割合。
    int currentBattlePhase = 0;
    float playerHpRate = 1.0f;
    float bossHpRate = 1.0f;

    // AIDirectorへ渡す攻撃傾向の優先フラグ。
    bool preferDelayedAttack = false;
    bool preferFeintAttack = false;
    bool preferGuardBreak = false;
    bool reduceAggression = false;

    // AIDirectorが攻撃選択の重み付けに使用する係数。
    float attackDelayRate = 1.0f;
    float feintRate = 0.0f;
    float guardBreakRate = 0.0f;
    float aggressionRate = 1.0f;
};
