#pragma once

#include "PlayerState.h"

class PlayerControllerComponent;

// Guard、Skill、Counterなど、専用Stateを持たないTimeline駆動Actionを扱う汎用State。
class PlayerActionState : public PlayerState
{
public:
    PlayerActionState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_,
        PLAYER_ACTION action_);

    const char* GetName() const override { return GetPlayerActionName(action); }
    void Enter() override;
    void Execute(float elapsedTime_) override;

private:
    // このStateが開始するAction種別。
    PLAYER_ACTION action = PLAYER_ACTION::NONE;
};
