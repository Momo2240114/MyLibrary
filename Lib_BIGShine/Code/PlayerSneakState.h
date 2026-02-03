#pragma once

#include"State.h"
#include"PlayerComponent.h"

#include"CapsuleCollider.h"

class PlayerSneakState :public State
{
public:
	PlayerSneakState(GameObject* gameObject) :State(gameObject) {}

	void Enter();

	void Execute(float et);

	void LateExecute(float et);

	void Exit();

	const char* GetName()const { return "PlayerSneak"; };
private:
	std::weak_ptr<PlayerComponent>m_pPlayerComponent;
	std::weak_ptr<EasingComponent> m_pEasingComponent;
	std::weak_ptr<MovementComponent>m_pMovementComponent;
	std::weak_ptr<CapsuleCollider> m_pCapsuleColliderComponent;
};