#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "PlayerStateMachine.h"
#include <algorithm>
#include <vector>

class ColliderComponent;
class BlackboardComponent;
class CharacterMovementComponent;
class HurtBox;

struct PlayerActionRuntime
{
    bool canCancel = true;
    bool canBufferInput = false;
    bool canCombo = false;
    bool actionEnded = false;
    PLAYER_ACTION bufferedAction = PLAYER_ACTION::NONE;
    int bufferedComboIndex = 0;
};

struct PlayerCancelTransition
{
    PLAYER_ACTION from = PLAYER_ACTION::WEAK_ATTACK;
    PLAYER_ACTION to = PLAYER_ACTION::DODGE;
};

class PlayerControllerComponent : public Component
{
public:
    const std::string& GetName() const override
    {
        static std::string name = "PlayerControllerComponent";
        return name;
    }

    void Start() override;
    void Update(float dt_) override;
    void Gui() override;

    void Serialize(nlohmann::json& j_) const override;
    void Deserialize(const nlohmann::json& j_) override;

    DirectX::XMFLOAT3 GetMoveInput() const;
    DirectX::XMFLOAT3 GetMoveInputAxis() const;
    DirectX::XMFLOAT3 GetCameraForwardDirection() const;
    void SetFacingDirection(const DirectX::XMFLOAT3& direction_);
    void SetHorizontalVelocity(const DirectX::XMFLOAT3& dir_, float speed_);

    ColliderComponent* GetCollider() const { return collider.get(); }
    CharacterMovementComponent* GetMovement() const { return movement.get(); }
    PLAYER_ACTION GetCurrentAction() const { return stateMachine.GetCurrentAction(); }
    int GetComboIndex() const { return comboIndex; }
    const std::vector<PlayerCancelTransition>& GetCancelTransitions() const { return cancelTransitions; }

    bool RequestAction(PLAYER_ACTION nextAction_);
    bool HasCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_) const;
    bool AddCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_);
    bool RemoveCancelTransition(PLAYER_ACTION from_, PLAYER_ACTION to_);
    void NotifyGuardedAttack();
    void SetCanCancel(bool enabled_);
    void SetCanBufferInput(bool enabled_);
    void SetCanCombo(bool enabled_);
    void NotifyComboTransition();
    void NotifyActionEnd();
    void HandleHitReaction();
    bool ConsumeBufferedAction();
    void EnterAction(PLAYER_ACTION action_);
    void ProcessActionInput();
    void SendMoveInputToBlackboard(const DirectX::XMFLOAT3& input_);

    float moveSpeed = 6.0f;

private:
    void EnsurePlayerInputBindings();
    int GetRequestedComboIndex(PLAYER_ACTION nextAction_) const;
    void RegisterPlayerAnimationActions();
    void SendActionToBlackboard(PLAYER_ACTION action_);
    void ApplyPendingMovement();

    bool ForceChangeAction(PLAYER_ACTION action_);

    std::shared_ptr<ColliderComponent> collider;
    std::shared_ptr<CharacterMovementComponent> movement;
    std::shared_ptr<HurtBox> hurtBox;
    std::shared_ptr<BlackboardComponent> blackboard;
    PlayerStateMachine stateMachine;
    PlayerActionRuntime actionRuntime;
    std::vector<PlayerCancelTransition> cancelTransitions
    {
        { PLAYER_ACTION::WEAK_ATTACK, PLAYER_ACTION::DODGE }
    };
    int comboIndex = 0;
    int pendingComboIndex = 0;
    DirectX::XMFLOAT3 pendingHorizontalVelocity{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 pendingFacingDirection{ 0.0f, 0.0f, 1.0f };
    bool hasPendingHorizontalVelocity = false;
    bool hasPendingFacingDirection = false;

    PLAYER_ACTION debugSelectedAction = PLAYER_ACTION::IDLE;
};
