#include "PrecompiledHeader.h"

#include "GameModeManager.h"

#include "GameModeBlackboardKeys.h"
#include "Engine/GamePlay/Serialize/ActionRegistry.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/Debug/TracyProfiler.h"
#include "Engine/GamePlay/Core/Object/GameObjectManager.h"
#include "Game/AI/Dragon/DragonAttackControllerComponent.h"
#include "Game/AI/Human/HumanBossAIComponent.h"
#include "Game/AI/Human/HumanBossActionComponent.h"

#include "Engine/GamePlay/Input/KeyConfig.h"
#include "Engine/Graphics/Core/GraphicsDeviceManager.h"
#include <imgui.h>
#include <cmath>

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

    constexpr float SKIP_HOLD_SECONDS = 1.0f;
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
void GameModeManager::Update(float deltaTime_, GameObjectManager& manager_)
{
    BS_PROFILE_SCOPE("GameModeManager::Update");

    if (IsBattleResultTracking() && !playerControlLocked && std::isfinite(deltaTime_) && deltaTime_ > 0.0f &&
        !IsBossSpecialActionActive(manager_))
    {
        battleResult.elapsedSeconds += deltaTime_;
    }

    const bool acceptsInput_ = !manager_.SupportsEditorPlayMode() || manager_.IsPlayInputCaptured();
    if (CanSkipPresentation() && acceptsInput_ &&
        GetForegroundWindow() == GraphicsDeviceManager::Instance().GetWindowHandle() &&
        (KeyInput::KeyPressed(VK_ESCAPE) || ControllerInput::Instance().ButtonPressed(PadCode::Y)))
    {
        skipHoldSeconds += (std::max)(0.0f, deltaTime_);
        if (skipHoldSeconds >= SKIP_HOLD_SECONDS)
        {
            skipHoldSeconds = 0.0f;
            if (currentSequenceName == SEQUENCE_BOSS_INTRO)
            {
                RequestSequence("BossIntroBattleResume");
            }
            else
            {
                ActionRegistry::Instance().Call("GameMode.SkipPhaseTransition");
            }
        }
    }
    else
    {
        skipHoldSeconds = 0.0f;
    }

    TryStartRequestedSequence();
    WriteBlackboard();
}

bool GameModeManager::CanSkipPresentation() const
{
    return !cinematicPreview && !phaseTransitionPreview &&
        (currentSequenceName == SEQUENCE_BOSS_INTRO || currentSequenceName == SEQUENCE_PHASE_1_DEFEAT ||
            currentSequenceName == SEQUENCE_PHASE_2_INTRO);
}

void GameModeManager::DrawSkipPrompt(ImDrawList& draw_, const ImVec2& minimum_, const ImVec2& maximum_) const
{
    if (!CanSkipPresentation())
    {
        return;
    }

    const float scale_ = (std::min)((maximum_.x - minimum_.x) / 1280.0f, (maximum_.y - minimum_.y) / 720.0f);
    const char* label_ = IM_U8("長押しでスキップ");
    const float fontSize_ = 18.0f * scale_;
    const auto font_ = ImGui::GetFont();
    const float width_ = font_->CalcTextSizeA(fontSize_, FLT_MAX, 0.0f, label_).x;
    const ImVec2 center_(maximum_.x - width_ - 58.0f * scale_, maximum_.y - 40.0f * scale_);
    const float radius_ = 20.0f * scale_;
    draw_.AddRectFilled(ImVec2(center_.x - radius_ - 6.0f * scale_, center_.y - radius_ - 4.0f * scale_),
        ImVec2(maximum_.x - 16.0f * scale_, center_.y + radius_ + 4.0f * scale_), IM_COL32(0, 0, 0, 175), radius_);
    draw_.AddText(font_, fontSize_, ImVec2(center_.x + radius_ + 8.0f * scale_, center_.y - fontSize_ * 0.5f),
        IM_COL32_WHITE, label_);
    draw_.AddCircle(center_, radius_, IM_COL32(255, 255, 255, 90), 64, 2.0f * scale_);
    const auto device_ = KeyConfig::Instance().GetActiveDevice();
    if (device_ == InputDisplayDevice::PLAYSTATION_PAD)
    {
        draw_.AddTriangle(ImVec2(center_.x, center_.y - 9.0f * scale_),
            ImVec2(center_.x + 10.0f * scale_, center_.y + 8.0f * scale_),
            ImVec2(center_.x - 10.0f * scale_, center_.y + 8.0f * scale_), IM_COL32_WHITE, 2.0f * scale_);
    }
    else
    {
        const char* key_ = device_ == InputDisplayDevice::KEYBOARD_MOUSE ? "Esc" : "Y";
        const float keyWidth_ = font_->CalcTextSizeA(fontSize_, FLT_MAX, 0.0f, key_).x;
        draw_.AddText(font_, fontSize_, ImVec2(center_.x - keyWidth_ * 0.5f, center_.y - fontSize_ * 0.5f), IM_COL32_WHITE, key_);
    }
    if (skipHoldSeconds > 0.0f)
    {
        const float angle_ = DirectX::XM_2PI * skipHoldSeconds / SKIP_HOLD_SECONDS;
        draw_.PathArcTo(center_, radius_, -DirectX::XM_PIDIV2, angle_ - DirectX::XM_PIDIV2, 64);
        draw_.PathStroke(IM_COL32_WHITE, 0, 3.0f * scale_);
    }
}

bool GameModeManager::IsBattleResultTracking() const
{
    return !cinematicPreview && !battlePaused && !gameplayLocked && !presentationPlaying &&
        (currentGameState == GameProgressState::PHASE_1_BATTLE ||
            currentGameState == GameProgressState::PHASE_2_BATTLE);
}

bool GameModeManager::IsBossSpecialActionActive(GameObjectManager& manager_) const
{
    if (currentGameState == GameProgressState::PHASE_1_BATTLE)
    {
        const auto dragon_ = manager_.Find("TestTarget");
        const auto attacks_ = dragon_ ? dragon_->GetComponent<DragonAttackControllerComponent>() : nullptr;
        return attacks_ && attacks_->IsEnabled() && attacks_->IsSpecialActionActive();
    }

    const auto boss_ = manager_.Find("HumanBoss");
    if (!boss_)
    {
        return false;
    }
    const auto action_ = boss_->GetComponent<HumanBossActionComponent>();
    const auto ai_ = boss_->GetComponent<HumanBossAIComponent>();
    return (action_ && action_->IsEnabled() && action_->IsSpecialActionActive()) ||
        (ai_ && ai_->IsEnabled() && ai_->IsBerserkInvulnerable());
}

void GameModeManager::RecordPlayerDefense(bool justDodge_)
{
    if (IsBattleResultTracking())
    {
        ++(justDodge_ ? battleResult.justDodges : battleResult.justGuards);
    }
}

void GameModeManager::RecordPlayerDamage(float damage_)
{
    if (IsBattleResultTracking() && std::isfinite(damage_) && damage_ > 0.0f)
    {
        battleResult.damageTaken += damage_;
    }
}

void GameModeManager::UpdateBattleMusic(GameObjectManager& manager_)
{
    BS_PROFILE_SCOPE("GameModeManager::UpdateBattleMusic");
    // 死亡・形態移行・撃破演出では、Sequenceと専用Directorの音声制御を優先する。
    if (cinematicPreview || presentationPlaying || gameplayLocked || battlePaused ||
        (currentGameState != GameProgressState::PHASE_1_BATTLE &&
            currentGameState != GameProgressState::PHASE_2_BATTLE))
    {
        return;
    }

    size_t musicIndex_ = currentGameState == GameProgressState::PHASE_1_BATTLE
        ? BGM_GAME : BGM_HUMAN_BOSS;
    if (currentGameState == GameProgressState::PHASE_2_BATTLE)
    {
        bool berserk_ = false;
        bool special_ = false;
        for (const auto& boss_ : manager_.FindComponents<HumanBossAIComponent>())
        {
            const auto owner_ = boss_ ? boss_->GetGameObject() : nullptr;
            if (!boss_ || !boss_->IsEnabled() || !owner_ || owner_->IsPendingDestroy())
            {
                continue;
            }
            berserk_ = berserk_ || boss_->IsBerserkActive();
            const auto action_ = owner_->GetComponent<HumanBossActionComponent>();
            special_ = special_ || (action_ && action_->IsEnabled() && action_->IsSpecialMusicActive());
        }
        // 大技を最優先にし、終了後は暴走が続いていれば暴走曲へ戻す。
        if (special_)
        {
            musicIndex_ = BGM_SPECIAL_ATTACK;
        }
        else if (berserk_)
        {
            musicIndex_ = BGM_BERSERK;
        }
    }
    AudioManager::Instance().SwitchBGM(musicIndex_);
}

// 戦闘開始前の状態へ戻し、ロックやAI方針が前回戦闘から残らないようにする。
void GameModeManager::ResetBattleState()
{
    gameplayLocked = false;
    playerControlLocked = false;
    enemyAILocked = false;
    presentationPlaying = false;
    battlePaused = false;

    skipHoldSeconds = 0.0f;
    currentGameState = GameProgressState::GAME_SCENE_START;
    currentSequenceName.clear();
    sequenceStartRequested = false;
    sequenceStartAttempted = false;

    phaseTransitionPreview = false;
    cinematicPreview = false;

    battleResult = {};
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

void GameModeManager::BeginCinematicPreview(GameProgressState context_)
{
    BS_PROFILE_SCOPE("GameModeManager::BeginCinematicPreview");
    cinematicPreview = true;
    sequenceStartRequested = false;
    sequenceStartAttempted = false;
    SetGameState(context_);
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
        IsGameplayLocked());

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PLAYER_CONTROL_LOCKED,
        IsPlayerControlLocked());

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::ENEMY_AI_LOCKED,
        IsEnemyAILocked());

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::PRESENTATION_PLAYING,
        IsPresentationPlaying());

    globalBlackboard.SetBool(
        GameModeBlackboardKeys::CATEGORY_GAME,
        GameModeBlackboardKeys::BATTLE_PAUSED,
        IsBattlePaused());

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
    if (globalActionsRegistered)
    {
        UnregisterGlobalActions();
    }

    ActionRegistry& registry_ = ActionRegistry::Instance();
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_SEQUENCE_FINISHED, [this]()
    {
        NotifySequenceFinished();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_NOTIFY_PHASE_1_BOSS_DEFEATED, [this]()
    {
        NotifyPhase1BossDefeated();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_NOTIFY_PHASE_2_BOSS_DEFEATED, [this]()
    {
        NotifyPhase2BossDefeated();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_BOSS_INTRO, [this]()
    {
        BeginBossIntro();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_PHASE_1_DEFEAT, [this]()
    {
        BeginPhase1Defeat();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_PHASE_2_INTRO, [this]()
    {
        BeginPhase2Intro();
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, ACTION_BEGIN_BOSS_DEFEATED, [this]()
    {
        BeginBossDefeated();
    });
    registry_.RemoveAction("Sequence", SEQUENCE_BOSS_DEFEATED);
    // BossIntroはScene上のActionSequenceから登録されるまで待ち、仮Actionで戦闘へ進めない。
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "DisablePlayerControl", [this]()
    {
        SetPlayerControlLocked(true);
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "EnablePlayerControl", [this]()
    {
        SetPlayerControlLocked(false);
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "DisableEnemyAI", [this]()
    {
        SetEnemyAILocked(true);
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "EnableEnemyAI", [this]()
    {
        SetEnemyAILocked(false);
    });
    // 形態移行はSceneのSequence登録を待ち、演出を飛ばして戦闘を再開しない。
    // 決着はGameObjectManagerの専用Directorが生の時間で更新する。
    registry_.RegisterAction("Sequence", SEQUENCE_BOSS_DEFEATED, []()
    {
    });
    // 単体再生もSequenceへ委ね、Managerはプレビューのロックと終了状態だけを管理する。
    const char* PREVIEW_PHASES[] = { "DragonDeath", "DragonDissolve", "StageCorruption", "MudAttack",
        "PlayerCounter", "HumanFormation", "SwordClash", "PlayerKnockdown" };
    for (const char* phase_ : PREVIEW_PHASES)
    {
        const std::string sequence_ = std::string("Preview") + phase_;
        registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, std::string("GameMode.") + sequence_, [this, sequence_]()
        {
            if (currentGameState != GameProgressState::PHASE_1_BATTLE && !phaseTransitionPreview)
            {
                return;
            }
            if (!ActionRegistry::Instance().HasAction(sequence_))
            {
                return;
            }
            phaseTransitionPreview = true;
            SetGameState(GameProgressState::PHASE_1_DEFEAT);
            RequestSequence(sequence_);
        });
    }
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "GameMode.CancelPhaseTransitionPreview", [this]()
    {
        if (phaseTransitionPreview)
        {
            RequestSequence("PhaseTransitionRestore");
        }
    });
    registry_.RegisterAction(ACTION_CATEGORY_GAME_MODE, "GameMode.EndPhaseTransitionPreview", [this]()
    {
        if (!phaseTransitionPreview)
        {
            return;
        }
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
        if (currentGameState != GameProgressState::PHASE_1_DEFEAT && currentGameState != GameProgressState::PHASE_2_INTRO)
        {
            return;
        }
        SetGameState(GameProgressState::PHASE_2_INTRO);
        RequestSequence("PhaseTransitionBattleResume");
    });
    globalActionsRegistered = true;
}

void GameModeManager::UnregisterGlobalActions()
{
    if (!globalActionsRegistered)
    {
        return;
    }

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
    {
        registry_.RemoveAction(ACTION_CATEGORY_GAME_MODE, std::string("GameMode.Preview") + phase_);
    }
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
    if (currentGameState != GameProgressState::GAME_SCENE_START)
    {
        return;
    }

    SetGameState(GameProgressState::BOSS_INTRO);
    RequestSequence(SEQUENCE_BOSS_INTRO);
}

void GameModeManager::BeginPhase1Defeat()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginPhase1Defeat");
    // 第一形態の戦闘中だけ撃破演出へ進め、重複通知による再開始を防ぐ。
    if (currentGameState != GameProgressState::PHASE_1_BATTLE)
    {
        return;
    }

    SetGameState(GameProgressState::PHASE_1_DEFEAT);
    RequestSequence(SEQUENCE_PHASE_1_DEFEAT);
}

void GameModeManager::BeginPhase2Intro()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginPhase2Intro");
    // 第一形態の撃破演出から第二形態への移行演出を開始する。
    if (currentGameState != GameProgressState::PHASE_1_DEFEAT)
    {
        return;
    }

    SetGameState(GameProgressState::PHASE_2_INTRO);
    RequestSequence(SEQUENCE_PHASE_2_INTRO);
}

void GameModeManager::BeginBossDefeated()
{
    BS_PROFILE_SCOPE("GameModeManager::BeginBossDefeated");
    // 第二形態の戦闘中だけ最終撃破演出を開始する。
    if (currentGameState != GameProgressState::PHASE_2_BATTLE)
    {
        return;
    }

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
        currentGameState == GameProgressState::BOSS_DEFEATED ||
        currentGameState == GameProgressState::RESULT;

    presentationPlaying = isSequenceState_;
    playerControlLocked = isSequenceState_;
    enemyAILocked = isSequenceState_;
    gameplayLocked = currentGameState == GameProgressState::BOSS_DEFEATED || currentGameState == GameProgressState::RESULT;
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
