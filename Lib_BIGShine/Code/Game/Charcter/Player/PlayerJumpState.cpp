#include "PrecompiledHeader.h"

#include "PlayerJumpState.h"

#include "Engine/Common/DebugLog.h"
#include "Engine/GamePlay/Core/Component/CharacterMovementComponent.h"
#include "PlayerControllerComponent.h"

#include <algorithm>

namespace
{
    inline constexpr const char* DOUBLE_JUMP_STATE_NAME = "Double_Jump_Combat_0_Seq_0";
}

// Jump開始時の復帰要求を確認し、開始/Loop/着地Endのどこから再開するかを決める。
void PlayerJumpState::Enter()
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    const bool resumeLoop_ = player_->ConsumeJumpLoopResumeRequest();
    const bool resumeLanding_ = resumeLoop_ && player_->IsGrounded();
    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        std::string("[Jump] Enter resumeLoop=") + (resumeLoop_ ? "true" : "false") +
        " resumeLanding=" + (resumeLanding_ ? "true" : "false"));
    phaseTimer = 0.0f;
    hasLeftGround = resumeLoop_;
    hasEnteredDoubleJumpAnimation = false;
    hasAppliedJumpImpulse = resumeLoop_;
    if (!resumeLoop_)
    {
        // 新規Jumpでは1回目のJumpとして扱う。
        jumpCount = 1;
    }
    else if (jumpCount <= 0)
    {
        jumpCount = 1;
    }
    if (resumeLanding_)
    {
        // 空中Action後に既に接地している場合は、着地フェーズから再開する。
        phase = PLAYER_JUMP_RUNTIME_PHASE::END;
    }
    else if (resumeLoop_)
    {
        phase = PLAYER_JUMP_RUNTIME_PHASE::LOOP;
    }
    else
    {
        phase = PLAYER_JUMP_RUNTIME_PHASE::START;
    }
    if (resumeLanding_)
    {
        player_->EndAirborneActionContext();
    }
    else
    {
        player_->BeginAirborneActionContext();
    }
    player_->EnterAction(PLAYER_ACTION::JUMP);
    PLAYER_LOCOMOTION_PHASE locomotionPhase_ = PLAYER_LOCOMOTION_PHASE::JUMP_START;
    if (resumeLanding_)
    {
        locomotionPhase_ = PLAYER_LOCOMOTION_PHASE::JUMP_END;
    }
    else if (resumeLoop_)
    {
        locomotionPhase_ = PLAYER_LOCOMOTION_PHASE::JUMP_LOOP;
    }
    player_->SendJumpPhaseToBlackboard(locomotionPhase_);

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);
    if (!resumeLoop_ && (input_.x != 0.0f || input_.z != 0.0f))
    {
        player_->SetFacingDirection(input_);
        player_->SetHorizontalVelocity(input_, player_->moveSpeed * player_->airControlRate);
    }

    // 1段目の上方向インパルスはSequencer Actionから発火し、モーションの踏み切りに合わせる。
}

// 空中制御、二段Jump受付、着地判定を処理する。
void PlayerJumpState::Execute(float elapsedTime_)
{
    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    phaseTimer += (std::max)(elapsedTime_, 0.0f);

    const DirectX::XMFLOAT3 input_ = player_->GetMoveInput();
    player_->SendMoveInputToBlackboard(input_);

    if (phase == PLAYER_JUMP_RUNTIME_PHASE::START &&
        !hasAppliedJumpImpulse &&
        player_->jumpImpulseFallbackDuration > 0.0f &&
        phaseTimer >= player_->jumpImpulseFallbackDuration)
    {
        // Sequencer側に踏み切りActionが未設定の場合でも、一定時間後にJumpを成立させる。
        BS_LOG_WARNING(LOG_CATEGORY::GAME, "[Jump] Jump impulse fallback.");
        ApplyJumpImpulseFromSequence();
    }

    if (CharacterMovementComponent* movement_ = player_->GetMovement())
    {
        const DirectX::XMFLOAT3 currentVelocity_ = movement_->GetDesiredHorizontalVelocity();
        DirectX::XMFLOAT3 targetVelocity_{ 0.0f, 0.0f, 0.0f };
        const bool hasMoveInput_ = input_.x != 0.0f || input_.z != 0.0f;
        if (hasMoveInput_)
        {
            // 空中では通常移動速度にairControlRateを掛けた目標速度へ補間する。
            targetVelocity_ =
            {
                input_.x * player_->moveSpeed * player_->airControlRate,
                0.0f,
                input_.z * player_->moveSpeed * player_->airControlRate
            };
            movement_->SetFacingDirection(input_, false);
        }

        const float blendRate_ = hasMoveInput_
            ? player_->airControlAcceleration
            : player_->airDeceleration;
        const float blend_ = std::clamp(blendRate_ * elapsedTime_, 0.0f, 1.0f);
        DirectX::XMFLOAT3 nextVelocity_
        {
            currentVelocity_.x + (targetVelocity_.x - currentVelocity_.x) * blend_,
            0.0f,
            currentVelocity_.z + (targetVelocity_.z - currentVelocity_.z) * blend_
        };

        const float speedSquared_ =
            nextVelocity_.x * nextVelocity_.x +
            nextVelocity_.z * nextVelocity_.z;
        if (speedSquared_ < 0.0001f)
        {
            nextVelocity_ = { 0.0f, 0.0f, 0.0f };
        }

        movement_->SetHorizontalVelocity(nextVelocity_);
    }

    const bool isGrounded_ = player_->IsGrounded();
    if (!isGrounded_)
    {
        // 地面から離れたらStartからLoopへ進め、以降は二段Jump入力を監視する。
        hasLeftGround = true;
        if (phase == PLAYER_JUMP_RUNTIME_PHASE::START)
        {
            BS_LOG_INFO(LOG_CATEGORY::GAME, "[Jump] Left ground. Start -> Loop.");
            ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::LOOP);
        }
        else if (phase == PLAYER_JUMP_RUNTIME_PHASE::DOUBLE_JUMP)
        {
            if (player_->IsCurrentAnimationState(DOUBLE_JUMP_STATE_NAME))
            {
                if (!hasEnteredDoubleJumpAnimation)
                {
                    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[Jump] Double jump animation state entered.");
                }
                hasEnteredDoubleJumpAnimation = true;
            }
            else if (hasEnteredDoubleJumpAnimation)
            {
                BS_LOG_INFO(LOG_CATEGORY::GAME, "[Jump] Double jump animation state exited. Returning to Loop.");
                ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::LOOP);
            }
        }
        TryStartDoubleJump(*player_, input_);
        player_->ProcessActionInput();
        return;
    }

    player_->ProcessActionInput();

    if (hasLeftGround)
    {
        // 接地後はJumpEndフェーズへ入り、移動入力または着地時間経過でActionを終了する。
        if (phase != PLAYER_JUMP_RUNTIME_PHASE::END)
        {
            BS_LOG_INFO(LOG_CATEGORY::GAME, "[Jump] Grounded after airborne. Enter End.");
        }
        ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::END);
        const bool hasMoveInput_ = input_.x != 0.0f || input_.z != 0.0f;
        if (hasMoveInput_ ||
            (phase == PLAYER_JUMP_RUNTIME_PHASE::END &&
                phaseTimer >= player_->jumpEndDuration))
        {
            player_->NotifyActionEnd();
        }
    }
}

// Sequencerの踏み切りタイミングから1段目Jumpの上方向インパルスを適用する。
void PlayerJumpState::ApplyJumpImpulseFromSequence()
{
    if (phase != PLAYER_JUMP_RUNTIME_PHASE::START || hasAppliedJumpImpulse)
    {
        BS_LOG_WARNING(
            LOG_CATEGORY::GAME,
            std::string("[Jump] ApplyJumpImpulse ignored. phase=") + GetJumpRuntimePhaseName(phase));
        return;
    }

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    CharacterMovementComponent* movement_ = player_->GetMovement();
    if (!movement_) return;

    // 新規Jumpだけ上方向インパルスを与える。空中Action復帰や複数イベントによる二重加速を避ける。
    hasAppliedJumpImpulse = true;
    BS_LOG_INFO(LOG_CATEGORY::GAME, "[Jump] First jump impulse from sequencer.");
    movement_->AddImpulse({ 0.0f, player_->jumpImpulse, 0.0f });
}

// 空中Loop中にJump入力を消費して二段Jumpを開始する。
void PlayerJumpState::TryStartDoubleJump(
    PlayerControllerComponent& player_,
    const DirectX::XMFLOAT3& input_)
{
    if (phase != PLAYER_JUMP_RUNTIME_PHASE::LOOP) return;
    if (jumpCount >= 2)
    {
        if (player_.IsJumpInputPressed())
        {
            BS_LOG_WARNING(LOG_CATEGORY::GAME, "[Jump] Double jump rejected: jumpCount limit.");
        }
        return;
    }
    if (!player_.IsJumpInputPressed()) return;

    // 二段Jumpは垂直速度をリセットしてから専用インパルスを与え、上昇量を安定させる。
    ++jumpCount;
    hasEnteredDoubleJumpAnimation = false;
    BS_LOG_SUCCESS(LOG_CATEGORY::GAME, "[Jump] Double jump requested. Sending JumpDouble phase.");
    ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::DOUBLE_JUMP);
    player_.BeginAirborneActionContext();

    if (CharacterMovementComponent* movement_ = player_.GetMovement())
    {
        movement_->ResetVerticalVelocity();
        BS_LOG_INFO(LOG_CATEGORY::GAME, "[Jump] Double jump vertical velocity reset before impulse.");
        movement_->AddImpulse({ 0.0f, player_.doubleJumpImpulse, 0.0f });
    }

    if (input_.x != 0.0f || input_.z != 0.0f)
    {
        player_.SetFacingDirection(input_);
        player_.SetHorizontalVelocity(input_, player_.moveSpeed * player_.airControlRate);
    }
}

// JumpStateを抜けるときに空中ActionContextと入力受付状態を整理する。
void PlayerJumpState::Exit()
{
    if (PlayerControllerComponent* player_ = GetPlayerController())
    {
        BS_LOG_INFO(
            LOG_CATEGORY::GAME,
            std::string("[Jump] Exit phase=") + GetJumpRuntimePhaseName(phase));
        if (player_->IsGrounded())
        {
            player_->EndAirborneActionContext();
        }
        player_->SetCanBufferInput(false);
        player_->SetCanCombo(false);
    }
}

// Animation側から二段Jump終了を受け取り、空中Loopへ戻す。
void PlayerJumpState::NotifyJumpLoopRequested()
{
    if (phase != PLAYER_JUMP_RUNTIME_PHASE::DOUBLE_JUMP)
    {
        BS_LOG_WARNING(
            LOG_CATEGORY::GAME,
            std::string("[Jump] NotifyJumpLoop ignored. phase=") + GetJumpRuntimePhaseName(phase));
        return;
    }

    BS_LOG_INFO(LOG_CATEGORY::GAME, "[Jump] NotifyJumpLoop received from sequencer.");
    hasEnteredDoubleJumpAnimation = true;
    ChangePhase(PLAYER_JUMP_RUNTIME_PHASE::LOOP);
}

// Runtimeフェーズを更新し、AnimationStateMachine用のJumpフェーズをBlackboardへ送る。
void PlayerJumpState::ChangePhase(PLAYER_JUMP_RUNTIME_PHASE phase_)
{
    if (phase == phase_) return;

    const PLAYER_JUMP_RUNTIME_PHASE previousPhase_ = phase;
    phase = phase_;
    phaseTimer = 0.0f;

    PlayerControllerComponent* player_ = GetPlayerController();
    if (!player_) return;

    BS_LOG_INFO(
        LOG_CATEGORY::GAME,
        std::string("[Jump] Phase ") + GetJumpRuntimePhaseName(previousPhase_) +
        " -> " + GetJumpRuntimePhaseName(phase));

    switch (phase)
    {
    case PLAYER_JUMP_RUNTIME_PHASE::START:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_START);
        break;
    case PLAYER_JUMP_RUNTIME_PHASE::DOUBLE_JUMP:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_DOUBLE);
        break;
    case PLAYER_JUMP_RUNTIME_PHASE::LOOP:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_LOOP);
        break;
    case PLAYER_JUMP_RUNTIME_PHASE::END:
        player_->SendJumpPhaseToBlackboard(PLAYER_LOCOMOTION_PHASE::JUMP_END);
        break;
    default:
        break;
    }
}

const char* PlayerJumpState::GetJumpRuntimePhaseName(PLAYER_JUMP_RUNTIME_PHASE phase_)
{
    switch (phase_)
    {
    case PLAYER_JUMP_RUNTIME_PHASE::START:
        return "Start";
    case PLAYER_JUMP_RUNTIME_PHASE::DOUBLE_JUMP:
        return "DoubleJump";
    case PLAYER_JUMP_RUNTIME_PHASE::LOOP:
        return "Loop";
    case PLAYER_JUMP_RUNTIME_PHASE::END:
        return "End";
    default:
        return "Unknown";
    }
}
