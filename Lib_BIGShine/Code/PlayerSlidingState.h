#pragma once


#include"State.h"


#include"PlayerComponent.h"
#include"MovementComponent.h"
#include"EasingComponent.h"

#include"CapsuleCollider.h"
class PlayerSlidingState :public State
{
public:
	PlayerSlidingState(GameObject* gameObject) :State(gameObject) {}

	void Enter();

	void Execute(float elapsedTime);

	void LateExecute(float et);

	void Exit();

	virtual const char* GetName()const { return "PlayerSliding"; };

private:
	std::weak_ptr<PlayerComponent> m_pPlayerComponent;
	std::weak_ptr<MovementComponent> m_pMovementComponent;
	std::weak_ptr<EasingComponent> m_pEasingComponent;
	std::weak_ptr<CapsuleCollider> m_pCapsuleColliderComponent;

	Vector3 m_Direction = {0,0,0};
};