#pragma once

#include "PlayerState.h"

class CameraComponent;
class CameraLockOnComponent;
class GameObject;

class PlayerAttackState : public PlayerState
{
public:
    PlayerAttackState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_,
        PLAYER_ACTION action_);

    const char* GetName() const override { return GetPlayerActionName(action); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

private:
    std::shared_ptr<GameObject> FindAssistTarget();
    void ApplyAttackAssist(float elapsedTime_);
    void ApplyAttackFacingAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    void ApplyAttackPositionAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();

    PLAYER_ACTION action = PLAYER_ACTION::NONE;
    std::weak_ptr<CameraLockOnComponent> cameraLockOnComponent;
};
