#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "PlayerStateMachine.h"

class ColliderComponent;
class BlackboardComponent;

struct PlayerActionRuntime
{
    bool canCancel = true;
    bool canBufferInput = false;
    bool canCombo = false;
    bool actionEnded = false;
    PLAYER_ACTION bufferedAction = PLAYER_ACTION::NONE;
    int bufferedComboIndex = 0;
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
    DirectX::XMFLOAT3 GetCameraForwardDirection() const;
    void SetHorizontalVelocity(const DirectX::XMFLOAT3& dir_, float speed_);

    ColliderComponent* GetCollider() const { return collider.get(); }
    PLAYER_ACTION GetCurrentAction() const { return stateMachine.GetCurrentAction(); }

    bool RequestAction(PLAYER_ACTION nextAction_);
    void NotifyGuardedAttack();
    void SetCanCancel(bool enabled_);
    void SetCanBufferInput(bool enabled_);
    void SetCanCombo(bool enabled_);
    void NotifyActionEnd();
    bool ConsumeBufferedAction();
    void EnterAction(PLAYER_ACTION action_);
    void ProcessActionInput();
    void SendMoveInputToBlackboard(const DirectX::XMFLOAT3& input_);

    float moveSpeed = 6.0f;
    float dodgeSpeed = 12.0f;

private:
    void EnsurePlayerInputBindings();
    int GetRequestedComboIndex(PLAYER_ACTION nextAction_) const;
    void RegisterPlayerAnimationActions();
    void SendActionToBlackboard(PLAYER_ACTION action_);

    std::shared_ptr<ColliderComponent> collider;
    std::shared_ptr<BlackboardComponent> blackboard;
    PlayerStateMachine stateMachine;
    PlayerActionRuntime actionRuntime;
    int comboIndex = 0;
    int pendingComboIndex = 0;


};
