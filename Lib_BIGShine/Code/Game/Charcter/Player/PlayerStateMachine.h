#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerStateMachine
{
public:
    template<class T, class... Args>
    void AddState(PLAYER_ACTION action_, Args&&... args_)
    {
        states[action_] = std::make_unique<T>(std::forward<Args>(args_)...);
    }

    void ChangeState(
        PlayerControllerComponent& player_,
        PLAYER_ACTION nextAction_);

    void Update(float dt_);

    const char* GetCurrentStateName() const;
    PLAYER_ACTION GetCurrentAction() const { return currentAction; }

private:
    std::unordered_map<PLAYER_ACTION, std::unique_ptr<PlayerState>> states;
    PlayerState* currentState = nullptr;
    PLAYER_ACTION currentAction = PLAYER_ACTION::NONE;
};
