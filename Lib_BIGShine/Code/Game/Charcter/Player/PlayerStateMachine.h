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
        if (!playerController)
        {
            return;
        }

        std::unique_ptr<T> state_ =
            std::make_unique<T>(
                owner.lock(),
                *playerController,
                std::forward<Args>(args_)...);
        states[action_] = std::move(state_);
    }

    template<class T>
    T* GetCurrentStateAs() const
    {
        return dynamic_cast<T*>(currentState);
    }

    void Initialize(PlayerControllerComponent& playerController_);
    void ChangeState(PLAYER_ACTION nextAction_);

    void Update(float dt_);

    const char* GetCurrentStateName() const;
    PLAYER_ACTION GetCurrentAction() const { return currentAction; }

private:
    std::unordered_map<PLAYER_ACTION, std::unique_ptr<PlayerState>> states;
    std::weak_ptr<GameObject> owner;
    PlayerControllerComponent* playerController = nullptr;
    PlayerState* currentState = nullptr;
    PLAYER_ACTION currentAction = PLAYER_ACTION::NONE;
};
