#include "PlayerIdleState.h"
#include "PlayerControllerComponent.h"

//ステートのinclude例
//#include "PlayerMoveState.h"
//#include "PlayerJumpState.h"
//#include "PlayerFallState.h"

void PlayerIdleState::Enter(PlayerControllerComponent& player_)
{
    player_.SetHorizontalVelocity({ 0, 0, 0 }, 0.0f);
}

void PlayerIdleState::Update(PlayerControllerComponent& player_, float dt_)
{
    auto input = player_.GetMoveInput();

    if (player_.IsJumpPressed() && player_.IsGrounded())
    {
        player_.Jump();
        // player.ChangeState が必要なので後述
        return;
    }

    if (!player_.IsGrounded())
    {
        // Fallへ
        return;
    }

    if (input.x != 0.0f || input.z != 0.0f)
    {
        // Moveへ
        return;
    }
}