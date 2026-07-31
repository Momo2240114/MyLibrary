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
    void ResolveAttackAssistTarget();
    void ApplyAttackAssist(float elapsedTime_);
    void ApplyAttackFacingAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    void ApplyAttackPositionAssist(const std::shared_ptr<GameObject>& target_, float elapsedTime_);
    std::shared_ptr<CameraLockOnComponent> ResolveCameraLockOnComponent();
    float GetAttackNormalizedTime();

    PLAYER_ACTION action = PLAYER_ACTION::NONE;
    std::weak_ptr<CameraLockOnComponent> cameraLockOnComponent;
    std::weak_ptr<GameObject> attackAssistTarget;
    DirectX::XMFLOAT3 attackStartForward{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 lockedAssistDirection{ 0.0f, 0.0f, 1.0f };
    float accumulatedFacingCorrectionDegrees = 0.0f;
    float attackElapsedTime = 0.0f;
    bool hasLockedAssistDirection = false;
    bool attackAssistTargetResolved = false;
};
