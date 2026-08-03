#pragma once

#include "PlayerState.h"

// Jump開始、空中Loop、二段Jump、着地Endを1つのJumpAction内で管理するState。
class PlayerJumpState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::JUMP); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;
    void NotifyJumpLoopRequested();

private:
    // Jump中のAnimation/制御フェーズ。
    enum class PLAYER_JUMP_RUNTIME_PHASE
    {
        START,
        DOUBLE_JUMP,
        LOOP,
        END
    };

    // 空中Loop中にJump入力があれば二段Jumpへ遷移する。
    void TryStartDoubleJump(
        PlayerControllerComponent& player_,
        const DirectX::XMFLOAT3& input_);
    // Runtimeフェーズを切り替え、対応するBlackboardのJumpフェーズを更新する。
    void ChangePhase(PLAYER_JUMP_RUNTIME_PHASE phase_);
    // ログ出力用にRuntimeフェーズ名を返す。
    static const char* GetJumpRuntimePhaseName(PLAYER_JUMP_RUNTIME_PHASE phase_);

    // 現在のJump Runtimeフェーズ。
    PLAYER_JUMP_RUNTIME_PHASE phase = PLAYER_JUMP_RUNTIME_PHASE::START;
    // 現フェーズに入ってからの経過時間。
    float phaseTimer = 0.0f;
    // このJumpAction中に実行したJump回数。
    int jumpCount = 0;
    // 一度でも地面から離れたか。
    bool hasLeftGround = false;
    // 二段Jump用Animationへ入ったことを検出済みか。
    bool hasEnteredDoubleJumpAnimation = false;
};
