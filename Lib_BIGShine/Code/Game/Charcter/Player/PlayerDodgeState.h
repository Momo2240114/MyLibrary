#pragma once

#include <DirectXMath.h>
#include <cstdint>
#include <string>

#include "PlayerState.h"

class PlayerControllerComponent;

class PlayerDodgeState : public PlayerState
{
public:
    using PlayerState::PlayerState;

    const char* GetName() const override { return GetPlayerActionName(PLAYER_ACTION::DODGE); }
    void Enter() override;
    void Execute(float elapsedTime_) override;
    void Exit() override;

    void NotifyJustDodgeSucceeded();

private:
    void RegisterJustDodgeActions();
    void SendDodgeInputToBlackboard(const DirectX::XMFLOAT3& input_);

    void ActivateDodgeCollision();
    void DeactivateDodgeCollision();

    void StartJustDodgeFrame();
    void EndJustDodgeFrame();

    void HandleJustDodgeSuccess();
    void ClearJustDodgeBlackboard();
    void SetCounterAvailable(bool enabled_);
    void ExecuteOwnerAction(const std::string& actionName_);

private:
    DirectX::XMFLOAT3 dodgeDirection{ 0.0f, 0.0f, 1.0f };
    DirectX::XMFLOAT3 dodgeBlendInput{ 0.0f, 0.0f, 1.0f };

    bool justDodgeActionsRegistered = false;

    bool dodgeActive = false;
    bool justDodgeFrameActive = false;
    bool justDodgeSucceeded = false;
    bool counterAvailable = false;

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
