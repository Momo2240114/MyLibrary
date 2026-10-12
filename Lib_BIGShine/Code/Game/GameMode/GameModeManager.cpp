#include "PrecompiledHeader.h"

#include "GameModeManager.h"

#include "GameModeBlackboardKeys.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"

namespace
{
    constexpr const char* ACTION_CATEGORY_GAME_MODE = "GameMode";
    constexpr const char* ACTION_SEQUENCE_FINISHED = "GameMode.SequenceFinished";
    constexpr const char* ACTION_NOTIFY_PHASE_1_BOSS_DEFEATED = "GameMode.NotifyPhase1BossDefeated";
    constexpr const char* ACTION_NOTIFY_PHASE_2_BOSS_DEFEATED = "GameMode.NotifyPhase2BossDefeated";
    constexpr const char* ACTION_BEGIN_BOSS_INTRO = "GameMode.BeginBossIntro";
    constexpr const char* ACTION_BEGIN_PHASE_1_DEFEAT = "GameMode.BeginPhase1Defeat";
    constexpr const char* ACTION_BEGIN_PHASE_2_INTRO = "GameMode.BeginPhase2Intro";
    constexpr const char* ACTION_BEGIN_BOSS_DEFEATED = "GameMode.BeginBossDefeated";

    constexpr const char* SEQUENCE_BOSS_INTRO = "BossIntro";
    constexpr const char* SEQUENCE_PHASE_1_DEFEAT = "Phase1Defeat";
    constexpr const char* SEQUENCE_PHASE_2_INTRO = "Phase2Intro";
    constexpr const char* SEQUENCE_BOSS_DEFEATED = "BossDefeated";
}

// 現在の初期値をGlobalBlackboardへ流し込み、他Systemが参照できる状態にする。
void GameModeManager::Start()
{
    WriteBlackboard();
}

// Manager自身の状態を正として、毎フレームGlobalBlackboardへ同期する。
void GameModeManager::Update(float deltaTime_)
{
    BS_PROFILE_SCOPE("GameModeManager::Update");

    (void)deltaTime_;

    TryStartRequestedSequence();
    WriteBlackboard();
}

// 戦闘開始前の状態へ戻し、ロックやAI方針が前回戦闘から残らないようにする。
void GameModeManager::ResetBattleState()
{
    gameplayLocked = false;
    playerControlLocked = false;
    enemyAILocked = false;
    presentationPlaying = false;
    battlePaused = false;

    currentGameState = GameProgressState::GAME_SCENE_START;
    currentSequenceName.clear();
    sequenceStartRequested = false;
    sequenceStartAttempted = false;

    phaseTransitionPreview = false;

    currentBattlePhase = 0;
    playerHpRate = 1.0f;
    bossHpRate = 1.0f;

    preferDelayedAttack = false;
    preferFeintAttack = false;
    preferGuardBreak = false;
    reduceAggression = false;

    attackDelayRate = 1.0f;
    feintRate = 0.0f;
    guardBreakRate = 0.0f;
    aggressionRate = 1.0f;

    WriteBlackboard();
}

void GameModeManager::BeginGameSceneFlow()
{
    if (currentGameState != GameProgressState::GAME_SCENE_START)
    {
        return;
    }

    BeginBossIntro();
}

void GameModeManager::SetGameplayLocked(bool locked_)
{
    gameplayLocked = locked_;
    WriteBlackboard();
}

void GameModeManager::SetPlayerControlLocked(bool locked_)
{
    playerControlLocked = locked_;
    WriteBlackboard();
}

void GameModeManager::SetEnemyAILocked(bool locked_)
{
    enemyAILocked = locked_;
    WriteBlackboard();
}

void GameModeManager::SetPresentationPlaying(bool playing_)
{
    presentationPlaying = playing_;
    WriteBlackboard();
}

void GameModeManager::SetBattlePaused(bool paused_)
{
    battlePaused = paused_;
    WriteBlackboard();
}

void GameModeManager::SetCurrentBattlePhase(int phase_)
{
    currentBattlePhase = phase_;
    WriteBlackboard();
}

void GameModeManager::SetPlayerHpRate(float rate_)
{
    // HP割合はUIやAI条件でそのまま使うため、0から1の範囲へ丸めて保存する。
    playerHpRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetBossHpRate(float rate_)
{
    // Boss側も同じ正規化割合として扱い、Blackboardの利用側に補正を持たせない。
    bossHpRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorPreferDelayedAttack(bool enabled_)
{
    preferDelayedAttack = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorPreferFeintAttack(bool enabled_)
{
    preferFeintAttack = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorPreferGuardBreak(bool enabled_)
{
    preferGuardBreak = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorReduceAggression(bool enabled_)
{
    reduceAggression = enabled_;
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorAttackDelayRate(float rate_)
{
    // 攻撃遅延率は倍率として扱うため、上限は設けず負値だけを防ぐ。
    attackDelayRate = (std::max)(0.0f, rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorFeintRate(float rate_)
{
    feintRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorGuardBreakRate(float rate_)
{
    guardBreakRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::SetAIDirectorAggressionRate(float rate_)
{
    aggressionRate = ClampRate(rate_);
    WriteBlackboard();
}

void GameModeManager::WriteBlackboard()
{
    // Gameカテゴリには操作や演出のロック状態を集約する。
    globalBlackboard.SetInt(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::CURRENT_GAME_STATE,
        static_cast<int>(currentGameState));

    globalBlackboard.SetString(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::CURRENT_SEQUENCE,
        currentSequenceName);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::GAMEPLAY_LOCKED,
        gameplayLocked);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PLAYER_CONTROL_LOCKED,
        playerControlLocked);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::ENEMY_AI_LOCKED,
        enemyAILocked);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PRESENTATION_PLAYING,
        presentationPlaying);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::BATTLE_PAUSED,
        battlePaused);

    // Battleカテゴリには現在フェーズとHP割合を保存し、UIやAIが同じ値を参照できるようにする。
    globalBlackboard.SetInt(
        GameModeBlackboardKeys::CATEGORY_BATTLE,
        GameModeBlackboardKeys::CURRENT_BATTLE_PHASE,
        currentBattlePhase);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_BATTLE,
        GameModeBlackboardKeys::PLAYER_HP_RATE,
        playerHpRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_BATTLE,
        GameModeBlackboardKeys::BOSS_HP_RATE,
        bossHpRate);

    // AIDirectorカテゴリには攻撃選択へ影響する方針フラグと重みを保存する。
    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::PREFER_DELAYED_ATTACK,
        preferDelayedAttack);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::PREFER_FEINT_ATTACK,
        preferFeintAttack);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::PREFER_GUARD_BREAK,
        preferGuardBreak);

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::REDUCE_AGGRESSION,
        reduceAggression);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::ATTACK_DELAY_RATE,
        attackDelayRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::FEINT_RATE,
        feintRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::GUARD_BREAK_RATE,
        guardBreakRate);

    globalBlackboard.SetFloat(
        GameModeBlackboardKeys::CATEGORY_AI_DIRECTOR,
        GameModeBlackboardKeys::AGGRESSION_RATE,
        aggressionRate);
}

float GameModeManager::ClampRate(float value_) const
{
    // 割合として扱う値は、Blackboardへ書く前に有効範囲へ収める。
    return std::clamp(value_, 0.0f, 1.0f);
}

const char* GameModeManager::GetCurrentGameStateName() const
{
    switch (currentGameState)
    {
    case GameProgressState::GAME_SCENE_START: return "GAME_SCENE_START";
    case GameProgressState::BOSS_INTRO: return "BOSS_INTRO";
    case GameProgressState::PHASE_1_BATTLE: return "PHASE_1_BATTLE";
    case GameProgressState::PHASE_1_DEFEAT: return "PHASE_1_DEFEAT";
    case GameProgressState::PHASE_2_INTRO: return "PHASE_2_INTRO";
    case GameProgressState::PHASE_2_BATTLE: return "PHASE_2_BATTLE";
    case GameProgressState::BOSS_DEFEATED: return "BOSS_DEFEATED";
    case GameProgressState::RESULT: return "RESULT";
    default: return "UNKNOWN";
    }
}

void GameModeManager::RegisterGlobalActions()
{
    if (globalActionsRegistered) UnregisterGlobalActions();

    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_SEQUENCE_FINISHED, [this]() { NotifySequenceFinished(); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_NOTIFY_PHASE_1_BOSS_DEFEATED, [this]() { NotifyPhase1BossDefeated(); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_NOTIFY_PHASE_2_BOSS_DEFEATED, [this]() { NotifyPhase2BossDefeated(); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_BOSS_INTRO, [this]() { BeginBossIntro(); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_PHASE_1_DEFEAT, [this]() { BeginPhase1Defeat(); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_PHASE_2_INTRO, [this]() { BeginPhase2Intro(); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_BOSS_DEFEATED, [this]() { BeginBossDefeated(); });
    registry_.RemoveAction("Sequence", SEQUENCE_BOSS_DEFEATED);
    // BossIntroはScene上のActionSequenceから登録されるまで待ち、仮Actionで戦闘へ進めない。
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "DisablePlayerControl", [this]() { SetPlayerControlLocked(true); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "EnablePlayerControl", [this]() { SetPlayerControlLocked(false); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "DisableEnemyAI", [this]() { SetEnemyAILocked(true); });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "EnableEnemyAI", [this]() { SetEnemyAILocked(false); });
    // 形態移行はSceneのSequence登録を待ち、演出を飛ばして戦闘を再開しない。
    registry_.RegisterAction("Sequence", SEQUENCE_BOSS_DEFEATED, [this]() { NotifySequenceFinished(); });
    // 単体再生もSequenceへ委ね、Managerはプレビューのロックと終了状態だけを管理する。
    const char* PREVIEW_PHASES[] = { "DragonDeath", "DragonDissolve", "StageCorruption", "MudAttack",
        "PlayerCounter", "HumanFormation", "SwordClash", "PlayerKnockdown" };
    for (const char* phase_ : PREVIEW_PHASES)
    {
        const std::string sequence_ = std::string("Preview") + phase_;
        registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, std::string("GameMode.") + sequence_, [this, sequence_]()
        {
            if (currentGameState != GameProgressState::PHASE_1_BATTLE && !phaseTransitionPreview) return;
            if (!ActionRegistry::Instance().HasAction(sequence_)) return;
            phaseTransitionPreview = true;
            SetGameState(GameProgressState::PHASE_1_DEFEAT);
            RequestSequence(sequence_);
        });
    }
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "GameMode.CancelPhaseTransitionPreview", [this]()
    {
        if (phaseTransitionPreview) RequestSequence("PhaseTransitionRestore");
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "GameMode.EndPhaseTransitionPreview", [this]()
    {
        if (!phaseTransitionPreview) return;
        phaseTransitionPreview = false;
        TransitionToPhase1Battle();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "GameMode.SkipPhaseTransition", [this]()
    {
        if (phaseTransitionPreview)
        {
            RequestSequence("PhaseTransitionRestore");
            return;
        }
        if (currentGameState != GameProgressState::PHASE_1_DEFEAT && currentGameState != GameProgressState::PHASE_2_INTRO) return;
        SetGameState(GameProgressState::PHASE_2_INTRO);
        RequestSequence("PhaseTransitionBattleResume");
    });
    globalActionsRegistered = true;
}

void GameModeManager::UnregisterGlobalActions()
{
    if (!globalActionsRegistered) return;

    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "GameMode.SkipPhaseTransition");
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_SEQUENCE_FINISHED);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_NOTIFY_PHASE_1_BOSS_DEFEATED);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_NOTIFY_PHASE_2_BOSS_DEFEATED);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_BOSS_INTRO);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_PHASE_1_DEFEAT);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_PHASE_2_INTRO);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_BOSS_DEFEATED);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "DisablePlayerControl");
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "EnablePlayerControl");
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "DisableEnemyAI");
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "EnableEnemyAI");
    const char* PREVIEW_PHASES[] = { "DragonDeath", "DragonDissolve", "StageCorruption", "MudAttack",
        "PlayerCounter", "HumanFormation", "SwordClash", "PlayerKnockdown" };
    for (const char* phase_ : PREVIEW_PHASES)
        registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, std::string("GameMode.Preview") + phase_);
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "GameMode.CancelPhaseTransitionPreview");
    registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, "GameMode.EndPhaseTransitionPreview");
    registry_.RemoveAction("Sequence", SEQUENCE_BOSS_INTRO);
    registry_.RemoveAction("Sequence", SEQUENCE_PHASE_1_DEFEAT);
    registry_.RemoveAction("Sequence", SEQUENCE_PHASE_2_INTRO);
    registry_.RemoveAction("Sequence", SEQUENCE_BOSS_DEFEATED);
    globalActionsRegistered = false;
}

void GameModeManager::NotifySequenceFinished()
{
    switch (currentGameState)
    {
    case GameProgressState::BOSS_INTRO:
        TransitionToPhase1Battle();
        break;
    case GameProgressState::PHASE_1_DEFEAT:
        BeginPhase2Intro();
        break;
    case GameProgressState::PHASE_2_INTRO:
        TransitionToPhase2Battle();
        break;
    case GameProgressState::BOSS_DEFEATED:
        TransitionToResult();
        break;
    default:
        break;
    }
}

void GameModeManager::NotifyPhase1BossDefeated()
{
    if (currentGameState != GameProgressState::PHASE_1_BATTLE)
    {
        return;
    }

    BeginPhase1Defeat();
}

void GameModeManager::NotifyPhase2BossDefeated()
{
    if (currentGameState != GameProgressState::PHASE_2_BATTLE)
    {
        return;
    }

    BeginBossDefeated();
}

void GameModeManager::BeginBossIntro()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginBossIntro");
    // ゲーム開始時だけ開幕演出を予約し、戦闘中の再呼び出しを無効にする。
    if (currentGameState != GameProgressState::GAME_SCENE_START) return;

    SetGameState(GameProgressState::BOSS_INTRO);
    RequestSequence(SEQUENCE_BOSS_INTRO);
}

void GameModeManager::BeginPhase1Defeat()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginPhase1Defeat");
    // 第一形態の戦闘中だけ撃破演出へ進め、重複通知による再開始を防ぐ。
    if (currentGameState != GameProgressState::PHASE_1_BATTLE) return;

    SetGameState(GameProgressState::PHASE_1_DEFEAT);
    RequestSequence(SEQUENCE_PHASE_1_DEFEAT);
}

void GameModeManager::BeginPhase2Intro()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginPhase2Intro");
    // 第一形態の撃破演出から第二形態への移行演出を開始する。
    if (currentGameState != GameProgressState::PHASE_1_DEFEAT) return;

    SetGameState(GameProgressState::PHASE_2_INTRO);
    RequestSequence(SEQUENCE_PHASE_2_INTRO);
}

void GameModeManager::BeginBossDefeated()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginBossDefeated");
    // 第二形態の戦闘中だけ最終撃破演出を開始する。
    if (currentGameState != GameProgressState::PHASE_2_BATTLE) return;

    SetGameState(GameProgressState::BOSS_DEFEATED);
    RequestSequence(SEQUENCE_BOSS_DEFEATED);
}

void GameModeManager::SetGameState(GameProgressState state_)
{
    currentGameState = state_;
    ApplyStateLocks();
    WriteBlackboard();
}

void GameModeManager::ApplyStateLocks()
{
    const bool isSequenceState_ =
        currentGameState == GameProgressState::BOSS_INTRO ||
        currentGameState == GameProgressState::PHASE_1_DEFEAT ||
        currentGameState == GameProgressState::PHASE_2_INTRO ||
        currentGameState == GameProgressState::BOSS_DEFEATED;

    presentationPlaying = isSequenceState_;
    playerControlLocked = isSequenceState_;
    enemyAILocked = isSequenceState_;
    gameplayLocked = false;
    battlePaused = false;

    switch (currentGameState)
    {
    case GameProgressState::PHASE_1_BATTLE:
    case GameProgressState::PHASE_1_DEFEAT:
        currentBattlePhase = static_cast<int>(BossBattlePhase::PHASE_1);
        break;
    case GameProgressState::PHASE_2_INTRO:
    case GameProgressState::PHASE_2_BATTLE:
        currentBattlePhase = static_cast<int>(BossBattlePhase::PHASE_2);
        break;
    case GameProgressState::BOSS_DEFEATED:
    case GameProgressState::RESULT:
        currentBattlePhase = static_cast<int>(BossBattlePhase::DEFEATED);
        break;
    default:
        currentBattlePhase = static_cast<int>(BossBattlePhase::NONE);
        break;
    }
}

void GameModeManager::RequestSequence(const std::string& sequenceName_)
{
    currentSequenceName = sequenceName_;
    sequenceStartRequested = !currentSequenceName.empty();
    sequenceStartAttempted = false;
    WriteBlackboard();
}

bool GameModeManager::TryStartRequestedSequence()
{
    if (!sequenceStartRequested || currentSequenceName.empty())
    {
        return false;
    }

    ActionRegistry& registry_ = ActionRegistry::Instance();
    if (!registry_.HasAction(currentSequenceName))
    {
        if (!sequenceStartAttempted)
        {
            sequenceStartAttempted = true;
        }
        return false;
    }

    const std::string sequenceName_ = currentSequenceName;
    sequenceStartRequested = false;
    sequenceStartAttempted = true;
    registry_.Call(sequenceName_);
    return true;
}

void GameModeManager::TransitionToPhase1Battle()
{
    currentSequenceName.clear();
    sequenceStartRequested = false;
    SetGameState(GameProgressState::PHASE_1_BATTLE);
}

void GameModeManager::TransitionToPhase2Battle()
{
    currentSequenceName.clear();
    sequenceStartRequested = false;
    SetGameState(GameProgressState::PHASE_2_BATTLE);
}

void GameModeManager::TransitionToResult()
{
    currentSequenceName.clear();
    sequenceStartRequested = false;
    SetGameState(GameProgressState::RESULT);
}
