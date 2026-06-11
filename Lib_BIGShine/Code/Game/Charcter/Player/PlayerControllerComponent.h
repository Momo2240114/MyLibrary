#pragma once

#include "Engine/GamePlay/Core/Component/ComponentBase.h"
#include "PlayerStateMachine.h"

class ColliderComponent;

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

public:
    DirectX::XMFLOAT3 GetMoveInput() const;
    bool IsJumpPressed() const;
    bool IsGrounded() const;

    void SetHorizontalVelocity(const DirectX::XMFLOAT3& dir_, float speed_);
    void Jump();

    ColliderComponent* GetCollider() const { return collider.get(); }

    float moveSpeed = 6.0f;
    float jumpPower = 8.0f;

    template<class T>
    void ChangeState()
    {
        stateMachine.ChangeState(*this, std::make_unique<T>());
    }

private:
    std::shared_ptr<ColliderComponent> collider;
    PlayerStateMachine stateMachine;
};