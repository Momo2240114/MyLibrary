#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerDodgeState : public PlayerState
{
public:
    PlayerDodgeState(
        const std::shared_ptr<GameObject>& owner_,
        PlayerControllerComponent& playerController_,
        PLAYER_ACTION action_ = PLAYER_ACTION::DODGE);

    const char* GetName() const override { return GetPlayerActionName(action); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

    void NotifyJustDodgeSucceeded();

private:
    void RegisterJustDodgeActions();
    void SendDodgeInputToBlackboard(const DirectX::XMFLOAT3& input_);

    void ActivateDodgeCollision();
    void DeactivateDodgeCollision();
    void SuspendAirDodgeVerticalMovement();
    void RestoreAirDodgeVerticalMovement();

    void StartJustDodgeFrame();
    void EndJustDodgeFrame();

    void HandleJustDodgeSuccess();
    void ClearJustDodgeBlackboard();
    void SetCounterAvailable(bool enabled_);
    void ExecuteOwnerAction(const std::string& actionName_);

private:
    PLAYER_ACTION action = PLAYER_ACTION::DODGE;
    DirectX::XMFLOAT3 dodgeDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 dodgeBlendInput{ 0.0f, 0.0f, 1.0f };

    bool justDodgeActionsRegistered = false;

    bool dodgeActive = false;
    bool justDodgeFrameActive = false;
    bool justDodgeSucceeded = false;
    bool counterAvailable = false;
    bool airDodgeGravityOverridden = false;
    bool previousAirDodgeUseGravity = false;

    float justDodgeFrameDuration = 0.18f;
    float justDodgeFrameTimer = 0.0f;

    std::string justDodgeColliderActivateActionName =
        "JustDodge_EnableJustDodgeCollider";
    std::string justDodgeColliderDeactivateActionName =
        "JustDodge_DisableJustDodgeCollider";

    std::string frameStartActionName = "JustDodge_FrameStart";
    std::string frameEndActionName = "JustDodge_FrameEnd";
    std::string consumeCounterActionName = "JustDodge_ConsumeCounter";
};
