#include "PlayerStateMachine.h"
#include "PlayerControllerComponent.h"

void PlayerStateMachine::Initialize(
    PlayerControllerComponent& playerController_)
{
    playerController = &playerController_;
    owner = playerController_.GetGameObject();
}

void PlayerStateMachine::ChangeState(PLAYER_ACTION nextAction_)
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
