#pragma once

#include <unordered_map>
#include <memory>
#include "PlayerState.h"


class PlayerControllerComponent;

class PlayerStateMachine
{
public:
    template<class T>
    void AddState(PLAYER_STATE stateType_)
    {
        states[stateType_] = std::make_unique<T>();
    }

    void ChangeState(
        PlayerControllerComponent& player_,
        PLAYER_STATE nextStateType_);

    void Update(PlayerControllerComponent& player_, float dt_);

    const char* GetCurrentStateName() const;

private:
    std::unordered_map<PLAYER_STATE, std::unique_ptr<PlayerState>> states;
    PlayerState* currentState = nullptr;
    PLAYER_STATE currentStateType = PLAYER_STATE::NONE;
};