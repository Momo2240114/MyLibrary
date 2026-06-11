#include "PlayerStateMachine.h"
#include "PlayerState.h"
#include "PlayerControllerComponent.h"


void PlayerStateMachine::ChangeState(PlayerControllerComponent& player_, PLAYER_STATE nextStateType_)
{
    if (currentStateType == nextStateType_)
    {
        return;
    }

    auto it_ = states.find(nextStateType_);
    if (it_ == states.end())
    {
        return;
    }

    if (currentState)
    {
        currentState->Exit(player_);
    }

    currentState = it_->second.get();
    currentStateType = nextStateType_;

    currentState->Enter(player_);
}

void PlayerStateMachine::Update(PlayerControllerComponent& player_, float dt_)
{
    if (currentState)
        currentState->Update(player_, dt_);
}

const char* PlayerStateMachine::GetCurrentStateName() const
{
    return currentState ? currentState->GetName() : "None";
}