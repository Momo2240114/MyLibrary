#include "PlayerStateMachine.h"
#include "PlayerControllerComponent.h"

void PlayerState::Bind(PlayerControllerComponent& player_)
{
    playerController = &player_;
    owner = player_.GetGameObject();
}

void PlayerStateMachine::ChangeState(PlayerControllerComponent& player_, PLAYER_ACTION nextAction_)
{
    auto it_ = states.find(nextAction_);
    if (it_ == states.end())
    {
        return;
    }

    if (currentState)
    {
        currentState->Exit();
    }

    currentState = it_->second.get();
    currentAction = nextAction_;
    currentState->Bind(player_);
    currentState->Enter();
}

void PlayerStateMachine::Update(float dt_)
{
    if (currentState)
    {
        currentState->Execute(dt_);
    }
}

const char* PlayerStateMachine::GetCurrentStateName() const
{
    return currentState ? currentState->GetName() : "None";
}
